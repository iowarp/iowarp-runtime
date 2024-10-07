//
// Created by llogan on 7/6/24.
//

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

#ifndef CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H
#define CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H

#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/module_registry/module_registry.h"
#include <thread>
#include <queue>
#include "affinity.h"
#include "chimaera/network/rpc_thallium.h"

#include "chimaera/util/key_queue.h"
#include "chimaera/util/key_set.h"
#include "chimaera/util/list_queue.h"

static inline pid_t GetLinuxTid() {
  return syscall(SYS_gettid);
}

#define CHI_WORKER_IS_REMOTE BIT_OPT(u32, 0)
#define CHI_WORKER_GROUP_AVAIL BIT_OPT(u32, 1)
#define CHI_WORKER_SHOULD_RUN BIT_OPT(u32, 2)
#define CHI_WORKER_IS_FLUSHING BIT_OPT(u32, 3)
#define CHI_WORKER_LONG_RUNNING BIT_OPT(u32, 4)

namespace chi {

#define WORKER_CONTINUOUS_POLLING BIT_OPT(u32, 0)
#define WORKER_LOW_LATENCY BIT_OPT(u32, 1)
#define WORKER_HIGH_LATENCY BIT_OPT(u32, 2)

/** Uniquely identify a queue lane */
struct WorkEntry {
  u32 prio_;
  ContainerId container_id_;
  ingress::Lane *lane_;
  ingress::LaneGroup *group_;
  ingress::MultiQueue *queue_;

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry() = default;

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(u32 prio, LaneId lane_id, ingress::MultiQueue *queue)
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
    return std::hash<chi::ingress::MultiQueue*>{}(key.queue_) +
        std::hash<u32>{}(key.container_id_) + std::hash<u64>{}(key.prio_);
  }
};
}  // namespace std

namespace chi {

class WorkOrchestrator;

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
  inline static const int NUM_QUEUES = 6;

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
    queues_[CONSTRUCT].Init(CONSTRUCT, max_lanes * qdepth);
    queues_[LOW_LAT].Init(LOW_LAT, max_lanes * qdepth);
    queues_[HIGH_LAT].Init(HIGH_LAT, max_lanes * qdepth);
    queues_[LONG_RUNNING].Init(LONG_RUNNING, max_lanes * qdepth);
    queues_[FLUSH].Init(LONG_RUNNING, max_lanes * qdepth);
    blocked_.Init(max_lanes * qdepth);
    complete_ = std::make_unique<hshm::mpsc_queue<LPointer<Task>>>(
        max_lanes * qdepth);
    root_count_ = 0;
    max_root_count_ = max_lanes * pqdepth;
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

  hshm::mpsc_queue<LPointer<Task>>& GetCompletion() {
    return *complete_;
  }

  bool push(const PrivateTaskQueueEntry &entry);

  void erase(int queue_id) {
    queues_[queue_id].erase();
  }

  void erase(int queue_id, PrivateTaskQueueEntry&) {
  }

  void block(PrivateTaskQueueEntry &entry) {
    LPointer<Task> blocked_task = entry.task_;
    entry.block_count_ = (ssize_t)blocked_task->rctx_.block_count_;
    blocked_.emplace(entry, blocked_task->rctx_.pending_key_);
//    HILOG(kInfo, "(node {}) Blocking task {} with count {}",
//          CHI_RPC->node_id_, (void*)blocked_task.ptr_, entry.block_count_);
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
//    HILOG(kInfo, "(node {}) Unblocking task {} with count {}",
//          CHI_RPC->node_id_, (void*)blocked_task.ptr_, entry->block_count_);
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
  bitfield32_t flags_;  /**< Worker metadata flags */
  std::unordered_map<hshm::charbuf, TaskNode>
      group_map_;        /**< Determine if a task can be executed right now */
  hshm::charbuf group_;  /**< The current group */
  hshm::spsc_queue<void*> stacks_;  /**< Cache of stacks for tasks */
  int num_stacks_ = 256;  /**< Number of stacks */
  int stack_size_ = KILOBYTES(64);
  PrivateTaskMultiQueue
      active_;  /** Tasks pending to complete */
  CacheTimer cur_time_;  /**< The current timepoint */
  CacheTimer sample_time_;
  WorkPending flush_;    /**< Info needed for flushing ops */
  float load_ = 0;  /** Load (# of ingress queues) */
  size_t load_nsec_ = 0;      /** Load (nanoseconds) */
  Task *cur_task_ = nullptr;  /** Currently executing task */
  Lane *cur_lane_ = nullptr;  /** Currently executing lane */
  size_t iter_count_ = 0;   /** Number of iterations the worker has done */
  size_t work_done_ = 0;  /** Amount of work in done (seconds) */
  bool do_sampling_ = false;  /**< Whether or not to sample */
  size_t monitor_gap_;    /**< Distance between sampling phases */
  size_t monitor_window_;  /** Length of sampling phase */


 public:
  /**===============================================================
   * Initialize Worker
   * =============================================================== */

  /** Constructor */
  Worker(u32 id, int cpu_id, ABT_xstream &xstream);

  /**===============================================================
   * Run tasks
   * =============================================================== */

  /** Check if worker should be sampling */
  bool ShouldSample() {
    sample_time_.Wrap(cur_time_);
    if (!do_sampling_) {
      size_t window_time = sample_time_.GetNsecFromStart();
      if (window_time > monitor_gap_) {
        sample_time_.Pause();
        sample_time_.Resume();
        do_sampling_ = true;
      }
    } else {
      size_t sample_time = sample_time_.GetNsecFromStart();
      if (sample_time > monitor_window_) {
        sample_time_.Pause();
        sample_time_.Resume();
        do_sampling_ = false;
      }
    }
    return do_sampling_;
  }

  /** Worker entrypoint */
  static void WorkerEntryPoint(void *arg);

  /** Flush the worker's tasks */
  void BeginFlush(WorkOrchestrator *orch);

  /** Check if work has been done */
  void EndFlush(WorkOrchestrator *orch);

  /** Check if any worker is still flushing */
  bool AnyFlushing(WorkOrchestrator *orch);

  /** Check if any worker did work */
  bool AnyFlushWorkDone(WorkOrchestrator *orch);

  /** Worker loop iteration */
  void Loop();

  /** Run a single iteration over all queues */
  void Run(bool flushing);

  /** Ingest all process lanes */
  HSHM_ALWAYS_INLINE
  void IngestProcLanes(bool flushing);

  /** Ingest all intermediate lanes */
  HSHM_ALWAYS_INLINE
  void IngestInterLanes(bool flushing);

  /** Ingest a lane */
  HSHM_ALWAYS_INLINE
  void IngestLane(WorkEntry &lane_info);

  /**
   * Detect if a DomainQuery is across nodes
   * */
  TaskRouteMode Reroute(const PoolId &scope,
                        DomainQuery &dom_query,
                        LPointer<Task> task,
                        ingress::Lane *lane);

  /** Process completion events */
  void ProcessCompletions();

  /** Poll the set of tasks in the private queue */
  HSHM_ALWAYS_INLINE
  size_t PollPrivateQueue(PrivateTaskQueue &queue, bool flushing);

  /** Run a task */
  bool RunTask(PrivateTaskQueue &queue,
               PrivateTaskQueueEntry &entry,
               LPointer<Task> task,
               bool flushing);

  /** Run an arbitrary task */
  HSHM_ALWAYS_INLINE
  void ExecTask(PrivateTaskQueue &queue,
                PrivateTaskQueueEntry &entry,
                Task *&task,
                RunContext &rctx,
                Container *&exec,
                bitfield32_t &props);

  /** Run a task */
  HSHM_ALWAYS_INLINE
  void ExecCoroutine(Task *&task, RunContext &rctx);

  /** Run a coroutine */
  static void CoroutineEntry(bctx::transfer_t t);

  /** Free a task when it is no longer needed */
  HSHM_ALWAYS_INLINE
  void EndTask(Container *exec, LPointer<Task> &task);

  /**===============================================================
   * Helpers
   * =============================================================== */

  /** Migrate a lane from this worker to another */
  void MigrateLane(Lane *lane, u32 new_worker);

  /** Unblock a task */
  static void SignalUnblock(Task *unblock_task);

  /** Externally signal a task as complete */
  HSHM_ALWAYS_INLINE
  static void SignalUnblock(LPointer<Task> &unblock_task);

  /** Get the characteristics of a task */
  HSHM_ALWAYS_INLINE
  bitfield32_t GetTaskProperties(Task *&task,
                                 bool flushing);

  /** Join worker */
  void Join();

  /** Get the pending queue for a worker */
  PrivateTaskMultiQueue& GetPendingQueue(Task *task);

  /** Tell worker to poll a set of queues */
  void PollQueues(const std::vector<WorkEntry> &queues);

  /** Actually poll the queues from within the worker */
  void _PollQueues();

  /**
   * Tell worker to start relinquishing some of its queues
   * This function must be called from a single thread (outside of worker)
   * */
  void RelinquishingQueues(const std::vector<WorkEntry> &queues);

  /** Actually relinquish the queues from within the worker */
  void _RelinquishQueues();

  /** Check if worker is still stealing queues */
  bool IsRelinquishingQueues();

  /** Set the sleep cycle */
  void SetPollingFrequency(size_t sleep_us);

  /** Enable continuous polling */
  void EnableContinuousPolling();

  /** Disable continuous polling */
  void DisableContinuousPolling();

  /** Check if continuously polling */
  bool IsContinuousPolling();

  /** Check if continuously polling */
  void SetHighLatency();

  /** Check if continuously polling */
  bool IsHighLatency();

  /** Check if continuously polling */
  void SetLowLatency();

  /** Check if continuously polling */
  bool IsLowLatency();

  /** Set the CPU affinity of this worker */
  void SetCpuAffinity(int cpu_id);

  /** Make maximum priority process */
  void MakeDedicated();

  /** Allocate a stack for a task */
  void* AllocateStack();

  /** Free a stack */
  void FreeStack(void *stack);
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H

