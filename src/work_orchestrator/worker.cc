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

#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/logging.h>
#include <hermes_shm/util/timer.h>

#include <queue>
#include <thread>

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/chimaera_types.h"
#include "chimaera/module_registry/module_registry.h"
#include "chimaera/network/rpc_thallium.h"
#include "chimaera/queue_manager/queue_manager.h"
#include "chimaera/work_orchestrator/comutex.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"

namespace chi {

/**===============================================================
 * Private Task Multi Queue
 * =============================================================== */
// Schedule the task to another node or to a local lane
bool PrivateTaskMultiQueue::push(const FullPtr<Task> &task) {
#ifdef CHIMAERA_REMOTE_DEBUG
  if (task->pool_ != chi::ADMIN_POOL_ID &&
      !task->task_flags_.Any(TASK_REMOTE_DEBUG_MARK) &&
      !task->IsLongRunning() && task->method_ != TaskMethod::kCreate &&
      CHI_RUNTIME->remote_created_) {
    task->SetRemote();
  }
  if (task->IsTriggerComplete()) {
    task->UnsetRemote();
  }
#endif
  RunContext &rctx = task->rctx_;
  if (task->IsTriggerComplete()) {
    return PushCompletedTask(rctx, task);
  }
  if (task->IsRouted()) {
    return PushRoutedTask(rctx, task);
  }
  std::vector<ResolvedDomainQuery> resolved =
      CHI_RPC->ResolveDomainQuery(task->pool_, task->dom_query_, false);
  DomainQuery res_query = resolved[0].dom_;
  if (!task->IsRemote() && resolved.size() == 1 &&
      resolved[0].node_ == CHI_RPC->node_id_ &&
      res_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
    return PushLocalTask(res_query, rctx, task);
  } else {
    return PushRemoteTask(rctx, task);
  }
  return true;
}

// CASE 1: The task is completed, just end it
HSHM_INLINE
bool PrivateTaskMultiQueue::PushCompletedTask(RunContext &rctx,
                                              const FullPtr<Task> &task) {
  HLOG(kInfo, kRemoteQueue, "[TASK_CHECK] Completing {}", task.ptr_);
  Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(task->pool_);
  CHI_CUR_WORKER->EndTask(exec, task, task->rctx_);
  return true;
}

// CASE 2: The task is already routed, just push it
HSHM_INLINE
bool PrivateTaskMultiQueue::PushRoutedTask(RunContext &rctx,
                                           const FullPtr<Task> &task) {
  Container *exec =
      CHI_MOD_REGISTRY->GetContainer(task->pool_, rctx.route_container_id_);
  if (!exec || !exec->is_created_) {
    return !GetFail().push(task).IsNull();
  }
  chi::Lane *chi_lane = rctx.route_lane_;
  chi_lane->push<false>(task);
  HLOG(kDebug, kWorkerDebug, "[TASK_CHECK] (node {}) Pushing task {}",
       CHI_CLIENT->node_id_, (void *)task.ptr_);
  return true;
}

// CASE 3: The task is local to this machine.
bool PrivateTaskMultiQueue::PushLocalTask(const DomainQuery &res_query,
                                          RunContext &rctx,
                                          const FullPtr<Task> &task) {
  // If the task is a flushing task. Place in the flush queue.
  // if (task->IsFlush()) {
  //   HLOG(kDebug, kWorkerDebug, "[TASK_CHECK] (node {}) Failing task {}",
  //        CHI_CLIENT->node_id_, (void *)task.ptr_);
  //   chi::Worker &flusher = CHI_WORK_ORCHESTRATOR->GetWorker(0);
  //   return !flusher.active_.GetFlush().push(task).IsNull();
  // }
  // Determine the lane the task should map to within container
  ContainerId container_id = res_query.sel_.id_;
  Container *exec = CHI_MOD_REGISTRY->GetContainer(task->pool_, container_id);
  if (!exec || !exec->is_created_) {
    // If the container doesn't exist, it's probably going to get created.
    // Put in the failed queue.
    HELOG(kWarning,
          "(node {}) For task {}, either {} or {} does not yet exist. If you "
          "see this message repeatedly, then there is something wrong.",
          CHI_CLIENT->node_id_, task->task_node_, task->pool_, container_id);
    return !GetFail().push(task).IsNull();
  }
  // Find the lane
  chi::Lane *chi_lane = exec->MapTaskToLane(task.ptr_);
  // if (rctx.load_.CalculateLoad()) {
  //   exec->Monitor(MonitorMode::kEstLoad, task->method_, task.ptr_, rctx);
  //   chi_lane->load_ += rctx.load_;
  // }
  rctx.exec_ = exec;
  rctx.route_container_id_ = container_id;
  rctx.route_lane_ = chi_lane;
  rctx.worker_id_ = chi_lane->worker_id_;
  task->SetRouted();
  chi_lane->push<false>(task);
  HLOG(kDebug, kWorkerDebug, "[TASK_CHECK] (node {}) Pushing task {}",
       CHI_CLIENT->node_id_, (void *)task.ptr_);
  return true;
}

// CASE 4: The task is remote to this machine
HSHM_INLINE
bool PrivateTaskMultiQueue::PushRemoteTask(RunContext &rctx,
                                           const FullPtr<Task> &task) {
  HLOG(kDebug, kWorkerDebug, "[TASK_CHECK] (node {}) Remoting task {}",
       CHI_CLIENT->node_id_, (void *)task.ptr_);
  // CASE 6: The task is remote to this machine, put in the remote queue.
  rctx.exec_ = CHI_MOD_REGISTRY->GetStaticContainer(task->pool_);
  if (!rctx.exec_) {
    HELOG(kFatal,
          "(node {}) Remote queue does not have static container "
          "established");
  }
  CHI_REMOTE_QUEUE->AsyncClientPushSubmitBase(
      HSHM_MCTX, nullptr, task->task_node_ + 1,
      DomainQuery::GetDirectId(SubDomainId::kGlobalContainers, 1), task.ptr_);
  return true;
}

/**===============================================================
 * Lanes
 * =============================================================== */
/**
 * Forward declaration of Lane:push templates
 * This was because ROCM compiler was not able to resolve the template
 * without this.
 */
template hshm::qtok_t Lane::push<false>(const FullPtr<Task> &task);
template hshm::qtok_t Lane::push<true>(const FullPtr<Task> &task);

/** Push a task  */
template <bool NO_COUNT>
hshm::qtok_t Lane::push(const FullPtr<Task> &task) {
  Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(worker_id_);
  Worker *cur_worker = CHI_CUR_WORKER;
  if (!cur_worker || worker.id_ != cur_worker->id_) {
    worker.active_.GetFail().push(task);
    return hshm::qtok_t();
  }
  if constexpr (!NO_COUNT) {
    size_t dup = count_.fetch_add(1);
    if (dup == 0) {
      HLOG(kDebug, kWorkerDebug,
           "Requesting lane {} with count {} with task {}", this, dup,
           task.ptr_);
      worker.RequestLane(this);
    } else {
      HLOG(kDebug, kWorkerDebug, "Skipping lane {} with count {} with task {}",
           this, dup, task.ptr_);
    }
  }
  hshm::qtok_t ret = active_tasks_.push(task);
  return ret;
}

/** Pop a set of tasks in sequence */
size_t Lane::pop_prep(size_t count) { return count_.fetch_sub(count) - count; }

/** Pop a task */
hshm::qtok_t Lane::pop(FullPtr<Task> &task) {
  hshm::qtok_t ret = active_tasks_.pop(task);
  if (!ret.IsNull() && !task->IsLongRunning()) {
    HLOG(kDebug, kWorkerDebug, "Popping task {} from {}", task.ptr_, this);
  }
  return ret;
}

/**===============================================================
 * Initialize Worker
 * =============================================================== */

/** Constructor */
Worker::Worker(WorkerId id, int cpu_id, ABT_xstream xstream) {
  id_ = id;
  sleep_us_ = 0;
  pid_ = 0;
  affinity_ = cpu_id;
  for (int i = 0; i < 16; ++i) {
    AllocateStack();
  }

  // MAX_DEPTH * [LOW_LAT, LONG_LAT]
  config::QueueManagerInfo &qm = CHI_QM->config_->queue_manager_;
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
    HSHM_THREAD_MODEL->Yield();
  }
  // On the root worker, detect if any work was done
  if (active_.GetFlush().size()) {
    if (AnyFlushWorkDone(orch)) {
      // Ensure that workers are relabeled as flushing
      flush_.flushing_ = true;
    } else {
      // Reap all FlushTasks and end recurion
      PollTempQueue<true>(active_.GetFlush(), false);
      HILOG(kInfo, "ENDING FLUSH (node={})", CHI_CLIENT->node_id_);
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
  CHI_WORK_ORCHESTRATOR->SetCurrentWorker(this);
  pid_ = HSHM_SYSTEM_INFO->pid_;
  SetCpuAffinity(affinity_);
  if (IsContinuousPolling()) {
    MakeDedicated();
  }
  HLOG(kDebug, kWorkerDebug, "Entered worker {}", id_);
  HILOG(kInfo, "CURRENT WORKER {} (node {})", CHI_CUR_WORKER->id_,
        CHI_CLIENT->node_id_);
  WorkOrchestrator *orch = CHI_WORK_ORCHESTRATOR;
  cur_time_.Refresh();
  while (orch->IsAlive()) {
    try {
      load_nsec_ = 0;
      bool flushing = flush_.flushing_ || active_.GetFlush().size();
      if (flushing) {
        // TODO(llogan): Re-enable this
        // BeginFlush(orch);
      }
      Run(flushing);
      if (flushing) {
        EndFlush(orch);
      }
      cur_time_.Refresh();
      iter_count_ += 1;
      if (load_nsec_ == 0) {
        // HSHM_THREAD_MODEL->SleepForUs(200);
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
  for (size_t i = 0; i < 8192; ++i) {
    IngestProcLanes(flushing);
    PollPrivateLaneMultiQueue(active_.active_lanes_.GetLowLatency(), flushing);
    PollTempQueue<false>(active_.GetFail(), flushing);
  }
  PollPrivateLaneMultiQueue(active_.active_lanes_.GetHighLatency(), flushing);
  PollTempQueue<false>(active_.GetFail(), flushing);
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
    FullPtr<Task> task(entry);
    active_.push(task);
  }
}

/** Poll the set of tasks in the private queue */
template <bool FROM_FLUSH>
HSHM_INLINE void Worker::PollTempQueue(PrivateTaskQueue &priv_queue,
                                       bool flushing) {
  size_t size = priv_queue.size();
  for (size_t i = 0; i < size; ++i) {
    FullPtr<Task> task;
    if (priv_queue.pop(task).IsNull()) {
      break;
    }
    if (task.IsNull()) {
      continue;
    }
    if constexpr (FROM_FLUSH) {
      if (task->IsFlush()) {
        task->UnsetFlush();
      }
    }
    active_.push(task);
  }
}

/** Poll the set of tasks in the private queue */
HSHM_INLINE
size_t Worker::PollPrivateLaneMultiQueue(PrivateLaneQueue &lanes,
                                         bool flushing) {
  size_t work = 0;
  size_t num_lanes = lanes.size();
  // HSHM_PERIODIC(0)->Run(SECONDS(1), [&] {
  //   size_t num_lanes = lanes.size();
  //   for (size_t lane_off = 0; lane_off < num_lanes; ++lane_off) {
  //     chi::Lane *chi_lane;
  //     if (lanes.pop(chi_lane).IsNull()) {
  //       break;
  //     }
  //     HLOG(kDebug, kWorkerDebug, "Polling lane {} with {} tasks", chi_lane,
  //           chi_lane->size());
  //     lanes.push(chi_lane);
  //   }
  // });
  if (num_lanes) {
    for (size_t lane_off = 0; lane_off < num_lanes; ++lane_off) {
      // Get the lane and make it current
      chi::Lane *chi_lane;
      if (lanes.pop(chi_lane).IsNull() || chi_lane == nullptr) {
        break;
      }
      cur_lane_ = chi_lane;
      if (cur_lane_ == nullptr) {
        HELOG(kFatal, "Lane is null, should never happen");
      }
      // Poll each task in the lane
      size_t max_lane_size = chi_lane->size();
      if (max_lane_size == 0) {
        HLOG(kDebug, kWorkerDebug, "Lane has no tasks {}", chi_lane);
      }
      size_t done_tasks = 0;
      for (; max_lane_size > 0; --max_lane_size) {
        FullPtr<Task> task;
        if (chi_lane->pop(task).IsNull()) {
          HLOG(kDebug, kWorkerDebug, "Lane has no tasks {}", chi_lane);
          break;
        }
        if (task.IsNull()) {
          continue;
        }
        bool pushback = RunTask(task, flushing);
        if (pushback) {
          chi_lane->push<true>(task);
        } else {
          ++done_tasks;
        }
        ++work;
      }
      // If the lane still has tasks, push it back
      size_t after_size = chi_lane->pop_prep(done_tasks);
      if (after_size > 0) {
        lanes.push(chi_lane);
        if (done_tasks > 0) {
          HLOG(kDebug, kWorkerDebug, "Requeuing lane {} with count {}",
               chi_lane, after_size);
        }
      } else {
        HLOG(kDebug, kWorkerDebug, "Dequeuing lane {} with count {}", chi_lane,
             chi_lane->size());
      }
    }
  }
  return work;
}

/** Run a task */
bool Worker::RunTask(FullPtr<Task> &task, bool flushing) {
#ifdef HSHM_DEBUG
  if (!task->IsLongRunning()) {
    HLOG(kDebug, kWorkerDebug, "");
  }
#endif
  // Get task properties
  ibitfield props = GetTaskProperties(task.ptr_, flushing);
  // Pack runtime context
  RunContext &rctx = task->rctx_;
  rctx.worker_props_ = props;
  rctx.flush_ = &flush_;
  // Run the task
  if (!task->IsTriggerComplete() && !task->IsBlocked()) {
    // Make this task current
    cur_task_ = task.ptr_;
    // Check if the task is dynamically-scheduled
    if (task->dom_query_.IsDynamic()) {
      rctx.exec_->Monitor(MonitorMode::kSchedule, task->method_, task.ptr_,
                          rctx);
      if (!task->IsRouted()) {
        active_.GetFail().push(task);
        return false;
      }
    }
    // Execute the task based on its properties
    ExecTask(task, rctx, rctx.exec_, props);
  }
  // Cleanup allocations
  bool pushback = true;
  if (task->IsBlocked()) {
    pushback = false;
    HILOG(kInfo, "(node {}) Blocking task {}", CHI_CLIENT->node_id_, *task);
    task->UnsetBlockedAndYielded();
  } else if (task->IsYielded()) {
    pushback = true;
    task->UnsetYielded();
  } else if (task->IsLongRunning() && !task->IsTriggerComplete()) {
    pushback = true;
  } else {
    pushback = false;
    EndTask(rctx.exec_, task, rctx);
  }
  return pushback;
}

/** Run an arbitrary task */
HSHM_INLINE
void Worker::ExecTask(FullPtr<Task> &task, RunContext &rctx, Container *&exec,
                      ibitfield &props) {
  // Determine if a task should be executed
  if (!props.All(CHI_WORKER_SHOULD_RUN)) {
    return;
  }
  // Flush tasks
  if (props.Any(CHI_WORKER_IS_FLUSHING)) {
    if (!task->IsLongRunning() && !task->ShouldSignalRemoteComplete()) {
      // flush_.count_ += 1;
    }
  }
  // Execute + monitor the task
  ExecCoroutine(task.ptr_, rctx);
}

/** Run a task */
HSHM_INLINE
void Worker::ExecCoroutine(Task *&task, RunContext &rctx) {
  // If task isn't started, allocate stack pointer
  if (!task->IsStarted()) {
    rctx.co_task_ = task;
    rctx.stack_ptr_ = AllocateStack();
    if (rctx.stack_ptr_ == nullptr) {
      HELOG(kFatal, "The stack pointer of size {} is NULL", stack_size_);
    }
    rctx.jmp_.fctx = bctx::make_fcontext((char *)rctx.stack_ptr_ + stack_size_,
                                         stack_size_, &Worker::CoroutineEntry);
    task->SetStarted();
  }
  // Jump to CoroutineEntry
  rctx.jmp_ = bctx::jump_fcontext(rctx.jmp_.fctx, &rctx);
  if (!task->IsStarted()) {
    FreeStack(rctx.stack_ptr_);
  }
}

/** Run a coroutine */
void Worker::CoroutineEntry(bctx::transfer_t t) {
  RunContext &rctx = *reinterpret_cast<RunContext *>(t.data);
  Task *task = rctx.co_task_;
  Container *&exec = rctx.exec_;
  chi::Lane *chi_lane = CHI_CUR_LANE;
  if (chi_lane == nullptr) {
    HELOG(kFatal, "Lane is null, should never happen");
  }
  rctx.jmp_ = t;
  try {
    exec->Run(task->method_, task, rctx);
  } catch (hshm::Error &e) {
    HELOG(kError, "(node {}) Worker {} caught an error: {}",
          CHI_CLIENT->node_id_, rctx.worker_id_, e.what());
  } catch (std::exception &e) {
    HELOG(kError, "(node {}) Worker {} caught an exception: {}",
          CHI_CLIENT->node_id_, rctx.worker_id_, e.what());
  } catch (...) {
    HELOG(kError, "(node {}) Worker {} caught an unknown exception",
          CHI_CLIENT->node_id_, rctx.worker_id_);
  }
  task->UnsetStarted();
  task->BaseYield();
}

/** Free a task when it is no longer needed */
HSHM_INLINE
void Worker::EndTask(Container *exec, FullPtr<Task> task, RunContext &rctx) {
  // Unblock the task pending on this one's completion
  if (task->ShouldSignalUnblock()) {
    Task *pending_to = rctx.pending_to_;
    if (!pending_to || pending_to == task.ptr_) {
      HELOG(kFatal,
            "(node {}) Invalid pending to during signaling unblock for task {}",
            CHI_CLIENT->node_id_, *task);
    }
    CHI_WORK_ORCHESTRATOR->SignalUnblock(pending_to, pending_to->rctx_);
  }
  // Signal back to the remote that spawned this task
  if (task->ShouldSignalRemoteComplete()) {
    Container *remote_exec =
        CHI_MOD_REGISTRY->GetContainer(CHI_REMOTE_QUEUE->id_, 1);
    remote_exec->Run(chi::remote_queue::Method::kServerPushComplete, task.ptr_,
                     rctx);
    return;
  }
  // Update the lane's load
  // chi_lane is null if this was a remote task
  // if (task->IsRouted()) {
  //   chi::Lane *chi_lane = rctx.route_lane_;
  //   chi_lane->load_ -= rctx.load_;
  // }
  // Free or complete the task
  if (exec && task->IsFireAndForget()) {
    CHI_CLIENT->DelTask(HSHM_MCTX, exec, task.ptr_);
  } else {
    task->SetComplete();
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
ibitfield Worker::GetTaskProperties(Task *&task, bool flushing) {
  ibitfield props;

  bool should_run = task->ShouldRun(cur_time_, flushing);
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
void *Worker::AllocateStack() {
  void *stack = (void *)stacks_.pop();
  if (!stack) {
    stack = malloc(stack_size_);
  }
  return stack;
}

/** Free a stack */
void Worker::FreeStack(void *stack) {
  stacks_.push((hipc::list_queue_entry *)stack);
}

}  // namespace chi
