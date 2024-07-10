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

#ifndef HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H_
#define HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/task_registry/task_registry.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera/api/chimaera_runtime.h"
#include <thread>
#include <queue>
#include "affinity.h"
#include "chimaera/network/rpc_thallium.h"

#include "chimaera/util/key_queue.h"
#include "chimaera/util/key_set.h"

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
  retries_ = 1;
  pid_ = 0;
  affinity_ = cpu_id;
  stacks_.Resize(num_stacks_);
  for (int i = 0; i < 16; ++i) {
    stacks_.emplace(malloc(stack_size_));
  }
  // MAX_DEPTH * [LOW_LAT, LONG_LAT]
  config::QueueManagerInfo &qm = CHI_QM_RUNTIME->config_->queue_manager_;
  active_.Init(id_, qm.proc_queue_depth_, qm.queue_depth_, qm.max_containers_pn_);
  cur_time_.Now();

  // Spawn threads
  xstream_ = xstream;
  // thread_ = std::make_unique<std::thread>(&Worker::Loop, this);
  // pthread_id_ = thread_->native_handle();
  tl_thread_ = CHI_WORK_ORCHESTRATOR->SpawnAsyncThread(
      xstream_, &Worker::WorkerEntryPoint, this);
}

/** Constructor without threading */
Worker::Worker(u32 id) {
  poll_queues_.Resize(1024);
  relinquish_queues_.Resize(1024);
  id_ = id;
  sleep_us_ = 0;
  EnableContinuousPolling();
  retries_ = 1;
  pid_ = 0;
  // pthread_id_ = GetLinuxTid();
}

/**===============================================================
 * Run tasks
 * =============================================================== */

/** Worker entrypoint */
void Worker::WorkerEntryPoint(void *arg) {
  Worker *worker = (Worker*)arg;
  worker->Loop();
}

/** Flush the worker's tasks */
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
  // Detect if any work has been done
  if (active_.GetFlush().size()) {
    if (AnyFlushWorkDone(orch)) {
      flush_.flush_iter_ = 0;
    } else {
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
  WorkOrchestrator *orch = CHI_WORK_ORCHESTRATOR;
  cur_time_.Now();
  size_t work = 0;
  while (orch->IsAlive()) {
    try {
      bool flushing = flush_.flushing_ || active_.GetFlush().size_;
      if (flushing) {
        BeginFlush(orch);
      }
      work += Run(flushing);
      if (flushing) {
        EndFlush(orch);
      }
      ++work;
      if (work >= 10000) {
        work = 0;
        cur_time_.Now();
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}", CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception", CHI_CLIENT->node_id_, id_);
    }
    if (!IsContinuousPolling()) {
      Yield();
    }
  }
  HILOG(kInfo, "(node {}) Worker {} wrapping up",
        CHI_CLIENT->node_id_, id_);
  Run(true);
  HILOG(kInfo, "(node {}) Worker {} has exited",
        CHI_CLIENT->node_id_, id_);
}

/** Run a single iteration over all queues */
size_t Worker::Run(bool flushing) {
//  hshm::Timer t;
//  t.Resume();
  // Are there any queues pending scheduling
  if (poll_queues_.size() > 0) {
    _PollQueues();
  }
  // Are there any queues pending descheduling
  if (relinquish_queues_.size() > 0) {
    _RelinquishQueues();
  }
  // Process tasks in the pending queues
  size_t work = 0;
  IngestProcLanes(flushing);
  size_t latwork = 0;
  for (size_t i = 0; i < 8192; ++i) {
    size_t diff = 0;
    IngestInterLanes(flushing);
    diff += PollPrivateQueue(active_.GetConstruct(), flushing);
    diff += PollPrivateQueue(active_.GetLowLat(), flushing);
//    if (diff == 0) {
//      break;
//    }
    latwork += diff;
    work += diff;
  }
  work += PollPrivateQueue(active_.GetHighLat(), flushing);
  PollPrivateQueue(active_.GetLongRunning(), flushing);
//  t.Pause();
//  if (latwork) {
//    HILOG(kInfo, "Worker iteration took {} us (work {})",
//          t.GetUsec(), latwork);
//  }
  return work;
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
    TaskRouteMode route = Reroute(task->pool_,
                                  dom_query,
                                  task,
                                  ig_lane);
    if (route == TaskRouteMode::kLocalWorker) {
      ig_lane->pop();
    } else {
      if (active_.push(PrivateTaskQueueEntry{task, dom_query})) {
//          HILOG(kInfo, "[TASK_CHECK] Running rep_task {} on node {} (long running: {})"
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
TaskRouteMode Worker::Reroute(const PoolId &scope,
                              DomainQuery &dom_query,
                              LPointer<Task> task,
                              ingress::Lane *ig_lane) {
#ifdef CHIMAERA_REMOTE_DEBUG
  if (task->pool_ != CHI_QM_CLIENT->admin_pool_id_ &&
      !task->task_flags_.Any(TASK_REMOTE_DEBUG_MARK) &&
      !task->IsLongRunning() &&
      task->method_ != TaskMethod::kCreate &&
      CHI_RUNTIME->remote_created_ &&
      !task->IsRemote()) {
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
    Container *exec = CHI_TASK_REGISTRY->GetContainer(
        task->pool_, dom_query.sel_.id_);
    chi::Lane *chi_lane = exec->GetLane(task->rctx_.route_lane_);
    if (chi_lane->ingress_id_ == ig_lane->id_) {
      return TaskRouteMode::kThisWorker;
    } else {
      MultiQueue *queue = CHI_CLIENT->GetQueue(
          CHI_QM_RUNTIME->admin_queue_id_);
      ingress::LaneGroup &ig_lane_group =
          queue->GetGroup(chi_lane->ingress_id_.node_id_);
      ingress::Lane &new_ig_lane = ig_lane_group.GetLane(
          chi_lane->ingress_id_.unique_);
      new_ig_lane.emplace(task.shm_);
      return TaskRouteMode::kLocalWorker;
    }
  }
}

/** Process completion events */
void Worker::ProcessCompletions() {
  while (active_.unblock());
}

/** Poll the set of tasks in the private queue */
HSHM_ALWAYS_INLINE
size_t Worker::PollPrivateQueue(PrivateTaskQueue &queue, bool flushing) {
  size_t work = 0;
  size_t size = queue.size_;
  for (size_t i = 0; i < size; ++i) {
    PrivateTaskQueueEntry entry;
    queue.pop(entry);
    if (entry.task_.ptr_ == nullptr) {
      break;
    }
    bool pushback = RunTask(
        queue, entry,
        entry.task_,
        flushing);
    if (pushback) {
      queue.push(entry);
    }
    ++work;
  }
  ProcessCompletions();
  return work;
}

/** Run a task */
bool Worker::RunTask(PrivateTaskQueue &queue,
                     PrivateTaskQueueEntry &entry,
                     LPointer<Task> task,
                     bool flushing) {
  // Get task properties
  bitfield32_t props =
      GetTaskProperties(task.ptr_, cur_time_,
                        flushing);
  // Get the task container
  Container *exec;
  if (props.Any(HSHM_WORKER_IS_REMOTE)) {
    exec = CHI_TASK_REGISTRY->GetStaticContainer(task->pool_);
  } else {
    exec = CHI_TASK_REGISTRY->GetContainer(task->pool_,
                                           entry.res_query_.sel_.id_);
  }
  if (!exec) {
    if (task->pool_ == PoolId::GetNull()) {
      HELOG(kFatal, "(node {}) Task {} pool does not exist",
            CHI_CLIENT->node_id_, task->task_node_);
      task->SetModuleComplete();
      return false;
    } else {
//        HELOG(kFatal, "(node {}) Could not find the pool {} for task {}"
//                        " with query: {}",
//              CHI_CLIENT->node_id_, task->pool_, task->task_node_,
//              entry.res_query_);
    }
    return true;
  }
  // Check if the task is apart of a plugged lane
  // TODO(llogan): Upgrade + migrate
//    chi::Lane *chi_lane = exec->Route(task.ptr_);
//    if (chi_lane->IsPlugged()) {
//      if (active_graphs_.find(task->task_node_.root_) == active_graphs_.end()) {
//        // Place into plug list
//        return false;
//      }
//    }
//    ++chi_lane->active_;
  // Pack runtime context
  RunContext &rctx = task->rctx_;
  rctx.worker_id_ = id_;
  rctx.flush_ = &flush_;
  rctx.exec_ = exec;
  // Allocate remote task and execute here
  // Execute the task based on its properties
  if (!task->IsModuleComplete()) {
    ExecTask(queue, entry, task.ptr_, rctx, exec, props);
  }
  // Cleanup allocations
  bool pushback = true;
  if (task->IsModuleComplete()) {
    pushback = false;
    // active_.erase(queue.id_);
    active_.erase(queue.id_, entry);
    EndTask(exec, task);
  } else if (task->IsBlocked()) {
    pushback = false;
  }
  return pushback;
}

/** Run an arbitrary task */
HSHM_ALWAYS_INLINE
void Worker::ExecTask(PrivateTaskQueue &queue,
                      PrivateTaskQueueEntry &entry,
                      Task *&task,
                      RunContext &rctx,
                      Container *&exec,
                      bitfield32_t &props) {
  // Determine if a task should be executed
  if (!props.All(HSHM_WORKER_SHOULD_RUN)) {
    return;
  }
  // Flush tasks
  if (props.Any(HSHM_WORKER_IS_FLUSHING)) {
    if (task->IsLongRunning()) {
      exec->Monitor(MonitorMode::kFlushStat, task, rctx);
    } else if (!task->IsFlush()) {
      flush_.count_ += 1;
    }
  }
  // Make this task current
  cur_task_ = task;
  cur_lane_ = exec->GetLane(task->rctx_.route_lane_);
  // Monitoring callback
  if (!task->IsStarted()) {
    exec->Monitor(MonitorMode::kBeginTrainTime, task, rctx);
//    if (active_graphs_.find(task->task_node_.root_) == active_graphs_.end()) {
//      active_graphs_.emplace(task->task_node_.root_, 0);
//    } else {
//      active_graphs_[task->task_node_.root_] += 1;
//    }
  }
  // Submit the task to the local remote container
  if (props.Any(HSHM_WORKER_IS_REMOTE)) {
    task->SetBlocked(1);
    active_.block(entry);
    cur_task_ = nullptr;
    cur_lane_ = nullptr;
    LPointer<remote_queue::ClientPushSubmitTask> remote_task =
        CHI_REMOTE_QUEUE->AsyncClientPushSubmitBase(
            nullptr, task->task_node_ + 1,
            DomainQuery::GetDirectId(SubDomainId::kGlobalContainers, 1),
            task);
    return;
  }

  // Actually execute the task
  ExecCoroutine(task, rctx);
//    if (task->IsCoroutine()) {
//      ExecCoroutine(task, rctx);
//    } else {
//      exec->Run(task->method_, task, rctx);
//      task->SetStarted();
//    }
  // Monitoring callback
  if (!task->IsStarted()) {
    exec->Monitor(MonitorMode::kEndTrainTime, task, rctx);
//    active_graphs_[task->task_node_.root_] -= 1;
//    if (active_graphs_[task->task_node_.root_] == 0) {
//      active_graphs_.erase(task->task_node_.root_);
//    }
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
      HELOG(kFatal, "The stack pointer of size {} is NULL",
            stack_size_);
    }
    rctx.jmp_.fctx = bctx::make_fcontext(
        (char *) rctx.stack_ptr_ + stack_size_,
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
  Task *task = reinterpret_cast<Task*>(t.data);
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
        CHI_TASK_REGISTRY->GetContainer(CHI_REMOTE_QUEUE->id_, 1);
    remote_exec->Run(chi::remote_queue::Method::kServerPushComplete,
                     task.ptr_, task->rctx_);
    return;
  }
  if (exec && task->IsFireAndForget()) {
    CHI_CLIENT->DelTask(exec, task.ptr_);
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
  Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(
      unblock_task->rctx_.worker_id_);
  HILOG(kInfo, "Signalling worker {} ({})",
        unblock_task->rctx_.worker_id_,
        &worker);
  PrivateTaskMultiQueue &pending =
      worker.GetPendingQueue(unblock_task.ptr_);
  pending.signal_unblock(pending, unblock_task);
}

/** Get the characteristics of a task */
HSHM_ALWAYS_INLINE
bitfield32_t Worker::GetTaskProperties(Task *&task,
                                       hshm::Timepoint &cur_time,
                                       bool flushing) {
  bitfield32_t props;

  bool group_avail = true;
  bool should_run = task->ShouldRun(cur_time, flushing);
  if (task->IsRemote()) {
    props.SetBits(HSHM_WORKER_IS_REMOTE);
  }
  if (group_avail) {
    props.SetBits(HSHM_WORKER_GROUP_AVAIL);
  }
  if (should_run) {
    props.SetBits(HSHM_WORKER_SHOULD_RUN);
  }
  if (flushing) {
    props.SetBits(HSHM_WORKER_IS_FLUSHING);
  }
  if (task->IsLongRunning()) {
    props.SetBits(HSHM_WORKER_LONG_RUNNING);
  }
  return props;
}

/** Join worker */
void Worker::Join() {
  // thread_->join();
  ABT_xstream_join(xstream_);
}

/** Get the pending queue for a worker */
PrivateTaskMultiQueue& Worker::GetPendingQueue(Task *task) {
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
void Worker::_RelinquishQueues() {
}

/** Check if worker is still stealing queues */
bool Worker::IsRelinquishingQueues() {
  return relinquish_queues_.size() > 0;
}

/** Set the sleep cycle */
void Worker::SetPollingFrequency(size_t sleep_us, u32 num_retries) {
  sleep_us_ = sleep_us;
  retries_ = num_retries;
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
void Worker::SetHighLatency() {
  flags_.SetBits(WORKER_HIGH_LATENCY);
}

/** Check if continuously polling */
bool Worker::IsHighLatency() {
  return flags_.Any(WORKER_HIGH_LATENCY);
}

/** Check if continuously polling */
void Worker::SetLowLatency() {
  flags_.SetBits(WORKER_LOW_LATENCY);
}

/** Check if continuously polling */
bool Worker::IsLowLatency() {
  return flags_.Any(WORKER_LOW_LATENCY);
}

/** Set the CPU affinity of this worker */
void Worker::SetCpuAffinity(int cpu_id) {
  affinity_ = cpu_id;
  ABT_xstream_set_affinity(xstream_, 1, &cpu_id);
}

/** Make maximum priority process */
void Worker::MakeDedicated() {
  int policy = SCHED_FIFO;
  struct sched_param param = { .sched_priority = 1 };
  sched_setscheduler(0, policy, &param);
}

/** Worker yields for a period of time */
void Worker::Yield() {
  if (flags_.Any(WORKER_CONTINUOUS_POLLING)) {
    return;
  }
  if (sleep_us_ > 0) {
    HERMES_THREAD_MODEL->SleepForUs(sleep_us_);
  } else {
    HERMES_THREAD_MODEL->Yield();
  }
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
  if(!stacks_.emplace(stack).IsNull()) {
    return;
  }
  stacks_.Resize(stacks_.size() * 2 + num_stacks_);
  stacks_.emplace(stack);
}

}  // namespace chi

#endif  // HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H_
