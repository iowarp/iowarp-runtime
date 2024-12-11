/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <queue>
#include <thread>

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/chimaera_types.h"
#include "chimaera/module_registry/module_registry.h"
#include "chimaera/network/rpc_thallium.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/work_orchestrator/affinity.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"

namespace chi {

/**===============================================================
 * Private Task Multi Queue
 * =============================================================== */
bool PrivateTaskMultiQueue::push(const LPointer<Task> &task) {
  // Determine the domain of the task
  std::vector<ResolvedDomainQuery> resolved =
      CHI_RPC->ResolveDomainQuery(task->pool_, task->dom_query_, false);
  DomainQuery res_query = resolved[0].dom_;
  if (resolved.size() == 1 && resolved[0].node_ == CHI_RPC->node_id_ &&
      res_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
    // CASE 0: The task is a flushing task. Place in the flush queue.
    if (task->IsFlush()) {
      return !GetFlush().push(task).IsNull();
    } 
    // CASE 1: The task is local to this machine, just find the lane.
    // Determine the lane the task should map to within container
    ContainerId container_id = res_query.sel_.id_;
    Container *exec =
        CHI_MOD_REGISTRY->GetContainer(task->pool_, container_id);
    if (!exec || !exec->is_created_) {
      // If the container doesn't exist, it's probably going to get created.
      // Put in the failed queue.
      return !GetFail().push(task).IsNull();
    }
    // Find the lane
    chi::Lane *chi_lane = exec->Route(task.ptr_);
    task->rctx_.route_container_ = container_id;
    task->rctx_.route_lane_ = chi_lane->lane_id_;
    chi_lane->push(task);
  } else {
    // CASE 2: The task is remote to this machine, put in the remote queue.
    CHI_REMOTE_QUEUE->AsyncClientPushSubmitBase(
        HSHM_DEFAULT_MEM_CTX, nullptr, task->task_node_ + 1,
        DomainQuery::GetDirectId(SubDomainId::kGlobalContainers, 1), task.ptr_);
  }
  return true;
}

/**===============================================================
 * Lanes
 * =============================================================== */
/** Push a task  */
hshm::qtok_t Lane::push(const LPointer<Task> &task) {
  hshm::qtok_t ret = active_tasks_.push(task);
  size_t dup = count_.fetch_add(1);
  if (dup == 0) {
    Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(worker_id_);
    worker.RequestLane(this);
  }
  return ret;
}

/** Pop a set of tasks in sequence */
size_t Lane::pop_prep(size_t count) {
  return count_.fetch_sub(count) - count;
}

/** Pop a set of tasks in sequence */
size_t Lane::pop_unprep(size_t count) {
  return count_.fetch_add(count) + count;
}

/** Pop a task */
hshm::qtok_t Lane::pop(LPointer<Task> &task) {
  return active_tasks_.pop(task);
}

/**===============================================================
 * Initialize Worker
 * =============================================================== */

/** Constructor */
Worker::Worker(u32 id, int cpu_id, ABT_xstream &xstream) {
  id_ = id;
  sleep_us_ = 0;
  pid_ = 0;
  affinity_ = cpu_id;
  stacks_.Resize(num_stacks_);
  for (int i = 0; i < 16; ++i) {
    stacks_.emplace(malloc(stack_size_));
  }

  // MAX_DEPTH * [LOW_LAT, LONG_LAT]
  config::QueueManagerInfo &qm = CHI_QM_RUNTIME->config_->queue_manager_;
  active_.Init(id_, qm.proc_queue_depth_, qm.queue_depth_,
               qm.max_containers_pn_);

  // Monitoring phase
  monitor_gap_ = CHI_WORK_ORCHESTRATOR->monitor_gap_;
  monitor_window_ = CHI_WORK_ORCHESTRATOR->monitor_window_;

  // Set xstream
  xstream_ = xstream;
}

/** Spawn worker thread */
void Worker::Spawn() {
  tl_thread_ = CHI_WORK_ORCHESTRATOR->SpawnAsyncThread(
      xstream_, &Worker::WorkerEntryPoint, this);
}

/**===============================================================
 * Run tasks
 * =============================================================== */

/** Worker entrypoint */
void Worker::WorkerEntryPoint(void *arg) {
  Worker *worker = (Worker *)arg;
  worker->Loop();
}

/**
 * Begin flushing all worker tasks
 * NOTE: The first worker holds all FlushTasks and will signal other workers
 * in the first iteration.
 * */
void Worker::BeginFlush(WorkOrchestrator *orch) {
  if (flush_.flush_iter_ == 0 && active_.GetFlush().size()) {
    for (std::unique_ptr<Worker> &worker : orch->workers_) {
      worker->flush_.flushing_ = true;
    }
  }
  ++flush_.flush_iter_;
}

/** Check if work has been done */
void Worker::EndFlush(WorkOrchestrator *orch) {
  // Barrier for all workers to complete
  flush_.flushing_ = false;
  while (AnyFlushing(orch)) {
    HERMES_THREAD_MODEL->Yield();
  }
  // On the root worker, detect if any work was done
  if (active_.GetFlush().size()) {
    if (AnyFlushWorkDone(orch)) {
      // Ensure that workers are relabeled as flushing
      flush_.flushing_ = true;
    } else {
      // Reap all FlushTasks and end recurion
      PollTempQueue(active_.GetFlush(), false);
    }
  }
}

/** Check if any worker is still flushing */
bool Worker::AnyFlushing(WorkOrchestrator *orch) {
  for (std::unique_ptr<Worker> &worker : orch->workers_) {
    if (worker->flush_.flushing_) {
      return true;
    }
  }
  return false;
}

/** Check if any worker did work */
bool Worker::AnyFlushWorkDone(WorkOrchestrator *orch) {
  bool ret = false;
  for (std::unique_ptr<Worker> &worker : orch->workers_) {
    if (worker->flush_.count_ != worker->flush_.work_done_) {
      worker->flush_.work_done_ = worker->flush_.count_;
      ret = true;
    }
  }
  return ret;
}

/** Worker loop iteration */
void Worker::Loop() {
  CHI_WORK_ORCHESTRATOR->SetThreadLocalBlock(this);
  pid_ = GetLinuxTid();
  SetCpuAffinity(affinity_);
  if (IsContinuousPolling()) {
    MakeDedicated();
  }
  HILOG(kDebug, "Entered worker {}", id_);
  WorkOrchestrator *orch = CHI_WORK_ORCHESTRATOR;
  cur_time_.Refresh();
  while (orch->IsAlive()) {
    try {
      load_nsec_ = 0;
      bool flushing = flush_.flushing_ || active_.GetFlush().size();
      if (flushing) {
        BeginFlush(orch);
      }
      Run(flushing);
      if (flushing) {
        EndFlush(orch);
      }
      cur_time_.Refresh();
      iter_count_ += 1;
      if (load_nsec_ == 0) {
        // HERMES_THREAD_MODEL->SleepForUs(200);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
  }
  HILOG(kInfo, "(node {}) Worker {} wrapping up", CHI_CLIENT->node_id_, id_);
  Run(true);
  HILOG(kInfo, "(node {}) Worker {} has exited", CHI_CLIENT->node_id_, id_);
}

/** Run a single iteration over all queues */
void Worker::Run(bool flushing) {
  // Process tasks in the pending queues
  IngestProcLanes(flushing);
  for (size_t i = 0; i < 8192; ++i) {
    PollPrivateLaneMultiQueue(active_.active_lanes_.GetLowLatency(),
                         flushing);
  }
  PollPrivateLaneMultiQueue(active_.active_lanes_.GetHighLatency(), flushing);
  PollTempQueue(active_.GetFail(), flushing);
}

/** Ingest all process lanes */
HSHM_INLINE
void Worker::IngestProcLanes(bool flushing) {
  for (IngressEntry &work_entry : work_proc_queue_) {
    IngestLane(work_entry);
  }
}

/** Ingest a lane */
HSHM_INLINE
void Worker::IngestLane(IngressEntry &lane_info) {
  // Ingest tasks from the ingress queues
  ingress::Lane *&ig_lane = lane_info.lane_;
  ingress::LaneData entry;
  while (true) {
    if (ig_lane->pop(entry).IsNull()) {
      break;
    }
    LPointer<Task> task;
    task.shm_ = entry.shm_;
    task.ptr_ = CHI_CLIENT->GetMainPointer<Task>(entry.shm_);
    active_.push(task);
  }
}

/** Poll the set of tasks in the private queue */
HSHM_INLINE
void Worker::PollTempQueue(PrivateTaskQueue &priv_queue, bool flushing) {
  size_t size = priv_queue.size();
  for (size_t i = 0; i < size; ++i) {
    LPointer<Task> task;
    if (priv_queue.pop(task).IsNull()) {
      break;
    }
    if (task.ptr_ == nullptr) {
      break;
    }
    if (task->IsFlush()) {
      task->UnsetFlush();
    }
    active_.push(task);
  }
}

/** Poll the set of tasks in the private queue */
HSHM_INLINE
size_t Worker::PollPrivateLaneMultiQueue(PrivateLaneQueue &lanes, bool flushing) {
  size_t work = 0;
  size_t num_lanes = lanes.size();
  for (size_t lane_off = 0; lane_off < num_lanes; ++lane_off) {
    chi::Lane *chi_lane;
    if (lanes.pop(chi_lane).IsNull()) {
      break;
    }
    size_t max_lane_size = chi_lane->size();
    size_t lane_size = 0;
    for (; lane_size < max_lane_size; ++lane_size) {
      LPointer<Task> task;
      if (chi_lane->pop(task).IsNull()) { 
        break;
      }
      if (task.ptr_ == nullptr) {
        break;
      }
      bool pushback = RunTask(task, flushing);
      if (pushback) {
        chi_lane->push(task);
      }
      ++work;
    }
    size_t after_size = chi_lane->pop_prep(lane_size);
    if (after_size > 0) {
      lanes.push(chi_lane);
    }
  }
  return work;
}

/** Run a task */
bool Worker::RunTask(LPointer<Task> &task, bool flushing) {
  // Get task properties
  bitfield32_t props = GetTaskProperties(task.ptr_, flushing);
  // Pack runtime context
  RunContext &rctx = task->rctx_;
  rctx.worker_props_ = props;
  rctx.worker_id_ = id_;
  rctx.flush_ = &flush_;
  // Get the task container
  Container *exec;
  if (task->IsModuleComplete()) {
    exec = CHI_MOD_REGISTRY->GetStaticContainer(task->pool_);
  } else {
    exec = CHI_MOD_REGISTRY->GetContainer(task->pool_, rctx.route_container_);
  }
  if (!exec) {
    if (task->pool_ == PoolId::GetNull()) {
      HELOG(kFatal, "(node {}) Task {} pool does not exist",
            CHI_CLIENT->node_id_, task->task_node_);
      task->SetModuleComplete();
      return false;
    }
    return true;
  }
  // Run the task
  if (!task->IsModuleComplete()) {
    // Make this task current
    cur_task_ = task.ptr_;
    cur_lane_ = exec->GetLane(task->rctx_.route_lane_);
    // Execute the task based on its properties
    rctx.exec_ = exec;
    ExecTask(task, rctx, exec, props);
  }
  // Cleanup allocations
  bool pushback = true;
  if (task->IsModuleComplete()) {
    pushback = false;
    EndTask(exec, task);
  } else if (task->IsBlocked()) {
    pushback = false;
  }
  return pushback;
}

/** Run an arbitrary task */
HSHM_INLINE
void Worker::ExecTask(LPointer<Task> &task,
                      RunContext &rctx, Container *&exec,
                      bitfield32_t &props) {
  // Determine if a task should be executed
  if (!props.All(CHI_WORKER_SHOULD_RUN)) {
    return;
  }
  // Flush tasks
  if (props.Any(CHI_WORKER_IS_FLUSHING)) {
    if (!task->IsFlush() && !task->IsLongRunning()) {
      flush_.count_ += 1;
    }
  }
  // Activate task
  if (!task->IsStarted()) {
    cur_lane_->SetActive(task->task_node_.root_);
    if (ShouldSample()) {
      task->SetShouldSample();
      rctx.timer_.Reset();
    }
  }
  // Execute + monitor the task
  if (task->ShouldSample()) {
    rctx.timer_.Resume();
    ExecCoroutine(task.ptr_, rctx);
    rctx.timer_.Pause();
  } else {
    ExecCoroutine(task.ptr_, rctx);
  }
  load_nsec_ += rctx.load_.cpu_load_ + 1;
  // Deactivate task and monitor
  if (!task->IsStarted()) {
    if (task->ShouldSample()) {
      exec->Monitor(MonitorMode::kSampleLoad, task->method_, task.ptr_, rctx);
      task->UnsetShouldSample();
    }
    cur_lane_->UnsetActive(task->task_node_.root_);
    // Update the load
    cur_lane_->load_ -= rctx.load_;
    cur_lane_->num_tasks_ -= 1;
    cur_time_.Tick(rctx.load_.cpu_load_);
  }
  task->DidRun(cur_time_);
  // Block the task
  if (task->IsBlocked()) {
    CHI_WORK_ORCHESTRATOR->Block(task.ptr_);
  }
}

/** Run a task */
HSHM_INLINE
void Worker::ExecCoroutine(Task *&task, RunContext &rctx) {
  // If task isn't started, allocate stack pointer
  if (!task->IsStarted()) {
    rctx.stack_ptr_ = AllocateStack();
    if (rctx.stack_ptr_ == nullptr) {
      HELOG(kFatal, "The stack pointer of size {} is NULL", stack_size_);
    }
    rctx.jmp_.fctx = bctx::make_fcontext((char *)rctx.stack_ptr_ + stack_size_,
                                         stack_size_, &Worker::CoroutineEntry);
    task->SetStarted();
  }
  // Jump to CoroutineEntry
  rctx.jmp_ = bctx::jump_fcontext(rctx.jmp_.fctx, task);
  if (!task->IsStarted()) {
    FreeStack(rctx.stack_ptr_);
  }
}

/** Run a coroutine */
void Worker::CoroutineEntry(bctx::transfer_t t) {
  Task *task = reinterpret_cast<Task *>(t.data);
  RunContext &rctx = task->rctx_;
  Container *&exec = rctx.exec_;
  rctx.jmp_ = t;
  exec->Run(task->method_, task, rctx);
  task->UnsetStarted();
  task->Yield();
}

/** Free a task when it is no longer needed */
HSHM_INLINE
void Worker::EndTask(Container *exec, LPointer<Task> &task) {
  task->SetComplete();
  if (task->ShouldSignalUnblock()) {
    CHI_WORK_ORCHESTRATOR->SignalUnblock(task->rctx_.pending_to_);
  }
  if (task->ShouldSignalRemoteComplete()) {
    Container *remote_exec =
        CHI_MOD_REGISTRY->GetContainer(CHI_REMOTE_QUEUE->id_, 1);
    remote_exec->Run(chi::remote_queue::Method::kServerPushComplete, task.ptr_,
                     task->rctx_);
    return;
  }
  if (exec && task->IsFireAndForget()) {
    CHI_CLIENT->DelTask(HSHM_DEFAULT_MEM_CTX, exec, task.ptr_);
  }
}

/**===============================================================
 * Helpers
 * =============================================================== */

/** Migrate a lane from this worker to another */
void Worker::MigrateLane(Lane *lane, u32 new_worker) {
  // Blocked + ingressed ops need to be located and removed from queues
}

/** Get the characteristics of a task */
HSHM_INLINE
bitfield32_t Worker::GetTaskProperties(Task *&task, bool flushing) {
  bitfield32_t props;

  bool should_run = task->ShouldRun(cur_time_, flushing);
  if (task->IsRemote()) {
    props.SetBits(CHI_WORKER_IS_REMOTE);
  }
  if (should_run || task->IsStarted()) {
    props.SetBits(CHI_WORKER_SHOULD_RUN);
  }
  if (flushing) {
    props.SetBits(CHI_WORKER_IS_FLUSHING);
  }
  return props;
}

/** Join worker */
void Worker::Join() {
  // thread_->join();
  ABT_xstream_join(xstream_);
}

/** Set the sleep cycle */
void Worker::SetPollingFrequency(size_t sleep_us) {
  sleep_us_ = sleep_us;
  flags_.UnsetBits(WORKER_CONTINUOUS_POLLING);
}

/** Enable continuous polling */
void Worker::EnableContinuousPolling() {
  flags_.SetBits(WORKER_CONTINUOUS_POLLING);
}

/** Disable continuous polling */
void Worker::DisableContinuousPolling() {
  flags_.UnsetBits(WORKER_CONTINUOUS_POLLING);
}

/** Check if continuously polling */
bool Worker::IsContinuousPolling() {
  return flags_.Any(WORKER_CONTINUOUS_POLLING);
}

/** Check if continuously polling */
void Worker::SetHighLatency() { flags_.SetBits(WORKER_HIGH_LATENCY); }

/** Check if continuously polling */
bool Worker::IsHighLatency() { return flags_.Any(WORKER_HIGH_LATENCY); }

/** Check if continuously polling */
void Worker::SetLowLatency() { flags_.SetBits(WORKER_LOW_LATENCY); }

/** Check if continuously polling */
bool Worker::IsLowLatency() { return flags_.Any(WORKER_LOW_LATENCY); }

/** Set the CPU affinity of this worker */
void Worker::SetCpuAffinity(int cpu_id) {
  affinity_ = cpu_id;
  ABT_xstream_set_affinity(xstream_, 1, &cpu_id);
}

/** Make maximum priority process */
void Worker::MakeDedicated() {
  int policy = SCHED_FIFO;
  struct sched_param param = {.sched_priority = 1};
  sched_setscheduler(0, policy, &param);
}

/** Allocate a stack for a task */
void* Worker::AllocateStack() {
  void *stack;
  if (!stacks_.pop(stack).IsNull()) {
    return stack;
  }
  return malloc(stack_size_);
}

/** Free a stack */
void Worker::FreeStack(void *stack) {
  if (!stacks_.emplace(stack).IsNull()) {
    return;
  }
  stacks_.Resize(stacks_.size() * 2 + num_stacks_);
  stacks_.emplace(stack);
}

}  // namespace chi
