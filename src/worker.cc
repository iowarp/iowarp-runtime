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
bool PrivateTaskMultiQueue::push(const PrivateTaskQueueEntry &entry) {
  Task *task = entry.task_.ptr_;
  if (task->pool_ == CHI_ADMIN->id_ &&
      task->method_ == chi::Admin::Method::kCreateContainer) {
    return GetConstruct().push(entry);
  } else if (task->IsFlush()) {
    return GetFlush().push(entry);
  } else if (task->IsLongRunning()) {
    return GetLongRunning().push(entry);
  } else if (task->prio_ == TaskPrio::kLowLatency) {
    return GetLowLat().push(entry);
  } else {
    return GetHighLat().push(entry);
  }
}

/**===============================================================
 * Initialize Worker
 * =============================================================== */

/** Constructor */
Worker::Worker(u32 id, int cpu_id, ABT_xstream &xstream) {
  poll_queues_.Resize(1024);
  relinquish_queues_.Resize(1024);
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

  // Spawn threads
  xstream_ = xstream;
  // thread_ = std::make_unique<std::thread>(&Worker::Loop, this);
  // pthread_id_ = thread_->native_handle();
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
      PollPrivateQueue(active_.GetFlush(), false);
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
  HILOG(kDebug, "Entered worker {}", id_);
  CHI_WORK_ORCHESTRATOR->SetThreadLocalBlock(this);
  pid_ = GetLinuxTid();
  SetCpuAffinity(affinity_);
  if (IsContinuousPolling()) {
    MakeDedicated();
  }
  WorkOrchestrator *orch = CHI_WORK_ORCHESTRATOR;
  cur_time_.Refresh();
  while (orch->IsAlive()) {
    try {
      load_nsec_ = 0;
      bool flushing = flush_.flushing_ || active_.GetFlush().size_;
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
  // Are there any queues pending scheduling
  if (poll_queues_.size() > 0) {
    _PollQueues();
  }
  // Are there any queues pending descheduling
  if (relinquish_queues_.size() > 0) {
    _RelinquishQueues();
  }
  // Process tasks in the pending queues
  IngestProcLanes(flushing);
  for (size_t i = 0; i < 8192; ++i) {
    IngestInterLanes(flushing);
    PollPrivateQueue(active_.GetConstruct(), flushing);
    size_t lowlat = active_.GetLowLat().size_;
    hshm::Timer t;
    if (lowlat) {
      t.Resume();
    }
    PollPrivateQueue(active_.GetLowLat(), flushing);
    if (lowlat) {
      t.Pause();
      HILOG(kInfo, "Worker {}: Low latency took {} msec for {} tasks ({} KOps)", 
      id_, t.GetMsec(), lowlat, lowlat / t.GetMsec());
    }
  }
  PollPrivateQueue(active_.GetHighLat(), flushing);
  PollPrivateQueue(active_.GetLongRunning(), flushing);
}

/** Ingest all process lanes */
HSHM_ALWAYS_INLINE
void Worker::IngestProcLanes(bool flushing) {
  for (WorkEntry &work_entry : work_proc_queue_) {
    IngestLane(work_entry);
  }
}

/** Ingest all intermediate lanes */
HSHM_ALWAYS_INLINE
void Worker::IngestInterLanes(bool flushing) {
  for (WorkEntry &work_entry : work_inter_queue_) {
    IngestLane(work_entry);
  }
}

/** Ingest a lane */
HSHM_ALWAYS_INLINE
void Worker::IngestLane(WorkEntry &lane_info) {
  // Ingest tasks from the ingress queues
  ingress::Lane *&ig_lane = lane_info.lane_;
  ingress::LaneData *entry;
  while (true) {
    if (ig_lane->peek(entry).IsNull()) {
      break;
    }
    LPointer<Task> task;
    task.shm_ = entry->p_;
    task.ptr_ = CHI_CLIENT->GetMainPointer<Task>(entry->p_);
    DomainQuery dom_query = task->dom_query_;
    TaskRouteMode route = Reroute(task->pool_, dom_query, task, ig_lane);
    if (route == TaskRouteMode::kLocalWorker) {
      ig_lane->pop();
    } else {
      if (active_.push(PrivateTaskQueueEntry{task, dom_query})) {
        //          HILOG(kInfo, "[TASK_CHECK] Running rep_task {} on node {}
        //          (long running: {})"
        //                       " pool={} method={}",
        //                (void*)task->rctx_.ret_task_addr_, CHI_RPC->node_id_,
        //                task->IsLongRunning(), task->pool_, task->method_);
        ig_lane->pop();
      } else {
        break;
      }
    }
  }
}

/**
 * Detect if a DomainQuery is across nodes
 * */
TaskRouteMode Worker::Reroute(const PoolId &scope, DomainQuery &dom_query,
                              LPointer<Task> task, ingress::Lane *ig_lane) {
#ifdef CHIMAERA_REMOTE_DEBUG
  if (task->pool_ != CHI_QM_CLIENT->admin_pool_id_ &&
      !task->task_flags_.Any(TASK_REMOTE_DEBUG_MARK) &&
      !task->IsLongRunning() && task->method_ != TaskMethod::kCreate &&
      CHI_RUNTIME->remote_created_ && !task->IsRemote()) {
    task->SetRemote();
  }
#endif
  if (task->IsRemote()) {
    return TaskRouteMode::kRemoteWorker;
  } else if (!task->IsRouted()) {
    CHI_CLIENT->ScheduleTaskRuntime(nullptr, task, ig_lane->id_);
    return TaskRouteMode::kLocalWorker;
  } else {
    dom_query = DomainQuery::GetDirectId(dom_query.sub_id_,
                                         task->rctx_.route_container_);
    Container *exec =
        CHI_MOD_REGISTRY->GetContainer(task->pool_, dom_query.sel_.id_);
    if (!exec || !exec->is_created_) {
      // NOTE(llogan): exec may be null if there is an update happening
      // For now, simply push back into the queue. This may technically
      // break strong consistency since tasks will be handled out-of order.
      // TODO(llogan): Should add another queue to maintain consistency.
      ig_lane->emplace(task.shm_);
      return TaskRouteMode::kLocalWorker;
    }
    chi::Lane *chi_lane = exec->GetLane(task->rctx_.route_lane_);
    if (chi_lane->ingress_id_ == ig_lane->id_) {
      // NOTE(llogan): May become incorrect if active push fails
      // Update the load
      exec->Monitor(MonitorMode::kEstLoad, task->method_, task.ptr_,
                    task->rctx_);
      chi_lane->load_ += task->rctx_.load_;
      chi_lane->num_tasks_ += 1;
      return TaskRouteMode::kThisWorker;
    } else {
      ingress::MultiQueue *queue =
          CHI_CLIENT->GetQueue(CHI_QM_RUNTIME->admin_queue_id_);
      ingress::LaneGroup &ig_lane_group =
          queue->GetGroup(chi_lane->ingress_id_.prio_);
      ingress::Lane &new_ig_lane =
          ig_lane_group.GetLane(chi_lane->ingress_id_.unique_);
      new_ig_lane.emplace(task.shm_);
      return TaskRouteMode::kLocalWorker;
    }
  }
}

/** Process completion events */
void Worker::ProcessCompletions() { while (active_.unblock()); }

/** Poll the set of tasks in the private queue */
HSHM_ALWAYS_INLINE
size_t Worker::PollPrivateQueue(PrivateTaskQueue &priv_queue, bool flushing) {
  size_t work = 0;
  size_t size = priv_queue.size_;
  for (size_t i = 0; i < size; ++i) {
    PrivateTaskQueueEntry entry;
    priv_queue.pop(entry);
    if (entry.task_.ptr_ == nullptr) {
      break;
    }
    bool pushback = RunTask(priv_queue, entry, entry.task_, flushing);
    if (pushback) {
      priv_queue.push(entry);
    }
    ++work;
  }
  ProcessCompletions();
  return work;
}

/** Run a task */
bool Worker::RunTask(PrivateTaskQueue &priv_queue, PrivateTaskQueueEntry &entry,
                     LPointer<Task> task, bool flushing) {
  // Get task properties
  bitfield32_t props = GetTaskProperties(task.ptr_, flushing);
  // Pack runtime context
  RunContext &rctx = task->rctx_;
  rctx.worker_props_ = props;
  rctx.worker_id_ = id_;
  rctx.flush_ = &flush_;
  // Get the task container
  Container *exec;
  if (!props.Any(CHI_WORKER_IS_REMOTE)) {
    exec =
        CHI_MOD_REGISTRY->GetContainer(task->pool_, entry.res_query_.sel_.id_);
  } else if (!task->IsModuleComplete()) {
    task->SetBlocked(1);
    active_.block(entry);
    cur_task_ = nullptr;
    cur_lane_ = nullptr;
    CHI_REMOTE_QUEUE->AsyncClientPushSubmitBase(
        HSHM_DEFAULT_MEM_CTX, nullptr, task->task_node_ + 1,
        DomainQuery::GetDirectId(SubDomainId::kGlobalContainers, 1), task.ptr_);
    return false;
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
  if (!task->IsModuleComplete()) {
    // Make this task current
    cur_task_ = task.ptr_;
    cur_lane_ = exec->GetLane(task->rctx_.route_lane_);
    // Check if the task is apart of a plugged lane
    if (cur_lane_->IsPlugged()) {
      if (cur_lane_->worker_id_ != id_) {
        ingress::MultiQueue *queue =
            CHI_CLIENT->GetQueue(CHI_QM_RUNTIME->admin_queue_id_);
        ingress::LaneGroup &ig_lane_group =
            queue->GetGroup(cur_lane_->ingress_id_.node_id_);
        ingress::Lane &new_ig_lane =
            ig_lane_group.GetLane(cur_lane_->ingress_id_.unique_);
        new_ig_lane.emplace(task.shm_);
        return false;
      }
      if (!cur_lane_->IsActive(task->task_node_.root_)) {
        // TODO(llogan): insert into plug list.
        return true;
      }
    }
    // Execute the task based on its properties
    rctx.exec_ = exec;
    ExecTask(priv_queue, entry, task.ptr_, rctx, exec, props);
  }
  // Cleanup allocations
  bool pushback = true;
  if (task->IsModuleComplete()) {
    pushback = false;
    // active_.erase(queue.id_);
    active_.erase(priv_queue.id_, entry);
    EndTask(exec, task);
  } else if (task->IsBlocked()) {
    pushback = false;
  }
  return pushback;
}

/** Run an arbitrary task */
HSHM_ALWAYS_INLINE
void Worker::ExecTask(PrivateTaskQueue &priv_queue,
                      PrivateTaskQueueEntry &entry, Task *&task,
                      RunContext &rctx, Container *&exec, bitfield32_t &props) {
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
    ExecCoroutine(task, rctx);
    rctx.timer_.Pause();
  } else {
    ExecCoroutine(task, rctx);
  }
  load_nsec_ += rctx.load_.cpu_load_ + 1;
  // Deactivate task and monitor
  if (!task->IsStarted()) {
    if (task->ShouldSample()) {
      exec->Monitor(MonitorMode::kSampleLoad, task->method_, task, rctx);
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
    active_.block(entry);
  }
}

/** Run a task */
HSHM_ALWAYS_INLINE
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
HSHM_ALWAYS_INLINE
void Worker::EndTask(Container *exec, LPointer<Task> &task) {
  //    if (task->task_flags_.Any(TASK_REMOTE_RECV_MARK)) {
  //      HILOG(kInfo, "[TASK_CHECK] Server completed rep_task {} on node {}",
  //            (void*)task->rctx_.ret_task_addr_, CHI_RPC->node_id_);
  //    }
  task->SetComplete();
  if (task->ShouldSignalUnblock()) {
    SignalUnblock(task->rctx_.pending_to_);
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

/** Unblock a task */
void Worker::SignalUnblock(Task *unblock_task) {
  LPointer<Task> ltask;
  ltask.ptr_ = unblock_task;
  ltask.shm_ = HERMES_MEMORY_MANAGER->Convert(ltask.ptr_);
  SignalUnblock(ltask);
}

/** Externally signal a task as complete */
HSHM_ALWAYS_INLINE
void Worker::SignalUnblock(LPointer<Task> &unblock_task) {
  Worker &worker =
      CHI_WORK_ORCHESTRATOR->GetWorker(unblock_task->rctx_.worker_id_);
  PrivateTaskMultiQueue &pending = worker.GetPendingQueue(unblock_task.ptr_);
  pending.signal_unblock(pending, unblock_task);
}

/** Get the characteristics of a task */
HSHM_ALWAYS_INLINE
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

/** Get the pending queue for a worker */
PrivateTaskMultiQueue &Worker::GetPendingQueue(Task *task) {
  PrivateTaskMultiQueue &pending =
      CHI_WORK_ORCHESTRATOR->workers_[task->rctx_.worker_id_]->active_;
  return pending;
}

/** Tell worker to poll a set of queues */
void Worker::PollQueues(const std::vector<WorkEntry> &queues) {
  poll_queues_.emplace(queues);
}

/** Actually poll the queues from within the worker */
void Worker::_PollQueues() {
  std::vector<WorkEntry> work_queue;
  while (!poll_queues_.pop(work_queue).IsNull()) {
    for (const WorkEntry &entry : work_queue) {
      if (entry.queue_->id_ == CHI_QM_RUNTIME->process_queue_id_) {
        work_proc_queue_.emplace_back(entry);
      } else {
        work_inter_queue_.emplace_back(entry);
      }
    }
  }
}

/**
 * Tell worker to start relinquishing some of its queues
 * This function must be called from a single thread (outside of worker)
 * */
void Worker::RelinquishingQueues(const std::vector<WorkEntry> &queues) {
  relinquish_queues_.emplace(queues);
}

/** Actually relinquish the queues from within the worker */
void Worker::_RelinquishQueues() {}

/** Check if worker is still stealing queues */
bool Worker::IsRelinquishingQueues() { return relinquish_queues_.size() > 0; }

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
