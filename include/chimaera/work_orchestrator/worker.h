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

#include <queue>
#include <thread>

#include "affinity.h"
#include "chimaera/chimaera_types.h"
#include "chimaera/module_registry/module_registry.h"
#include "chimaera/network/rpc_thallium.h"
#include "chimaera/queue_manager/queue_manager_runtime.h"

static inline pid_t GetLinuxTid() { return syscall(SYS_gettid); }

#define CHI_WORKER_SHOULD_RUN BIT_OPT(u32, 1)
#define CHI_WORKER_IS_FLUSHING BIT_OPT(u32, 2)

namespace chi {

#define WORKER_CONTINUOUS_POLLING BIT_OPT(u32, 0)
#define WORKER_LOW_LATENCY BIT_OPT(u32, 1)
#define WORKER_HIGH_LATENCY BIT_OPT(u32, 2)

/** Uniquely identify a queue lane */
struct IngressEntry {
  u32 prio_;
  ContainerId container_id_;
  ingress::Lane *lane_;
  ingress::LaneGroup *group_;
  ingress::MultiQueue *queue_;

  /** Default constructor */
  HSHM_INLINE
  IngressEntry() = default;

  /** Emplace constructor */
  HSHM_INLINE
  IngressEntry(u32 prio, LaneId lane_id, ingress::MultiQueue *queue)
      : prio_(prio), container_id_(lane_id), queue_(queue) {
    group_ = &queue->GetGroup(prio);
    lane_ = &queue->GetLane(prio, lane_id);
  }

  /** Copy constructor */
  HSHM_INLINE
  IngressEntry(const IngressEntry &other) {
    prio_ = other.prio_;
    container_id_ = other.container_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Copy assignment */
  HSHM_INLINE
  IngressEntry &operator=(const IngressEntry &other) {
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
  HSHM_INLINE
  IngressEntry(IngressEntry &&other) noexcept {
    prio_ = other.prio_;
    container_id_ = other.container_id_;
    lane_ = other.lane_;
    group_ = other.group_;
    queue_ = other.queue_;
  }

  /** Move assignment */
  HSHM_INLINE
  IngressEntry &operator=(IngressEntry &&other) noexcept {
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
  HSHM_INLINE bool IsNull() const {
    return queue_->IsNull();
  }

  /** Equality operator */
  HSHM_INLINE
  bool operator==(const IngressEntry &other) const {
    return queue_ == other.queue_ && container_id_ == other.container_id_ &&
           prio_ == other.prio_;
  }
};

}  // namespace chi

namespace std {
/** Hash function for IngressEntry */
template <>
struct hash<chi::IngressEntry> {
  HSHM_INLINE
  std::size_t operator()(const chi::IngressEntry &key) const {
    return std::hash<chi::ingress::MultiQueue *>{}(key.queue_) +
           std::hash<u32>{}(key.container_id_) + std::hash<u64>{}(key.prio_);
  }
};
}  // namespace std

namespace chi {

class WorkOrchestrator;

typedef chi::mpsc_ptr_queue<TaskPointer> PrivateTaskQueue;
typedef chi::mpmc_lifo_list_queue<chi::Lane> PrivateLaneQueue;

class PrivateLaneMultiQueue {
 public:
  PrivateLaneQueue active_[2];

 public:
  void request(chi::Lane *lane) { active_[lane->prio_].push(lane); }

  void resize(size_t new_depth) {
    // active_[0].resize(new_depth);
    // active_[1].resize(new_depth);
  }

  PrivateLaneQueue &GetLowLatency() { return active_[TaskPrio::kLowLatency]; }
  PrivateLaneQueue &GetHighLatency() { return active_[TaskPrio::kHighLatency]; }
};

class PrivateTaskMultiQueue {
 public:
  CLS_CONST int FLUSH = 1;
  CLS_CONST int FAIL = 2;
  CLS_CONST int REMAP = 3;
  CLS_CONST int NUM_QUEUES = 4;

 public:
  PrivateTaskQueue queues_[NUM_QUEUES];
  PrivateLaneMultiQueue active_lanes_;
  size_t id_;

 public:
  void Init(size_t id, size_t pqdepth, size_t qdepth, size_t max_lanes) {
    id_ = id;
    queues_[FLUSH].resize(max_lanes * qdepth);
    queues_[FAIL].resize(max_lanes * qdepth);
    queues_[REMAP].resize(max_lanes * qdepth);
    // TODO(llogan): Don't hardcode lane queue depth
    active_lanes_.resize(8192);
  }

  PrivateLaneQueue &GetLowLatency() { return active_lanes_.GetLowLatency(); }

  PrivateLaneQueue &GetHighLatency() { return active_lanes_.GetHighLatency(); }

  void request(chi::Lane *lane) { active_lanes_.request(lane); }

  PrivateTaskQueue &GetRemap() { return queues_[REMAP]; }

  PrivateTaskQueue &GetFail() { return queues_[FAIL]; }

  PrivateTaskQueue &GetFlush() { return queues_[FLUSH]; }

  bool push(const TaskPointer &entry);

  template <typename TaskT>
  bool push(const FullPtr<TaskT> &task) {
    return push(task.template Cast<Task>());
  }
};

class Worker {
 public:
  WorkerId id_; /**< Unique identifier of this worker */
  // std::unique_ptr<std::thread> thread_;  /**< The worker thread handle */
  // int pthread_id_;      /**< The worker pthread handle */
  ABT_thread tl_thread_; /**< The worker argobots thread handle */
  std::atomic<int> pid_; /**< The worker process id */
  int affinity_;         /**< The worker CPU affinity */
  u32 numa_node_;        // TODO(llogan): track NUMA affinity
  ABT_xstream xstream_;
  std::vector<IngressEntry> work_proc_queue_; /**< The set of queues to poll */
  size_t sleep_us_;    /**< Time the worker should sleep after a run */
  bitfield32_t flags_; /**< Worker metadata flags */
  std::unordered_map<hshm::charwrap, TaskNode>
      group_map_;       /**< Determine if a task can be executed right now */
  chi::charwrap group_; /**< The current group */
  chi::iqueue<hipc::iqueue_entry> stacks_; /**< Cache of stacks for tasks */
  int num_stacks_ = 256;                   /**< Number of stacks */
  int stack_size_ = KILOBYTES(64);
  PrivateTaskMultiQueue active_; /** Tasks pending to complete */
  CacheTimer cur_time_;          /**< The current timepoint */
  CacheTimer sample_time_;
  WorkPending flush_;        /**< Info needed for flushing ops */
  float load_ = 0;           /** Load (# of ingress queues) */
  size_t load_nsec_ = 0;     /** Load (nanoseconds) */
  Task *cur_task_ = nullptr; /** Currently executing task */
  Lane *cur_lane_ = nullptr; /** Currently executing lane */
  size_t iter_count_ = 0;    /** Number of iterations the worker has done */
  size_t work_done_ = 0;     /** Amount of work in done (seconds) */
  bool do_sampling_ = false; /**< Whether or not to sample */
  size_t monitor_gap_;       /**< Distance between sampling phases */
  size_t monitor_window_;    /** Length of sampling phase */

 public:
  /**===============================================================
   * Initialize Worker
   * =============================================================== */

  /** Constructor */
  Worker(WorkerId id, int cpu_id, ABT_xstream xstream);

  /** Spawn */
  void Spawn();

  /** Request a lane */
  void RequestLane(chi::Lane *lane) { active_.request(lane); }

  /** Get remap */
  PrivateTaskQueue &GetRemap() { return active_.GetRemap(); }

  /**===============================================================
   * Run tasks
   * =============================================================== */

  /** Check if worker should be sampling */
  bool ShouldSample() {
    return false;
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
  HSHM_INLINE
  void IngestProcLanes(bool flushing);

  /** Ingest a lane */
  HSHM_INLINE
  void IngestLane(IngressEntry &lane_info);

  /** Poll the set of tasks in the private queue */
  template <bool FROM_FLUSH>
  HSHM_INLINE void PollTempQueue(PrivateTaskQueue &queue, bool flushing);

  /** Poll the set of tasks in the private queue */
  HSHM_INLINE
  size_t PollPrivateLaneMultiQueue(PrivateLaneQueue &queue, bool flushing);

  /** Run a task */
  bool RunTask(FullPtr<Task> &task, bool flushing);

  /** Run an arbitrary task */
  HSHM_INLINE
  void ExecTask(FullPtr<Task> &task, RunContext &rctx, Container *&exec,
                bitfield32_t &props);

  /** Run a task */
  HSHM_INLINE
  void ExecCoroutine(Task *&task, RunContext &rctx);

  /** Run a coroutine */
  static void CoroutineEntry(bctx::transfer_t t);

  /** Free a task when it is no longer needed */
  HSHM_INLINE
  void EndTask(Container *exec, FullPtr<Task> task, RunContext &rctx);

  /**===============================================================
   * Helpers
   * =============================================================== */

  /** Migrate a lane from this worker to another */
  void MigrateLane(Lane *lane, u32 new_worker);

  /** Get the characteristics of a task */
  HSHM_INLINE
  bitfield32_t GetTaskProperties(Task *&task, bool flushing);

  /** Join worker */
  void Join();

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
  void *AllocateStack();

  /** Free a stack */
  void FreeStack(void *stack);
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_WORKER_H
