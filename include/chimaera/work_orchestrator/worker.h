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

 public:
  PrivateTaskQueueEntry() = default;
  PrivateTaskQueueEntry(const LPointer<Task> &task, WorkEntry *lane_info)
  : task_(task), lane_info_(lane_info) {}

  PrivateTaskQueueEntry(const PrivateTaskQueueEntry &other) {
    task_ = other.task_;
    lane_info_ = other.lane_info_;
  }

  PrivateTaskQueueEntry& operator=(const PrivateTaskQueueEntry &other) {
    if (this != &other) {
      task_ = other.task_;
      lane_info_ = other.lane_info_;
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

class PrivateTaskQueue {
 public:
  std::vector<PrivateTaskQueueEntry> queue_;
  size_t size_, head_, tail_;
  int id_;

 public:
  void Init(int id, size_t queue_depth) {
    queue_.resize(queue_depth);
    size_ = 0;
    tail_ = 0;
    head_ = 0;
    id_ = id;
  }

  HSHM_ALWAYS_INLINE
  bool push(const PrivateTaskQueueEntry &entry) {
    size_t off;
    return push(entry, off);
  }

  HSHM_ALWAYS_INLINE
  bool push(const PrivateTaskQueueEntry &entry, size_t &off) {
    size_t diff = tail_ - head_;
    if (diff >= queue_.size()) {
      return false;
    }
    queue_[tail_ % queue_.size()] = entry;
    off = tail_;
    ++size_;
    ++tail_;
    return true;
  }

  HSHM_ALWAYS_INLINE
  void peek(size_t off, PrivateTaskQueueEntry &entry) {
    entry = queue_[off % queue_.size()];
  }

  HSHM_ALWAYS_INLINE
  void pop(size_t off, PrivateTaskQueueEntry &entry) {
    peek(off, entry);
    erase(off);
  }

  HSHM_ALWAYS_INLINE
  void erase(size_t off) {
    queue_[off % queue_.size()].task_.ptr_ = nullptr;
    --size_;
    _correct_head();
  }

  HSHM_ALWAYS_INLINE
  void _correct_head() {
    for (; head_ < tail_; ++head_) {
      if (queue_[head_ % queue_.size()].task_.ptr_) {
        break;
      }
    }
  }
};

class PrivateTaskMultiQueue {
 public:
  inline static const int ROOT = 0;
  inline static const int LOW_LAT = 1;
  inline static const int HIGH_LAT = 2;
  inline static const int LONG_RUNNING = 3;
  inline static const int PENDING = 4;
  inline static const int NUM_QUEUES = 5;

 public:
  size_t root_count_;
  size_t max_root_count_;
  PrivateTaskQueue queues_[NUM_QUEUES];
  hipc::uptr<mpsc_queue<LPointer<Task>>> complete_u_;
  mpsc_queue<LPointer<Task>> *complete_;

 public:
  void Init(size_t pqdepth, size_t qdepth, size_t max_lanes) {
    queues_[ROOT].Init(ROOT, max_lanes * qdepth);
    queues_[LOW_LAT].Init(LOW_LAT, max_lanes * qdepth);
    queues_[HIGH_LAT].Init(HIGH_LAT, max_lanes * qdepth);
    queues_[LONG_RUNNING].Init(LONG_RUNNING, max_lanes * qdepth);
    queues_[PENDING].Init(PENDING, PENDING * max_lanes * qdepth);
    complete_u_ = hipc::make_uptr<mpsc_queue<LPointer<Task>>>(
        PENDING * max_lanes * qdepth);
    complete_ = complete_u_.get();
    root_count_ = 0;
    max_root_count_ = max_lanes * pqdepth;
  }

  PrivateTaskQueue& GetRoot() {
    return queues_[ROOT];
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

  PrivateTaskQueue& GetPending() {
    return queues_[PENDING];
  }

  mpsc_queue<LPointer<Task>>& GetCompletion() {
    return *complete_;
  }

  template<bool WAS_PENDING=false>
  bool push(const PrivateTaskQueueEntry &entry) {
    Task *task = entry.task_.ptr_;
    if (task->IsLongRunning()) {
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

  void erase(int queue_id, size_t off) {
    if (queue_id == ROOT) {
      --root_count_;
    }
    queues_[queue_id].erase(off);
  }

  void push_pending(int queue_id, size_t off) {
    PrivateTaskQueueEntry entry;
    queues_[queue_id].pop(off, entry);
    GetPending().push(entry, entry.task_->ctx_.pending_key_);
  }

  void signal_complete(PrivateTaskMultiQueue &worker_pending,
                       LPointer<Task> &done_task) {
    worker_pending.GetCompletion().emplace(done_task);
  }

  bool process_complete() {
    LPointer<Task> done_task;
    if (GetCompletion().pop(done_task).IsNull()) {
      return false;
    }
    PrivateTaskQueueEntry entry;
    Task *pending = (Task*)done_task->ctx_.pending_to_;
    GetPending().pop(pending->ctx_.pending_key_, entry);
    if (entry.task_.ptr_ != pending) {
      return true;
    }
    pending->UnsetBlocked();
    push<true>(entry);
    return true;
  }
};

class Worker {
 public:
  u32 id_;  /**< Unique identifier of this worker */
  std::unique_ptr<std::thread> thread_;  /**< The worker thread handle */
  ABT_thread tl_thread_;  /**< The worker argobots thread handle */
  int pthread_id_;      /**< The worker pthread handle */
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
      pending_;  /** Tasks pending to complete */
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
    pending_.Init(qm.proc_queue_depth_, qm.queue_depth_, qm.max_lanes_);
    cur_time_.Now();

    // Spawn threads
    xstream_ = xstream;
    thread_ = std::make_unique<std::thread>(&Worker::Loop, this);
    pthread_id_ = thread_->native_handle();
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
    pthread_id_ = GetLinuxTid();
    // TODO(llogan): implement reserve for group
    group_.resize(512);
    group_.resize(0);
  }

  /** Get the pending queue for a worker */
  PrivateTaskMultiQueue& GetPendingQueue(Task *task) {
    PrivateTaskMultiQueue &pending =
        HRUN_WORK_ORCHESTRATOR->workers_[task->ctx_.worker_id_]->pending_;
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
        //
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
    ProcessAffiner::SetCpuAffinity(pid_, affinity_);
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

  /** Worker loop iteration */
  void Loop() {
    pid_ = GetLinuxTid();
    SetCpuAffinity(affinity_);
    if (IsContinuousPolling()) {
      MakeDedicated();
    }
    WorkOrchestrator *orchestrator = HRUN_WORK_ORCHESTRATOR;
    cur_time_.Now();
    size_t work = 0;
    while (orchestrator->IsAlive()) {
      try {
        bool flushing = flush_.flushing_;
        work += Run(flushing);
        ++work;
        if (work >= 10000) {
          work = 0;
          cur_time_.Now();
        }
        if (flushing) {
          flush_.flushing_ = false;
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
    HILOG(kInfo, "Orchestrator is dying");
    Run(true);
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
    work += PollPrivateQueue(pending_.GetRoot(), flushing);
    for (size_t i = 0; i < 8192; ++i) {
      size_t diff = 0;
      IngestInterLanes(flushing);
      diff += PollPrivateQueue(pending_.GetLowLat(), flushing);
      if (diff == 0) {
        break;
      }
      work += diff;
    }
    work += PollPrivateQueue(pending_.GetHighLat(), flushing);
    PollPrivateQueue(pending_.GetLongRunning(), flushing);
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
            task->method_ != TaskMethod::kConstruct &&
            HRUN_RUNTIME->remote_created_) {
          is_remote = true;
        }
#endif
      if (is_remote) {
        task->SetRemote();
      }
      if (pending_.push(PrivateTaskQueueEntry{task, &lane_info})) {
        lane->pop();
      } else {
        break;
      }
    }
  }

  /** Process completion events */
  void ProcessCompletions() {
    while (pending_.process_complete());
  }

  /** Poll the set of tasks in the private queue */
  HSHM_ALWAYS_INLINE
  size_t PollPrivateQueue(PrivateTaskQueue &queue, bool flushing) {
    size_t work = 0;
    size_t tail = queue.tail_;
    for (size_t i = queue.head_; i < tail; ++i) {
      PrivateTaskQueueEntry entry;
      queue.peek(i, entry);
      if (entry.task_.ptr_ != nullptr) {
        RunTask(queue, entry, i,
                *entry.lane_info_,
                entry.task_,
                entry.lane_info_->lane_id_,
                flushing);
        ++work;
      }
    }
    ProcessCompletions();
    return work;
  }

  /** Run a task */
  HSHM_ALWAYS_INLINE
  TaskState* RunTask(PrivateTaskQueue &queue,
                     PrivateTaskQueueEntry &entry,
                     size_t queue_off,
                     WorkEntry &lane_info,
                     LPointer<Task> task,
                     u32 lane_id,
                     bool flushing) {
    // Get the task state
    TaskState *exec = GetTaskState(task->task_state_);
    if (!exec) {
      HELOG(kWarning, "(node {}) Could not find the task state: {}",
            HRUN_CLIENT->node_id_, task->task_state_);
      return exec;
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
    ExecTask(lane_info, task.ptr_, rctx, exec, props);
    // Cleanup allocations
    if (task->IsModuleComplete()) {
      RemoveTaskGroup(task.ptr_, exec,
                      lane_id,
                      props.Any(HSHM_WORKER_IS_REMOTE));
      pending_.erase(queue.id_, queue_off);
      EndTask(exec, task);
    } else if (task->IsBlocked()) {
      // pending_.push_pending(queue.id_, queue_off);
    }
    return exec;
  }

  /** Get the characteristics of a task */
  HSHM_ALWAYS_INLINE
  bitfield32_t GetTaskProperties(Task *&task,
                                 TaskState *&exec,
                                 hshm::Timepoint &cur_time,
                                 u32 lane_id,
                                 bool flushing) {
    bitfield32_t props;

    bool group_avail = CheckTaskGroup(task, exec,
                                      lane_id,
                                      task->task_node_,
                                      task->IsRemote());
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

  /** Run an arbitrary task */
  HSHM_ALWAYS_INLINE
  bool ExecTask(WorkEntry &lane_info,
                Task *&task,
                RunContext &rctx,
                TaskState *&exec,
                bitfield32_t &props) {
    // Determine if a task should be executed
    if (task->IsRunDisabled() ||
        !props.All(HSHM_WORKER_GROUP_AVAIL | HSHM_WORKER_SHOULD_RUN)) {
      return false;
    }
    // Flush tasks
    if (props.Any(HSHM_WORKER_IS_FLUSHING) && !task->IsFlush()) {
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
      auto ids = HRUN_RUNTIME->ResolveDomainId(
          task->domain_id_);
      HRUN_REMOTE_QUEUE->Disperse(task, exec, ids);
      task->SetDisableRun();
    } else if (task->IsLaneAll()) {
      HRUN_REMOTE_QUEUE->DisperseLocal(task, exec,
                                       lane_info.queue_,
                                       lane_info.group_);
      task->SetDisableRun();
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
    return !task->IsFlush();
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

  /** Check if two tasks can execute concurrently */
  HSHM_ALWAYS_INLINE
  bool CheckTaskGroup(Task *task, TaskState *exec,
                      u32 lane_id,
                      TaskNode node, const bool &is_remote) {
    if (is_remote || task->IsStarted() || task->IsLaneAll()) {
      return true;
    }
    int ret = exec->GetGroup(task->method_, task, group_);
    if (ret == TASK_UNORDERED || task->IsUnordered()) {
//      HILOG(kDebug, "(node {}) Task {} is unordered, so count remains 0 worker={}",
//            HRUN_CLIENT->node_id_, task->task_node_, id_);
      return true;
    }

    // Ensure that concurrent requests are not serialized
    LocalSerialize srl(group_, false);
    srl << lane_id;

    auto it = group_map_.find(group_);
    if (it == group_map_.end()) {
      node.node_depth_ = 1;
      group_map_.emplace(group_, node);
//      HILOG(kDebug, "(node {}) Increasing depth of group {} to {} (worker={})",
//            HRUN_CLIENT->node_id_, std::hash<hshm::charbuf>{}(group_), node.node_depth_, id_);
      return true;
    }
    TaskNode &node_cmp = it->second;
    if (node_cmp.root_ == node.root_) {
      node_cmp.node_depth_ += 1;
//      HILOG(kDebug, "(node {}) Increasing depth of group {} to {} (worker={})",
//            HRUN_CLIENT->node_id_, std::hash<hshm::charbuf>{}(group_), node.node_depth_, id_);
      return true;
    }
    return false;
  }

  /** No longer serialize tasks of the same group */
  HSHM_ALWAYS_INLINE
  void RemoveTaskGroup(Task *task, TaskState *exec,
                       u32 lane_id, const bool &is_remote) {
    if (is_remote || task->IsLaneAll()) {
      return;
    }
    int ret = exec->GetGroup(task->method_, task, group_);
    if (ret == TASK_UNORDERED || task->IsUnordered()) {
//      HILOG(kDebug, "(node {}) Decreasing depth of group remains 0 (task_node={} worker={})",
//            HRUN_CLIENT->node_id_, task->task_node_, id_);
      return;
    }

    // Ensure that concurrent requests are not serialized
    LocalSerialize srl(group_, false);
    srl << lane_id;

    TaskNode &node_cmp = group_map_[group_];
    if (node_cmp.node_depth_ == 0) {
      HELOG(kFatal, "(node {}) Group {} depth is already 0 (task_node={} worker={})",
            HRUN_CLIENT->node_id_, std::hash<hshm::charbuf>{}(group_), task->task_node_, id_);
    }
    node_cmp.node_depth_ -= 1;
//    HILOG(kDebug, "(node {}) Decreasing depth of to {} (task_node={} worker={})",
//          HRUN_CLIENT->node_id_, node_cmp.node_depth_, task->task_node_, id_);
    if (node_cmp.node_depth_ == 0) {
      group_map_.erase(group_);
    }
  }

  /** Free a task when it is no longer needed */
  HSHM_ALWAYS_INLINE
  void EndTask(TaskState *exec, LPointer<Task> &task) {
//    if (task->ShouldSignalComplete()) {
//      Task *pending_to = (Task*)task->ctx_.pending_to_;
//      pending_.signal_complete(GetPendingQueue(pending_to),
//                               task);
//    }
    if (exec && task->IsFireAndForget()) {
      exec->Del(task->method_, task.ptr_);
    } else {
      task->SetComplete();
    }
  }
};

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_WORK_ORCHESTRATOR_WORKER_H_
