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
#include "chimaera/api/chimaera_runtime_.h"
#include <thread>
#include <queue>
#include "affinity.h"
#include "chimaera/network/rpc_thallium.h"

#include "chimaera/util/key_queue.h"
#include "chimaera/util/key_set.h"

static inline pid_t GetLinuxTid() {
  return syscall(SYS_gettid);
}

#define HSHM_WORKER_IS_REMOTE BIT_OPT(u32, 0)
#define HSHM_WORKER_GROUP_AVAIL BIT_OPT(u32, 1)
#define HSHM_WORKER_SHOULD_RUN BIT_OPT(u32, 2)
#define HSHM_WORKER_IS_FLUSHING BIT_OPT(u32, 3)
#define HSHM_WORKER_LONG_RUNNING BIT_OPT(u32, 4)

namespace chi {

#define WORKER_CONTINUOUS_POLLING BIT_OPT(u32, 0)
#define WORKER_LOW_LATENCY BIT_OPT(u32, 1)
#define WORKER_HIGH_LATENCY BIT_OPT(u32, 2)

/** Uniquely identify a queue lane */
struct WorkEntry {
  u32 prio_;
  ContainerId container_id_;
  Lane *lane_;
  LaneGroup *group_;
  MultiQueue *queue_;

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry() = default;

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(u32 prio, LaneId lane_id, MultiQueue *queue)
  : prio_(prio), container_id_(lane_id), queue_(queue) {
    group_ = &queue->GetGroup(prio);
    lane_ = &queue->GetLane(prio, lane_id);
  }

  /** Copy constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(const WorkEntry &other) {
    prio_ = other.prio_;
    container_id_ = other.container_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Copy assignment */
  HSHM_ALWAYS_INLINE
  WorkEntry& operator=(const WorkEntry &other) {
    if (this != &other) {
      prio_ = other.prio_;
      container_id_ = other.container_id_;
      lane_ = other.lane_;
      group_ = other.group_;
      queue_ = other.queue_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(WorkEntry &&other) noexcept {
    prio_ = other.prio_;
    container_id_ = other.container_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Move assignment */
  HSHM_ALWAYS_INLINE
  WorkEntry& operator=(WorkEntry &&other) noexcept {
    if (this != &other) {
      prio_ = other.prio_;
      container_id_ = other.container_id_;
      lane_ = other.lane_;
      group_ = other.group_;
      queue_ = other.queue_;
    }
    return *this;
  }

  /** Check if null */
  [[nodiscard]]
  HSHM_ALWAYS_INLINE bool IsNull() const {
    return queue_->IsNull();
  }

  /** Equality operator */
  HSHM_ALWAYS_INLINE
  bool operator==(const WorkEntry &other) const {
    return queue_ == other.queue_ && container_id_ == other.container_id_ &&
        prio_ == other.prio_;
  }
};

}  // namespace chi

namespace std {
/** Hash function for WorkEntry */
template<>
struct hash<chi::WorkEntry> {
  HSHM_ALWAYS_INLINE
      std::size_t
  operator()(const chi::WorkEntry &key) const {
    return std::hash<chi::MultiQueue*>{}(key.queue_) +
        std::hash<u32>{}(key.container_id_) + std::hash<u64>{}(key.prio_);
  }
};
}  // namespace std

namespace chi {

struct PrivateTaskQueueEntry {
 public:
  LPointer<Task> task_;
  DomainQuery res_query_;
  size_t next_, prior_;
  ssize_t block_count_ = 1;          /**< Count used for unblocking */

 public:
  PrivateTaskQueueEntry() = default;
  PrivateTaskQueueEntry(const LPointer<Task> &task,
                        const DomainQuery &res_query)
  : task_(task), res_query_(res_query) {}
  template<typename TaskT>
  PrivateTaskQueueEntry(const LPointer<TaskT> &task,
                        const DomainQuery &res_query)
      : res_query_(res_query) {
    task_.ptr_ = (Task*)task.ptr_;
    task_.shm_ = task.shm_;
  }

  PrivateTaskQueueEntry(const PrivateTaskQueueEntry &other) = default;

  PrivateTaskQueueEntry& operator=(const PrivateTaskQueueEntry &other) = default;

  PrivateTaskQueueEntry(PrivateTaskQueueEntry &&other) noexcept = default;

  PrivateTaskQueueEntry& operator=(PrivateTaskQueueEntry &&other) noexcept  = default;
};

typedef KeySet<PrivateTaskQueueEntry> PrivateTaskSet;
typedef KeyQueue<PrivateTaskQueueEntry> PrivateTaskQueue;

class PrivateTaskMultiQueue {
 public:
  inline static const int ROOT = 0;
  inline static const int CONSTRUCT = 1;
  inline static const int LOW_LAT = 2;
  inline static const int HIGH_LAT = 3;
  inline static const int LONG_RUNNING = 4;
  inline static const int FLUSH = 5;
  inline static const int REMOTE = 6;
  inline static const int NUM_QUEUES = 7;

 public:
  size_t root_count_;
  size_t max_root_count_;
  PrivateTaskQueue queues_[NUM_QUEUES];
  PrivateTaskSet blocked_;
  std::unique_ptr<hshm::mpsc_queue<LPointer<Task>>> complete_;
  size_t id_;

 public:
  void Init(size_t id, size_t pqdepth, size_t qdepth, size_t max_lanes) {
    id_ = id;
    queues_[ROOT].Init(ROOT, max_lanes * qdepth);
    queues_[CONSTRUCT].Init(CONSTRUCT, max_lanes * qdepth);
    queues_[LOW_LAT].Init(LOW_LAT, max_lanes * qdepth);
    queues_[HIGH_LAT].Init(HIGH_LAT, max_lanes * qdepth);
    queues_[LONG_RUNNING].Init(LONG_RUNNING, max_lanes * qdepth);
    queues_[FLUSH].Init(LONG_RUNNING, max_lanes * qdepth);
    queues_[REMOTE].Init(REMOTE, max_lanes * qdepth);
    blocked_.Init(max_lanes * qdepth);
    complete_ = std::make_unique<hshm::mpsc_queue<LPointer<Task>>>(
        max_lanes * qdepth);
    root_count_ = 0;
    max_root_count_ = max_lanes * pqdepth;
  }

  PrivateTaskQueue& GetRoot() {
    return queues_[ROOT];
  }

  PrivateTaskQueue& GetConstruct() {
    return queues_[CONSTRUCT];
  }

  PrivateTaskQueue& GetLowLat() {
    return queues_[LOW_LAT];
  }

  PrivateTaskQueue& GetHighLat() {
    return queues_[HIGH_LAT];
  }

  PrivateTaskQueue& GetLongRunning() {
    return queues_[LONG_RUNNING];
  }

  PrivateTaskQueue& GetFlush() {
    return queues_[FLUSH];
  }

  PrivateTaskQueue& GetRemote() {
    return queues_[REMOTE];
  }

  hshm::mpsc_queue<LPointer<Task>>& GetCompletion() {
    return *complete_;
  }

  bool push(const PrivateTaskQueueEntry &entry) {
    Task *task = entry.task_.ptr_;
    if (task->pool_ == CHI_ADMIN->id_ &&
        task->method_ == chi::Admin::Method::kCreateContainer) {
      return GetConstruct().push(entry);
    } else if (task->IsFlush()) {
      return GetFlush().push(entry);
    } else if (task->IsLongRunning()) {
      return GetLongRunning().push(entry);
    } else if (task->task_node_.node_depth_ == 0) {
      return GetRoot().push(entry);
    } else if (task->prio_ == TaskPrio::kLowLatency) {
      return GetLowLat().push(entry);
    } else {
      return GetHighLat().push(entry);
    }
  }

  void erase(int queue_id) {
//    if (queue_id == ROOT) {
//      --root_count_;
//    }
    queues_[queue_id].erase();
  }

  void erase(int queue_id, PrivateTaskQueueEntry&) {
//    if (queue_id == ROOT) {
//      --root_count_;
//    }
  }

  void block(PrivateTaskQueueEntry &entry) {
    LPointer<Task> blocked_task = entry.task_;
    entry.block_count_ = (ssize_t)blocked_task->rctx_.block_count_;
    blocked_.emplace(entry, blocked_task->rctx_.pending_key_);
    HILOG(kInfo, "(node {}) Blocking task {} with count {}",
          CHI_RPC->node_id_, (void*)blocked_task.ptr_, entry.block_count_);
  }

  void signal_unblock(PrivateTaskMultiQueue &worker_pending,
                      LPointer<Task> &blocked_task) {
    worker_pending.GetCompletion().emplace(blocked_task);
  }

  bool unblock() {
    LPointer<Task> blocked_task;
    hshm::mpsc_queue<LPointer<Task>> &complete = GetCompletion();
    if (complete.pop(blocked_task).IsNull()) {
      return false;
    }
    if (!blocked_task->IsBlocked()) {
      HELOG(kFatal, "An unblocked task was unblocked again");
    }
    PrivateTaskQueueEntry *entry;
    blocked_.peek(blocked_task->rctx_.pending_key_, entry);
    if (blocked_task.ptr_ != entry->task_.ptr_) {
      HELOG(kFatal, "A blocked task was lost");
    }
    entry->block_count_ -= 1;
    HILOG(kInfo, "(node {}) Unblocking task {} with count {}",
          CHI_RPC->node_id_, (void*)blocked_task.ptr_, entry->block_count_);
    if (entry->block_count_ == 0) {
      blocked_task->UnsetBlocked();
      blocked_.erase(blocked_task->rctx_.pending_key_);
      push(*entry);
    }
    return true;
  }
};

class Worker {
 public:
  u32 id_;  /**< Unique identifier of this worker */
  // std::unique_ptr<std::thread> thread_;  /**< The worker thread handle */
  // int pthread_id_;      /**< The worker pthread handle */
  ABT_thread tl_thread_;  /**< The worker argobots thread handle */
  std::atomic<int> pid_;  /**< The worker process id */
  int affinity_;        /**< The worker CPU affinity */
  u32 numa_node_;       // TODO(llogan): track NUMA affinity
  ABT_xstream xstream_;
  std::vector<WorkEntry> work_proc_queue_;  /**< The set of queues to poll */
  std::vector<WorkEntry> work_inter_queue_;  /**< The set of queues to poll */
  /**< A set of queues to begin polling in a worker */
  hshm::spsc_queue<std::vector<WorkEntry>> poll_queues_;
  /**< A set of queues to stop polling in a worker */
  hshm::spsc_queue<std::vector<WorkEntry>> relinquish_queues_;
  size_t sleep_us_;     /**< Time the worker should sleep after a run */
  u32 retries_;         /**< The number of times to repeat the internal run loop before sleeping */
  bitfield32_t flags_;  /**< Worker metadata flags */
  std::unordered_map<hshm::charbuf, TaskNode>
      group_map_;        /**< Determine if a task can be executed right now */
  hshm::charbuf group_;  /**< The current group */
  hshm::spsc_queue<void*> stacks_;  /**< Cache of stacks for tasks */
  int num_stacks_ = 256;  /**< Number of stacks */
  int stack_size_ = KILOBYTES(64);
  PrivateTaskMultiQueue
      active_;  /** Tasks pending to complete */
  hshm::Timepoint cur_time_;  /**< The current timepoint */
  WorkPending flush_;    /**< Info needed for flushing ops */


 public:
  /**===============================================================
   * Initialize Worker
   * =============================================================== */

  /** Constructor */
  Worker(u32 id, int cpu_id, ABT_xstream &xstream) {
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
  Worker(u32 id) {
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
  static void WorkerEntryPoint(void *arg) {
    Worker *worker = (Worker*)arg;
    worker->Loop();
  }

  /** Flush the worker's tasks */
  void BeginFlush(WorkOrchestrator *orch) {
    if (flush_.flush_iter_ == 0 && active_.GetFlush().size()) {
      for (std::unique_ptr<Worker> &worker : orch->workers_) {
        worker->flush_.flushing_ = true;
      }
    }
    ++flush_.flush_iter_;
  }

  /** Check if work has been done */
  void EndFlush(WorkOrchestrator *orch) {
    // Update the work count
    if (flush_.count_ != flush_.work_done_) {
      flush_.work_done_ = flush_.count_.load();
      flush_.did_work_ = true;
      return;
    }
    flush_.did_work_ = false;
    // Check if each worker has finished flushing
    if (FinishedFlushingWork(orch)) {
      flush_.flush_iter_ = 0;
      flush_.flushing_ = false;
      flush_.did_work_ = true;
      PollPrivateQueue(active_.GetFlush(), false);
    }
  }

  /** Barrier for all workers to flush */
  bool FinishedFlushingWork(WorkOrchestrator *orch) {
    for (std::unique_ptr<Worker> &worker : orch->workers_) {
      if (worker->flush_.did_work_) {
        return false;
      }
    }
    return true;
  }

  /** Worker loop iteration */
  void Loop() {
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
  size_t Run(bool flushing) {
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
    work += PollPrivateQueue(active_.GetRoot(), flushing);
    for (size_t i = 0; i < 8192; ++i) {
      size_t diff = 0;
      IngestInterLanes(flushing);
      diff += PollPrivateQueue(active_.GetConstruct(), flushing);
      diff += PollPrivateQueue(active_.GetLowLat(), flushing);
      if (diff == 0) {
        break;
      }
      work += diff;
    }
    work += PollPrivateQueue(active_.GetHighLat(), flushing);
    PollPrivateQueue(active_.GetLongRunning(), flushing);
    return work;
  }

  /** Ingest all process lanes */
  HSHM_ALWAYS_INLINE
  void IngestProcLanes(bool flushing) {
    for (WorkEntry &work_entry : work_proc_queue_) {
      IngestLane(work_entry);
    }
  }

  /** Ingest all intermediate lanes */
  HSHM_ALWAYS_INLINE
  void IngestInterLanes(bool flushing) {
    for (WorkEntry &work_entry : work_inter_queue_) {
      IngestLane(work_entry);
    }
  }

  /** Ingest a lane */
  HSHM_ALWAYS_INLINE
  void IngestLane(WorkEntry &lane_info) {
    // Ingest tasks from the ingress queues
    Lane *&lane = lane_info.lane_;
    LaneData *entry;
    while (true) {
      if (lane->peek(entry).IsNull()) {
        break;
      }
      LPointer<Task> task;
      task.shm_ = entry->p_;
      task.ptr_ = CHI_CLIENT->GetMainPointer<Task>(entry->p_);
      DomainQuery dom_query = task->dom_query_;
      TaskRouteMode route = Reroute(task->pool_,
                                    dom_query,
                                    task,
                                    lane);
      if (route == TaskRouteMode::kRemoteWorker) {
        task->SetRemote();
      }
      if (route == TaskRouteMode::kLocalWorker) {
        lane->pop();
      } else {
        if (active_.push(PrivateTaskQueueEntry{task, dom_query})) {
          HILOG(kInfo, "[TASK_CHECK] Running rep_task {} on node {} (long running: {})"
                       " pool={} method={}",
                (void*)task->rctx_.ret_task_addr_, CHI_RPC->node_id_,
                task->IsLongRunning(), task->pool_, task->method_);
          lane->pop();
        } else {
          break;
        }
      }
    }
  }

  /**
   * Detect if a DomainQuery is across nodes
   * */
  static TaskRouteMode Reroute(const PoolId &scope,
                               DomainQuery &dom_query,
                               LPointer<Task> task,
                               Lane *lane) {
    std::vector<ResolvedDomainQuery> resolved =
        CHI_RPC->ResolveDomainQuery(scope, dom_query, false);
    if (resolved.size() == 1 && resolved[0].node_ == CHI_RPC->node_id_) {
      dom_query = resolved[0].dom_;
#ifdef CHIMAERA_REMOTE_DEBUG
      if (task->pool_ != CHI_QM_CLIENT->admin_pool_id_ &&
          !task->task_flags_.Any(TASK_REMOTE_DEBUG_MARK) &&
          !task->IsLongRunning() &&
          task->method_ != TaskMethod::kCreate &&
          CHI_RUNTIME->remote_created_ &&
          !task->IsRemote()) {
        return TaskRouteMode::kRemoteWorker;
      }
#endif
      if (dom_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
        MultiQueue *queue = CHI_CLIENT->GetQueue(
            task->pool_);
        LaneGroup &lane_group = queue->GetGroup(task->prio_);
        u32 lane_id = dom_query.sel_.id_ % lane_group.num_lanes_;
        Lane &lane_cmp = lane_group.GetLane(lane_id);
        if (&lane_cmp == lane) {
          return TaskRouteMode::kThisWorker;
        } else {
          lane_cmp.emplace(task.shm_);
          return TaskRouteMode::kLocalWorker;
        }
      }
      return TaskRouteMode::kRemoteWorker;
    } else if (resolved.size() >= 1) {
      return TaskRouteMode::kRemoteWorker;
    } else {
      HELOG(kFatal, "{} resolved to no sub-queries for "
                    "task_node={} pool={}",
                    dom_query, task->task_node_, task->pool_);
    }
    return TaskRouteMode::kRemoteWorker;
  }

  /** Process completion events */
  void ProcessCompletions() {
    while (active_.unblock());
  }

  /** Poll the set of tasks in the private queue */
  HSHM_ALWAYS_INLINE
  size_t PollPrivateQueue(PrivateTaskQueue &queue, bool flushing) {
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
  bool RunTask(PrivateTaskQueue &queue,
               PrivateTaskQueueEntry &entry,
               LPointer<Task> task,
               bool flushing) {
    // Get task properties
    bitfield32_t props =
        GetTaskProperties(task.ptr_, cur_time_,
                          flushing);
    // Get the task state
    Container *exec;
    if (props.Any(HSHM_WORKER_IS_REMOTE)) {
      exec = CHI_TASK_REGISTRY->GetAnyContainer(task->pool_);
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
  void ExecTask(PrivateTaskQueue &queue,
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
        HILOG(kInfo, "(node {}) Flushing task {} pool {} method {}",
              CHI_RPC->node_id_, (void*)task,
              task->pool_, task->method_);
      }
    }

    // Monitoring callback
    if (!task->IsStarted()) {
      exec->Monitor(MonitorMode::kBeginTrainTime, task, rctx);
    }
    // Attempt to run the task if it's ready and runnable
    if (props.Any(HSHM_WORKER_IS_REMOTE)) {
//      HILOG(kInfo, "Automaking remote task {}", (size_t)task);
      task->SetBlocked(1);
      active_.block(entry);
      LPointer<remote_queue::ClientPushSubmitTask> remote_task =
          CHI_REMOTE_QUEUE->AsyncClientPushSubmit(
          nullptr, task->task_node_ + 1,
          DomainQuery::GetDirectHash(SubDomainId::kLocalContainers, 0),
          task);
//      std::vector<ResolvedDomainQuery> resolved =
//          CHI_RPC->ResolveDomainQuery(remote_task->pool_,
//                                      remote_task->dom_query_,
//                                      false);
//      PrivateTaskQueueEntry remote_entry{remote_task, resolved[0].dom_};
//      active_.GetRemote().push(remote_entry);
//      PollPrivateQueue(active_.GetRemote(), false);
      return;
    }

    // Actually execute the task
    // ExecCoroutine(task, rctx);
    if (task->IsCoroutine()) {
      ExecCoroutine(task, rctx);
    } else {
      exec->Run(task->method_, task, rctx);
      task->SetStarted();
    }
    // Monitoring callback
    if (task->IsModuleComplete()) {
      exec->Monitor(MonitorMode::kEndTrainTime, task, rctx);
    }
    task->DidRun(cur_time_);
    // Block the task
    if (task->IsBlocked()) {
      active_.block(entry);
    }
  }

  /** Run a task */
  HSHM_ALWAYS_INLINE
  void ExecCoroutine(Task *&task, RunContext &rctx) {
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
  static void CoroutineEntry(bctx::transfer_t t) {
    Task *task = reinterpret_cast<Task*>(t.data);
    RunContext &rctx = task->rctx_;
    Container *&exec = rctx.exec_;
    rctx.jmp_ = t;
    exec->Run(task->method_, task, rctx);
    task->UnsetStarted();
    task->Yield<TASK_YIELD_CO>();
  }

  /** Free a task when it is no longer needed */
  HSHM_ALWAYS_INLINE
  void EndTask(Container *exec, LPointer<Task> &task) {
    if (task->task_flags_.Any(TASK_REMOTE_RECV_MARK)) {
      HILOG(kInfo, "[TASK_CHECK] Server completed rep_task {} on node {}",
            (void*)task->rctx_.ret_task_addr_, CHI_RPC->node_id_);
    }
    if (task->ShouldSignalUnblock()) {
      SignalUnblock(task->rctx_.pending_to_);
    }
    if (task->ShouldSignalRemoteComplete()) {
      Container *remote_exec =
          CHI_TASK_REGISTRY->GetAnyContainer(CHI_REMOTE_QUEUE->id_);
      task->SetComplete();
      remote_exec->Run(chi::remote_queue::Method::kServerPushComplete,
                       task.ptr_, task->rctx_);
      return;
    }
    if (exec && task->IsFireAndForget()) {
      CHI_CLIENT->DelTask(exec, task.ptr_);
    } else {
      task->SetComplete();
    }
  }

  /**===============================================================
   * Helpers
   * =============================================================== */

  /** Unblock a task */
  static void SignalUnblock(Task *unblock_task) {
    LPointer<Task> ltask;
    ltask.ptr_ = unblock_task;
    ltask.shm_ = HERMES_MEMORY_MANAGER->Convert(ltask.ptr_);
    SignalUnblock(ltask);
  }

  /** Externally signal a task as complete */
  HSHM_ALWAYS_INLINE
  static void SignalUnblock(LPointer<Task> &unblock_task) {
    Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(
        unblock_task->rctx_.worker_id_);
    PrivateTaskMultiQueue &pending =
        worker.GetPendingQueue(unblock_task.ptr_);
    pending.signal_unblock(pending, unblock_task);
  }

  /** Get the characteristics of a task */
  HSHM_ALWAYS_INLINE
  bitfield32_t GetTaskProperties(Task *&task,
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
  void Join() {
    // thread_->join();
    ABT_xstream_join(xstream_);
  }

  /** Get the pending queue for a worker */
  PrivateTaskMultiQueue& GetPendingQueue(Task *task) {
    PrivateTaskMultiQueue &pending =
        CHI_WORK_ORCHESTRATOR->workers_[task->rctx_.worker_id_]->active_;
    return pending;
  }

  /** Tell worker to poll a set of queues */
  void PollQueues(const std::vector<WorkEntry> &queues) {
    poll_queues_.emplace(queues);
  }

  /** Actually poll the queues from within the worker */
  void _PollQueues() {
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
  void RelinquishingQueues(const std::vector<WorkEntry> &queues) {
    relinquish_queues_.emplace(queues);
  }

  /** Actually relinquish the queues from within the worker */
  void _RelinquishQueues() {
  }

  /** Check if worker is still stealing queues */
  bool IsRelinquishingQueues() {
    return relinquish_queues_.size() > 0;
  }

  /** Set the sleep cycle */
  void SetPollingFrequency(size_t sleep_us, u32 num_retries) {
    sleep_us_ = sleep_us;
    retries_ = num_retries;
    flags_.UnsetBits(WORKER_CONTINUOUS_POLLING);
  }

  /** Enable continuous polling */
  void EnableContinuousPolling() {
    flags_.SetBits(WORKER_CONTINUOUS_POLLING);
  }

  /** Disable continuous polling */
  void DisableContinuousPolling() {
    flags_.UnsetBits(WORKER_CONTINUOUS_POLLING);
  }

  /** Check if continuously polling */
  bool IsContinuousPolling() {
    return flags_.Any(WORKER_CONTINUOUS_POLLING);
  }

  /** Check if continuously polling */
  void SetHighLatency() {
    flags_.SetBits(WORKER_HIGH_LATENCY);
  }

  /** Check if continuously polling */
  bool IsHighLatency() {
    return flags_.Any(WORKER_HIGH_LATENCY);
  }

  /** Check if continuously polling */
  void SetLowLatency() {
    flags_.SetBits(WORKER_LOW_LATENCY);
  }

  /** Check if continuously polling */
  bool IsLowLatency() {
    return flags_.Any(WORKER_LOW_LATENCY);
  }

  /** Set the CPU affinity of this worker */
  void SetCpuAffinity(int cpu_id) {
    affinity_ = cpu_id;
    ABT_xstream_set_affinity(xstream_, 1, &cpu_id);
  }

  /** Make maximum priority process */
  void MakeDedicated() {
    int policy = SCHED_FIFO;
    struct sched_param param = { .sched_priority = 1 };
    sched_setscheduler(0, policy, &param);
  }

  /** Worker yields for a period of time */
  void Yield() {
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
  void* AllocateStack() {
    void *stack;
    if (!stacks_.pop(stack).IsNull()) {
      return stack;
    }
    return malloc(stack_size_);
  }

  /** Free a stack */
  void FreeStack(void *stack) {
    if(!stacks_.emplace(stack).IsNull()) {
      return;
    }
    stacks_.Resize(stacks_.size() * 2 + num_stacks_);
    stacks_.emplace(stack);
  }
};

}  // namespace chi

#endif  // HRUN_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H_
