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

#ifndef HRUN_INCLUDE_HRUN_WORK_ORCHESTRATOR_WORKER_H_
#define HRUN_INCLUDE_HRUN_WORK_ORCHESTRATOR_WORKER_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"
#include "chimaera/task_registry/task_registry.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera/api/chimaera_runtime_.h"
#include <thread>
#include <queue>
#include "affinity.h"
#include "chimaera/network/rpc_thallium.h"

static inline pid_t GetLinuxTid() {
  return syscall(SYS_gettid);
}

#define HSHM_WORKER_IS_REMOTE BIT_OPT(u32, 0)
#define HSHM_WORKER_GROUP_AVAIL BIT_OPT(u32, 1)
#define HSHM_WORKER_SHOULD_RUN BIT_OPT(u32, 2)
#define HSHM_WORKER_IS_FLUSHING BIT_OPT(u32, 3)
#define HSHM_WORKER_LONG_RUNNING BIT_OPT(u32, 4)
#define HSHM_WORKER_IS_CONSTRUCT BIT_OPT(u32, 5)

namespace chm {

#define WORKER_CONTINUOUS_POLLING BIT_OPT(u32, 0)
#define WORKER_LOW_LATENCY BIT_OPT(u32, 1)
#define WORKER_HIGH_LATENCY BIT_OPT(u32, 2)

/** Uniquely identify a queue lane */
struct WorkEntry {
  u32 prio_;
  u32 lane_id_;
  Lane *lane_;
  LaneGroup *group_;
  MultiQueue *queue_;

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry() = default;

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(u32 prio, u32 lane_id, MultiQueue *queue)
  : prio_(prio), lane_id_(lane_id), queue_(queue) {
    group_ = &queue->GetGroup(prio);
    lane_ = &queue->GetLane(prio, lane_id);
  }

  /** Copy constructor */
  HSHM_ALWAYS_INLINE
  WorkEntry(const WorkEntry &other) {
    prio_ = other.prio_;
    lane_id_ = other.lane_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Copy assignment */
  HSHM_ALWAYS_INLINE
  WorkEntry& operator=(const WorkEntry &other) {
    if (this != &other) {
      prio_ = other.prio_;
      lane_id_ = other.lane_id_;
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
    lane_id_ = other.lane_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Move assignment */
  HSHM_ALWAYS_INLINE
  WorkEntry& operator=(WorkEntry &&other) noexcept {
    if (this != &other) {
      prio_ = other.prio_;
      lane_id_ = other.lane_id_;
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
    return queue_ == other.queue_ && lane_id_ == other.lane_id_ &&
        prio_ == other.prio_;
  }
};

}  // namespace chm

namespace std {
/** Hash function for WorkEntry */
template<>
struct hash<chm::WorkEntry> {
  HSHM_ALWAYS_INLINE
      std::size_t
  operator()(const chm::WorkEntry &key) const {
    return std::hash<chm::MultiQueue*>{}(key.queue_) +
        std::hash<u32>{}(key.lane_id_) + std::hash<u64>{}(key.prio_);
  }
};
}  // namespace std

namespace chm {

struct PrivateTaskQueueEntry {
 public:
  LPointer<Task> task_;
  WorkEntry *lane_info_;
  size_t next_, prior_;

 public:
  PrivateTaskQueueEntry() = default;
  PrivateTaskQueueEntry(const LPointer<Task> &task, WorkEntry *lane_info)
  : task_(task), lane_info_(lane_info) {}

  PrivateTaskQueueEntry(const PrivateTaskQueueEntry &other) {
    task_ = other.task_;
    lane_info_ = other.lane_info_;
    next_ = other.next_;
    prior_ = other.prior_;
  }

  PrivateTaskQueueEntry& operator=(const PrivateTaskQueueEntry &other) {
    if (this != &other) {
      task_ = other.task_;
      lane_info_ = other.lane_info_;
      next_ = other.next_;
      prior_ = other.prior_;
    }
    return *this;
  }

  PrivateTaskQueueEntry(PrivateTaskQueueEntry &&other) noexcept {
    task_ = other.task_;
    lane_info_ = other.lane_info_;
  }

  PrivateTaskQueueEntry& operator=(PrivateTaskQueueEntry &&other) noexcept {
    if (this != &other) {
      task_ = other.task_;
      lane_info_ = other.lane_info_;
    }
    return *this;
  }
};

class PrivateTaskSet {
 public:
  hshm::spsc_queue<size_t> keys_;
  std::vector<PrivateTaskQueueEntry> set_;

 public:
  void Init(size_t max_size) {
    keys_.Resize(max_size);
    set_.resize(max_size);
    for (size_t i = 0; i < max_size; ++i) {
      keys_.emplace(i);
    }
  }

  void resize() {
    size_t old_size = set_.size();
    size_t new_size = set_.size() * 2;
    keys_.Resize(new_size);
    for (size_t i = old_size; i < new_size; ++i) {
      keys_.emplace(i);
    }
    set_.resize(new_size);
  }

  void emplace(const PrivateTaskQueueEntry &entry, size_t &key) {
    if (keys_.pop(key).IsNull()) {
      resize();
      keys_.pop(key);
    }
    set_[key] = entry;
  }

  void peek(size_t key, PrivateTaskQueueEntry *&entry) {
    entry = &set_[key];
  }

  void pop(size_t key, PrivateTaskQueueEntry &entry) {
    entry = set_[key];
    erase(key);
  }

  void erase(size_t key) {
    keys_.emplace(key);
  }
};

class PrivateTaskQueue {
 public:
  PrivateTaskSet queue_;
  size_t size_, head_, tail_;
  int id_;

 public:
  void Init(int id, size_t queue_depth) {
    queue_.Init(queue_depth);
    size_ = 0;
    tail_ = 0;
    head_ = 0;
    id_ = id;
  }

  HSHM_ALWAYS_INLINE
  bool push(const PrivateTaskQueueEntry &entry) {
    size_t key;
    queue_.emplace(entry, key);
    if (size_ == 0) {
      head_ = key;
      tail_ = key;
    } else {
      PrivateTaskQueueEntry *point;
      // Tail is entry's prior
      queue_.peek(key, point);
      point->prior_ = tail_;
      // Prior's next is entry
      queue_.peek(tail_, point);
      point->next_ = key;
      // Update tail
      tail_ = key;
    }
    ++size_;
    return true;
  }

  HSHM_ALWAYS_INLINE
  void peek(PrivateTaskQueueEntry *&entry, size_t off) {
    queue_.peek(off, entry);
  }

  HSHM_ALWAYS_INLINE
  void peek(PrivateTaskQueueEntry *&entry) {
    queue_.peek(head_, entry);
  }

  HSHM_ALWAYS_INLINE
  void pop(PrivateTaskQueueEntry &entry) {
    PrivateTaskQueueEntry *point;
    peek(point);
    entry = *point;
    erase();
  }

  HSHM_ALWAYS_INLINE
  void erase() {
    PrivateTaskQueueEntry *point;
    queue_.peek(head_, point);
    size_t head = point->next_;
    queue_.erase(head_);
    head_ = head;
    --size_;
  }
};

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

 public:
  void Init(size_t pqdepth, size_t qdepth, size_t max_lanes) {
    queues_[ROOT].Init(ROOT, max_lanes * qdepth);
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

  hshm::mpsc_queue<LPointer<Task>>& GetCompletion() {
    return *complete_;
  }

  template<bool WAS_PENDING=false>
  bool push(const PrivateTaskQueueEntry &entry) {
    Task *task = entry.task_.ptr_;
    if (task->task_state_ == CHM_ADMIN->id_ &&
        task->method_ == chm::Admin::Method::kCreateTaskState) {
      return GetConstruct().push(entry);
    } else if (task->IsFlush() && !task->IsRemote()) {
      return GetFlush().push(entry);
    } else if (task->IsLongRunning()) {
      return GetLongRunning().push(entry);
    } else if (task->task_node_.node_depth_ == 0) {
      if constexpr (!WAS_PENDING) {
        if (root_count_ == max_root_count_) {
          return false;
        }
      }
      bool ret = GetRoot().push(entry);
      if constexpr (!WAS_PENDING) {
        root_count_ += ret;
      }
      return ret;
    } else if (task->prio_ == TaskPrio::kLowLatency) {
      return GetLowLat().push(entry);
    } else {
      return GetHighLat().push(entry);
    }
  }

  void erase(int queue_id) {
    if (queue_id == ROOT) {
      --root_count_;
    }
    queues_[queue_id].erase();
  }

  void erase(int queue_id, PrivateTaskQueueEntry&) {
    if (queue_id == ROOT) {
      --root_count_;
    }
  }

  void block(int queue_id) {
    PrivateTaskQueueEntry entry;
    queues_[queue_id].pop(entry);
    blocked_.emplace(entry, entry.task_->ctx_.pending_key_);
  }

  void block(PrivateTaskQueueEntry &entry) {
    blocked_.emplace(entry, entry.task_->ctx_.pending_key_);
  }

  void signal_unblock(PrivateTaskMultiQueue &worker_pending,
                       LPointer<Task> &unblock_task) {
    worker_pending.GetCompletion().emplace(unblock_task);
  }

  bool unblock() {
    LPointer<Task> blocked_task;
    if (GetCompletion().pop(blocked_task).IsNull()) {
      return false;
    }
    PrivateTaskQueueEntry entry;
    blocked_.pop(blocked_task->ctx_.pending_key_, entry);
    if (blocked_task.ptr_ != entry.task_.ptr_) {
      HILOG(kFatal, "A blocked task was lost");
    }
    blocked_task->UnsetBlocked();
    push<true>(entry);
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
  std::unordered_map<TaskStateId, TaskState*>
      state_map_;       /**< The set of task states */
  hshm::charbuf group_;  /**< The current group */
  WorkPending flush_;    /**< Info needed for flushing ops */
  hshm::spsc_queue<void*> stacks_;  /**< Cache of stacks for tasks */
  int num_stacks_ = 256;  /**< Number of stacks */
  int stack_size_ = KILOBYTES(64);
  PrivateTaskMultiQueue
      active_;  /** Tasks pending to complete */
  hshm::Timepoint cur_time_;  /**< The current timepoint */

 public:
  /**===============================================================
   * Initialize Worker and Change Utilization
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
    // TODO(llogan): implement reserve for group
    group_.resize(512);
    group_.resize(0);
    stacks_.Resize(num_stacks_);
    for (int i = 0; i < 16; ++i) {
      stacks_.emplace(malloc(stack_size_));
    }
    // MAX_DEPTH * [LOW_LAT, LONG_LAT]
    config::QueueManagerInfo &qm = HRUN_QM_RUNTIME->config_->queue_manager_;
    active_.Init(qm.proc_queue_depth_, qm.queue_depth_, qm.max_lanes_);
    cur_time_.Now();

    // Spawn threads
    xstream_ = xstream;
    // thread_ = std::make_unique<std::thread>(&Worker::Loop, this);
    // pthread_id_ = thread_->native_handle();
    tl_thread_ = HRUN_WORK_ORCHESTRATOR->SpawnAsyncThread(
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

  /** Join worker */
  void Join() {
    // thread_->join();
    ABT_xstream_join(xstream_);
  }

  /** Get the pending queue for a worker */
  PrivateTaskMultiQueue& GetPendingQueue(Task *task) {
    PrivateTaskMultiQueue &pending =
        HRUN_WORK_ORCHESTRATOR->workers_[task->ctx_.worker_id_]->active_;
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
        if (entry.queue_->id_ == HRUN_QM_RUNTIME->process_queue_id_) {
          HILOG(kDebug, "Worker {}: Scheduled queue {} (lane {}, prio {}) as a proc queue",
                id_, entry.queue_->id_, entry.lane_id_, entry.prio_);
          work_proc_queue_.emplace_back(entry);
        } else {
          HILOG(kDebug, "Worker {}: Scheduled queue {} (lane {}, prio {}) as an inter queue",
                id_, entry.queue_->id_, entry.lane_id_, entry.prio_);
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
    HILOG(kInfo, "Affining worker {} (pid={}) to {}", id_, pid_, cpu_id);
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
    // Reset flushing counters
    flush_.count_ = 0;
    flush_.flushing_ = true;
  }

  /** Check if work has been done */
  void EndFlush(WorkOrchestrator *orch) {
    // Keep flushing until count is 0
    if (flush_.count_ > 0) {
      flush_.work_done_ += flush_.count_;
      return;
    }
    flush_.flushing_ = false;

    // Check if all workers have finished flushing
    if (active_.GetFlush().size_ > 0) {
      size_t work_done = 0;
      size_t consensus = 0;
      for (std::unique_ptr<Worker> &worker : orch->workers_) {
        if (!worker->flush_.flushing_) {
          work_done += flush_.work_done_;
          if (worker->flush_.work_done_ == 0) {
            ++consensus;
          }
          flush_.work_done_ = 0;
        }
      }
      if (consensus == orch->workers_.size()) {
        PrivateTaskQueue &queue = active_.GetFlush();
        PollPrivateQueue(queue, false);
      } else {
        for (std::unique_ptr<Worker> &worker : orch->workers_) {
          UpdateFlushCount(*worker, work_done);
          worker->flush_.flushing_ = true;
        }
      }
    }
  }

  /** Update work count for flush tasks */
  void UpdateFlushCount(Worker &worker, size_t count) {
    PrivateTaskQueue &queue = worker.active_.GetFlush();
    for (size_t i = 0; i < queue.size_; ++i) {
        PrivateTaskQueueEntry *entry;
        queue.peek(entry, queue.head_ + i);
        chm::Admin::FlushTask *flush_task =
            (chm::Admin::FlushTask *)entry->task_.ptr_;
        flush_task->work_done_ += count;
    }
  }

  /** Worker loop iteration */
  void Loop() {
    pid_ = GetLinuxTid();
    SetCpuAffinity(affinity_);
    if (IsContinuousPolling()) {
      MakeDedicated();
    }
    WorkOrchestrator *orch = HRUN_WORK_ORCHESTRATOR;
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
        HELOG(kError, "(node {}) Worker {} caught an error: {}", HRUN_CLIENT->node_id_, id_, e.what());
      } catch (std::exception &e) {
        HELOG(kError, "(node {}) Worker {} caught an exception: {}", HRUN_CLIENT->node_id_, id_, e.what());
      } catch (...) {
        HELOG(kError, "(node {}) Worker {} caught an unknown exception", HRUN_CLIENT->node_id_, id_);
      }
      if (!IsContinuousPolling()) {
        Yield();
      }
    }
    HILOG(kInfo, "(node {}) Worker {} wrapping up",
          HRUN_CLIENT->node_id_, id_);
    Run(true);
    HILOG(kInfo, "(node {}) Worker {} has exited",
          HRUN_CLIENT->node_id_, id_);
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
      IngestLane<0>(work_entry);
    }
  }

  /** Ingest all intermediate lanes */
  HSHM_ALWAYS_INLINE
  void IngestInterLanes(bool flushing) {
    for (WorkEntry &work_entry : work_inter_queue_) {
      IngestLane<1>(work_entry);
    }
  }

  /** Ingest a lane */
  template<int TYPE>
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
      task.ptr_ = HRUN_CLIENT->GetMainPointer<Task>(entry->p_);
      bool is_remote = task->domain_id_.IsRemote(
          HRUN_RPC->GetNumHosts(), HRUN_CLIENT->node_id_);
#ifdef CHIMAERA_REMOTE_DEBUG
      if (task->task_state_ != HRUN_QM_CLIENT->admin_task_state_ &&
            !task->task_flags_.Any(TASK_REMOTE_DEBUG_MARK) &&
            task->method_ != TaskMethod::kCreate &&
            HRUN_RUNTIME->remote_created_) {
          is_remote = true;
        }
#endif
      if (is_remote) {
        task->SetRemote();
      }
      if (active_.push(PrivateTaskQueueEntry{task, &lane_info})) {
        lane->pop();
      } else {
        break;
      }
    }
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
      bool pushback = RunTask(queue, entry,
              *entry.lane_info_,
              entry.task_,
              entry.lane_info_->lane_id_,
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
  HSHM_ALWAYS_INLINE
  bool RunTask(PrivateTaskQueue &queue,
               PrivateTaskQueueEntry &entry,
               WorkEntry &lane_info,
               LPointer<Task> task,
               u32 lane_id,
               bool flushing) {
    // Get the task state
    TaskState *exec = GetTaskState(task->task_state_);
    if (!exec) {
      if (task->task_state_ == TaskStateId::GetNull()) {
        HELOG(kFatal, "(node {}) Task {} has no task state",
              HRUN_CLIENT->node_id_, task->task_node_);
        task->SetModuleComplete();
        return false;
      } else {
        HELOG(kWarning, "(node {}) Could not find the task state {} for task {}",
              HRUN_CLIENT->node_id_, task->task_state_, task->task_node_);
      }
      return true;
    }
    // Pack runtime context
    RunContext &rctx = task->ctx_;
    rctx.worker_id_ = id_;
    rctx.flush_ = &flush_;
    rctx.exec_ = exec;
    // Get task properties
    bitfield32_t props =
        GetTaskProperties(task.ptr_, exec, cur_time_,
                          lane_id, flushing);
    // Execute the task based on its properties
    if (!task->IsModuleComplete()) {
      ExecTask(lane_info, task.ptr_, rctx, exec, props);
    }
    // Cleanup allocations
    bool pushback = true;
    if (task->IsModuleComplete()) {
      pushback = false;
      // active_.erase(queue.id_);
      active_.erase(queue.id_, entry);
      if (props.Any(HSHM_WORKER_IS_CONSTRUCT)) {
        TaskStateId id = ((chm::Admin::CreateTaskStateTask*)task.ptr_)->id_;
        exec = GetTaskState(id);
      }
      EndTask(exec, task);
    } else if (task->IsBlocked()) {
      pushback = false;
      // active_.block(queue.id_);
      active_.block(entry);
    }
    return pushback;
  }

  /** Externally signal a task as complete */
  HSHM_ALWAYS_INLINE
  void SignalUnblock(Task *unblock_task) {
    LPointer<Task> ltask;
    ltask.ptr_ = unblock_task;
    ltask.shm_ = HERMES_MEMORY_MANAGER->Convert(ltask.ptr_);
    SignalUnblock(ltask);
  }

  /** Externally signal a task as complete */
  HSHM_ALWAYS_INLINE
  void SignalUnblock(LPointer<Task> &unblock_task) {
    PrivateTaskMultiQueue &pending = GetPendingQueue(unblock_task.ptr_);
    pending.signal_unblock(pending, unblock_task);
  }

  /** Free a task when it is no longer needed */
  HSHM_ALWAYS_INLINE
  void EndTask(TaskState *exec, LPointer<Task> &task) {
    if (task->ShouldSignalUnblock()) {
      SignalUnblock(task->ctx_.pending_to_);
    } else if (task->ShouldSignalRemoteComplete()) {
      TaskState *remote_exec = GetTaskState(HRUN_REMOTE_QUEUE->id_);
      remote_exec->Run(chm::remote_queue::Method::kServerPushComplete,
                       task.ptr_, task->ctx_);
    }
    if (exec && task->IsFireAndForget()) {
      exec->Del(task->method_, task.ptr_);
    } else {
      task->SetComplete();
    }
  }

  /** Get the characteristics of a task */
  HSHM_ALWAYS_INLINE
  bitfield32_t GetTaskProperties(Task *&task,
                                 TaskState *&exec,
                                 hshm::Timepoint &cur_time,
                                 u32 lane_id,
                                 bool flushing) {
    bitfield32_t props;

    bool group_avail = true;
    bool should_run = task->ShouldRun(cur_time, flushing);
    if (task->method_ == TaskMethod::kCreate) {
      props.SetBits(HSHM_WORKER_IS_CONSTRUCT);
    }
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

  /** Run an arbitrary task */
  HSHM_ALWAYS_INLINE
  void ExecTask(WorkEntry &lane_info,
                Task *&task,
                RunContext &rctx,
                TaskState *&exec,
                bitfield32_t &props) {
    // Determine if a task should be executed
    if (!props.All(HSHM_WORKER_SHOULD_RUN)) {
      return;
    }
    // Flush tasks
    if (props.Any(HSHM_WORKER_IS_FLUSHING)) {
      if (task->IsLongRunning()) {
        exec->Monitor(MonitorMode::kFlushStat, task, rctx);
      } else {
        flush_.count_ += 1;
      }
    }
    // Monitoring callback
    if (!task->IsStarted()) {
      exec->Monitor(MonitorMode::kBeginTrainTime, task, rctx);
    }
    // Attempt to run the task if it's ready and runnable
    if (props.Any(HSHM_WORKER_IS_REMOTE)) {
      TaskState *remote_exec = GetTaskState(HRUN_REMOTE_QUEUE->id_);
      remote_exec->Run(chm::remote_queue::Method::kClientPushSubmit,
                       task, rctx);
    } else if (task->IsLaneAll()) {
      //      TaskState *remote_exec = GetTaskState(HRUN_REMOTE_QUEUE->id_);
      //      remote_exec->Run(chm::remote_queue::Method::kPush,
      //                       task, rctx);
      //      task->SetBlocked();
    } else if (task->IsCoroutine()) {
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
    RunContext &rctx = task->ctx_;
    TaskState *&exec = rctx.exec_;
    rctx.jmp_ = t;
    exec->Run(task->method_, task, rctx);
    task->UnsetStarted();
    task->Yield<TASK_YIELD_CO>();
  }

  /**===============================================================
   * Task Ordering and Completion
   * =============================================================== */

  /** Get task state */
  HSHM_ALWAYS_INLINE
  TaskState* GetTaskState(const TaskStateId &state_id) {
    auto it = state_map_.find(state_id);
    if (it == state_map_.end()) {
      TaskState *state = HRUN_TASK_REGISTRY->GetTaskState(state_id);
      if (state == nullptr) {
        return nullptr;
      }
      state_map_.emplace(state_id, state);
      return state_map_[state_id];
    }
    return it->second;
  }
};

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_WORK_ORCHESTRATOR_WORKER_H_
