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

#ifndef HRUN_INCLUDE_HRUN_QUEUE_MANAGER_REQUEST_H_
#define HRUN_INCLUDE_HRUN_QUEUE_MANAGER_REQUEST_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/network/local_serialize.h"
#include <csetjmp>
#include <thallium.hpp>

namespace chm {

class TaskLib;

/** This task reads a state */
#define TASK_READ BIT_OPT(u32, 0)
/** This task writes to a state */
#define TASK_WRITE BIT_OPT(u32, 1)
/** This task fundamentally updates a state */
#define TASK_UPDATE BIT_OPT(u32, 2)
/** This task is paused until a set of tasks complete */
#define TASK_BLOCKED BIT_OPT(u32, 3)
/** This task is latency-sensitive (deprecated) */
#define TASK_SIGNAL_REMOTE_COMPLETE BIT_OPT(u32, 4)
/** This task makes system calls and may hurt caching */
#define TASK_SYSCALL BIT_OPT(u32, 5)
/** This task supports merging */
#define TASK_MERGE BIT_OPT(u32, 6)
/** This task does not depend on its position in the queue */
#define TASK_UNORDERED BIT_OPT(u32, 7)
/** This task has began execution */
#define TASK_HAS_STARTED BIT_OPT(u32, 8)
/** This task is completed */
#define TASK_COMPLETE BIT_OPT(u32, 9)
/** This task was marked completed outside of the worker thread */
#define TASK_MODULE_COMPLETE BIT_OPT(u32, 10)
/** This task is long-running */
#define TASK_LONG_RUNNING (BIT_OPT(u32, 11) | TASK_UNORDERED)
/** This task is fire and forget. Free when completed */
#define TASK_FIRE_AND_FORGET BIT_OPT(u32, 12)
/** This task should not be run at this time (deprecated) */
#define TASK_DISABLE_RUN 0
/** This task owns the data in the task */
#define TASK_DATA_OWNER BIT_OPT(u32, 14)
/** This task uses co-routine wait */
#define TASK_COROUTINE BIT_OPT(u32, 15)
/** This task can be scheduled on any lane */
#define TASK_LANE_ANY BIT_OPT(u32, 18)
/** This task should be scheduled on all lanes */
#define TASK_LANE_ALL BIT_OPT(u32, 19)
/** This task flushes the runtime */
#define TASK_FLUSH BIT_OPT(u32, 20)
/** This task signals its completion */
#define TASK_SIGNAL_COMPLETE BIT_OPT(u32, 21)
/** This task is a remote task */
#define TASK_REMOTE BIT_OPT(u32, 22)
/** This task is apart of remote debugging */
#define TASK_REMOTE_DEBUG_MARK BIT_OPT(u32, 31)

/** Used to define task methods */
#define TASK_METHOD_T static inline const u32

/** Used to indicate Yield to use */
#define TASK_YIELD_STD 0
#define TASK_YIELD_CO 1
#define TASK_YIELD_NOCO 2
#define TASK_YIELD_ABT 3
#define TASK_YIELD_EMPTY 4

/** The baseline set of tasks */
struct TaskMethod {
  TASK_METHOD_T kCreate = 0;    /**< The constructor of the task */
  TASK_METHOD_T kDestruct = 1;  /**< The destructor of the task */
  TASK_METHOD_T kLast = 2;      /**< Where the next method should take place */
};

/**
 * Let's say we have an I/O request to a device
 * I/O requests + MD operations need to be controlled for correctness
 * Is there a case where root tasks from different TaskStates need to be ordered? No.
 * Tasks spawned from the same root task need to be keyed to the same worker stack
 * Tasks apart of the same task group need to be ordered
 * */

/** An identifier used for representing the location of a task in a task graph */
struct TaskNode {
  TaskId root_;         /**< The id of the root task */
  u32 node_depth_;      /**< The depth of the task in the task graph */

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  TaskNode() = default;

  /** Emplace constructor for root task */
  HSHM_ALWAYS_INLINE
  TaskNode(TaskId root) {
    root_ = root;
    node_depth_ = 0;
  }

  /** Copy constructor */
  HSHM_ALWAYS_INLINE
  TaskNode(const TaskNode &other) {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
  }

  /** Copy assignment operator */
  HSHM_ALWAYS_INLINE
  TaskNode& operator=(const TaskNode &other) {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
    return *this;
  }

  /** Move constructor */
  HSHM_ALWAYS_INLINE
  TaskNode(TaskNode &&other) noexcept {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
  }

  /** Move assignment operator */
  HSHM_ALWAYS_INLINE
  TaskNode& operator=(TaskNode &&other) noexcept {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
    return *this;
  }

  /** Addition operator*/
  HSHM_ALWAYS_INLINE
  TaskNode operator+(int i) const {
    TaskNode ret;
    ret.root_ = root_;
    ret.node_depth_ = node_depth_ + i;
    return ret;
  }

  /** Null task node */
  HSHM_ALWAYS_INLINE
  static TaskNode GetNull() {
    TaskNode ret;
    ret.root_ = TaskId::GetNull();
    ret.node_depth_ = 0;
    return ret;
  }

  /** Check if null */
  HSHM_ALWAYS_INLINE
  bool IsNull() const {
    return root_.IsNull();
  }

  /** Check if the root task */
  HSHM_ALWAYS_INLINE
  bool IsRoot() const {
    return node_depth_ == 0;
  }

  /** Serialization*/
  template<typename Ar>
  void serialize(Ar &ar) {
    ar(root_, node_depth_);
  }
};

/** Allow TaskNode to be printed as strings */
static inline std::ostream &operator<<(std::ostream &os, const TaskNode &obj) {
  return os << obj.root_ << "/" << std::to_string(obj.node_depth_);
}

/** This task uses SerializeStart */
#define TF_SRL_SYM_START BIT_OPT(u32, 0)
/** This task uses SaveStart + LoadStart */
#define TF_SRL_ASYM_START BIT_OPT(u32, 1)
/** This task uses SerializeEnd */
#define TF_SRL_SYM_END BIT_OPT(u32, 2)
/** This task uses SaveEnd + LoadEnd */
#define TF_SRL_ASYM_END BIT_OPT(u32, 3)
/** This task uses symmetric serialization */
#define TF_SRL_SYM (TF_SRL_SYM_START | TF_SRL_SYM_END)
/** This task uses asymmetric serialization */
#define TF_SRL_ASYM (TF_SRL_ASYM_START | TF_SRL_ASYM_END)
/** This task uses replication */
#define TF_REPLICA BIT_OPT(u32, 31)
/** This task is intended to be used only locally */
#define TF_LOCAL BIT_OPT(u32, 5)
/** This task supports monitoring of all sub-methods */
#define TF_MONITOR BIT_OPT(u32, 6)
/** This task has a CompareGroup function */
#define TF_CMPGRP BIT_OPT(u32, 7)

/** All tasks inherit this to easily check if a class is a task using SFINAE */
class IsTask {};
/** The type of a compile-time task flag */
#define TASK_FLAG_T constexpr inline static bool
/** Determine this is a task */
#define IS_TASK(T) \
  std::is_base_of_v<chm::IsTask, T>
/** Determine this task supports serialization */
#define IS_SRL(T) \
  T::SUPPORTS_SRL
/** Determine this task uses SerializeStart */
#define USES_SRL_START(T) \
  T::SRL_SYM_START
/** Determine this task uses SerializeEnd */
#define USES_SRL_END(T) \
  T::SRL_SYM_END

/** Compile-time flags indicating task methods and operation support */
template<u32 FLAGS>
struct TaskFlags : public IsTask {
 public:
  TASK_FLAG_T IS_LOCAL = FLAGS & TF_LOCAL;
  TASK_FLAG_T SUPPORTS_SRL = FLAGS & (TF_SRL_SYM | TF_SRL_ASYM);
  TASK_FLAG_T SRL_SYM_START = FLAGS & TF_SRL_SYM_START;
  TASK_FLAG_T SRL_SYM_END = FLAGS & TF_SRL_SYM_END;
  TASK_FLAG_T REPLICA = FLAGS & TF_REPLICA;
  TASK_FLAG_T MONITOR = FLAGS & TF_MONITOR;
  TASK_FLAG_T CMPGRP = FLAGS & TF_CMPGRP;
};

/** The type of a compile-time task flag */
#define TASK_PRIO_T constexpr inline static int

/** Prioritization of tasks */
class TaskPrio {
 public:
  TASK_PRIO_T kLowLatency = 0;         /**< Low latency task lane */
  TASK_PRIO_T kHighLatency = 1;        /**< High latency task lane */
};

/** Used to indicate the amount of work remaining to do when flushing */
struct WorkPending {
  size_t count_;
  size_t last_check_;
  size_t cur_iter_;
  size_t work_done_;
  bool flushing_;

  /** Default constructor */
  WorkPending()
  :  count_(0), last_check_(0), cur_iter_(0), work_done_(0), flushing_(false) {}
};

struct Task;

struct RemoteInfo {
  std::atomic<u32> rep_cnt_;
  u32 rep_max_;
  std::vector<LPointer<Task>> replicas_;
  DomainId ret_domain_;
  size_t task_addr_;
};

/** Context passed to the Run method of a task */
struct RunContext {
  u32 worker_id_;         /**< The worker id of the task */
  u32 lane_id_;           /**< The lane id of the task */
  bctx::transfer_t jmp_;  /**< Stack info for coroutines */
  void *stack_ptr_;       /**< Stack pointer (coroutine) */
  TaskLib *exec_;
  WorkPending *flush_;
  hshm::Timer timer_;
  Task *pending_to_;
  size_t pending_key_;
  RemoteInfo *next_net_;
  RemoteInfo *prior_net_;
  size_t task_addr_;
};

/** A generic task base class */
struct Task : public hipc::ShmContainer {
 SHM_CONTAINER_TEMPLATE((Task), (Task))
 public:
  TaskStateId task_state_;     /**< The unique name of a task state */
  TaskNode task_node_;         /**< The unique ID of this task in the graph */
  DomainId domain_id_;         /**< The nodes that the task should run on */
  u32 prio_;                   /**< An indication of the priority of the request */
  u32 lane_hash_;              /**< Determine the lane a task is keyed to */
  u32 method_;                 /**< The method to call in the state */
  bitfield32_t task_flags_;    /**< Properties of the task */
  double period_ns_;           /**< The period of the task */
  hshm::Timepoint start_;      /**< The time the task started */
  RunContext ctx_;
#ifdef TASK_DEBUG
  std::atomic<int> delcnt_ = 0;    /**< # of times deltask called */
#endif

  /**====================================
   * Task Helpers
   * ===================================*/

  /** Set task as externally complete */
  HSHM_ALWAYS_INLINE void SetModuleComplete() {
    // atask_flags_ |= TASK_MODULE_COMPLETE;
    task_flags_.SetBits(TASK_MODULE_COMPLETE);
  }

  /** Check if a task marked complete externally */
  HSHM_ALWAYS_INLINE bool IsModuleComplete() {
    // return atask_flags_.load() & TASK_MODULE_COMPLETE;
    return task_flags_.Any(TASK_MODULE_COMPLETE);
  }

  /** Set task as complete */
  HSHM_ALWAYS_INLINE void SetComplete() {
    // atask_flags_ |= TASK_MODULE_COMPLETE | TASK_COMPLETE;
    task_flags_.SetBits(TASK_MODULE_COMPLETE | TASK_COMPLETE);
  }

  /** Check if task is complete */
  HSHM_ALWAYS_INLINE bool IsComplete() {
    // return atask_flags_ & TASK_COMPLETE;
    return task_flags_.Any(TASK_COMPLETE);
  }

  /** Set task as fire & forget */
  HSHM_ALWAYS_INLINE void SetFireAndForget() {
    task_flags_.SetBits(TASK_FIRE_AND_FORGET);
  }

  /** Check if a task is fire & forget */
  HSHM_ALWAYS_INLINE bool IsFireAndForget() {
    return task_flags_.Any(TASK_FIRE_AND_FORGET);
  }

  /** Unset fire & forget */
  HSHM_ALWAYS_INLINE void UnsetFireAndForget() {
    task_flags_.UnsetBits(TASK_FIRE_AND_FORGET);
  }

  /** Check if task is long running */
  HSHM_ALWAYS_INLINE bool IsLongRunning() {
    return task_flags_.All(TASK_LONG_RUNNING);
  }

  /** Check if task is unordered */
  HSHM_ALWAYS_INLINE bool IsUnordered() {
    return task_flags_.Any(TASK_UNORDERED);
  }

  /** Set task as unordered */
  HSHM_ALWAYS_INLINE bool SetUnordered() {
    return task_flags_.Any(TASK_UNORDERED);
  }

  /** Set task as data owner */
  HSHM_ALWAYS_INLINE void SetDataOwner() {
    task_flags_.SetBits(TASK_DATA_OWNER);
  }

  /** Check if task is data owner */
  HSHM_ALWAYS_INLINE bool IsDataOwner() {
    return task_flags_.Any(TASK_DATA_OWNER);
  }

  /** Unset task as data owner */
  HSHM_ALWAYS_INLINE void UnsetDataOwner() {
    task_flags_.UnsetBits(TASK_DATA_OWNER);
  }

  /** Set this task as started */
  HSHM_ALWAYS_INLINE void SetStarted() {
    task_flags_.SetBits(TASK_HAS_STARTED);
  }

  /** Set this task as started */
  void UnsetStarted() {
    task_flags_.UnsetBits(TASK_HAS_STARTED);
  }

  /** Check if task has started */
  HSHM_ALWAYS_INLINE bool IsStarted() {
    return task_flags_.Any(TASK_HAS_STARTED);
  }

  /** Set blocked */
  HSHM_ALWAYS_INLINE void SetBlocked() {
    task_flags_.SetBits(TASK_BLOCKED);
  }

  /** Unset blocked */
  HSHM_ALWAYS_INLINE void UnsetBlocked() {
    task_flags_.UnsetBits(TASK_BLOCKED);
  }

  /** Check if task is blocked */
  HSHM_ALWAYS_INLINE bool IsBlocked() {
    return task_flags_.Any(TASK_BLOCKED);
  }

  /** Set this task as started */
  HSHM_ALWAYS_INLINE void UnsetLongRunning() {
    task_flags_.UnsetBits(TASK_LONG_RUNNING);
  }

  /** Set this task as blocking */
  HSHM_ALWAYS_INLINE bool IsCoroutine() {
    return task_flags_.Any(TASK_COROUTINE);
  }

  /** Set this task as blocking */
  HSHM_ALWAYS_INLINE void UnsetCoroutine() {
    task_flags_.UnsetBits(TASK_COROUTINE);
  }

  /** This task should be dispersed across all lanes */
  HSHM_ALWAYS_INLINE bool IsLaneAll() {
    return task_flags_.Any(TASK_LANE_ALL);
  }

  /** Unset this task as lane-dispersable */
  HSHM_ALWAYS_INLINE void UnsetLaneAll() {
    task_flags_.UnsetBits(TASK_LANE_ALL);
  }

  /** Set period in nanoseconds */
  HSHM_ALWAYS_INLINE void SetPeriodNs(double ns) {
    period_ns_ = ns;
  }

  /** Set period in microseconds */
  HSHM_ALWAYS_INLINE void SetPeriodUs(double us) {
    period_ns_ = us * 1000;
  }

  /** Set period in milliseconds */
  HSHM_ALWAYS_INLINE void SetPeriodMs(double ms) {
    period_ns_ = ms * 1000000;
  }

  /** Set period in seconds */
  HSHM_ALWAYS_INLINE void SetPeriodSec(double sec) {
    period_ns_ = sec * 1000000000;
  }

  /** Set period in minutes */
  HSHM_ALWAYS_INLINE void SetPeriodMin(double min) {
    period_ns_ = min * 60000000000;
  }

  /** This task flushes the runtime */
  HSHM_ALWAYS_INLINE bool IsFlush() {
    return task_flags_.Any(TASK_FLUSH);
  }

  /** Set signal complete */
  HSHM_ALWAYS_INLINE void SetSignalUnblock() {
    task_flags_.SetBits(TASK_SIGNAL_COMPLETE);
  }

  /** Check if task should signal complete */
  HSHM_ALWAYS_INLINE bool ShouldSignalUnblock() {
    return task_flags_.Any(TASK_SIGNAL_COMPLETE);
  }

  /** Unset signal complete */
  HSHM_ALWAYS_INLINE void UnsetSignalUnblock() {
    task_flags_.UnsetBits(TASK_SIGNAL_COMPLETE);
  }

  /** Set signal remote complete */
  HSHM_ALWAYS_INLINE void SetSignalRemoteComplete() {
    task_flags_.SetBits(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Check if task should signal complete */
  HSHM_ALWAYS_INLINE bool ShouldSignalRemoteComplete() {
    return task_flags_.Any(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Unset signal complete */
  HSHM_ALWAYS_INLINE void UnsetSignalRemoteComplete() {
    task_flags_.UnsetBits(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Mark this task as remote */
  HSHM_ALWAYS_INLINE void SetRead() {
    task_flags_.SetBits(TASK_READ);
  }

  /** Check if task is remote */
  HSHM_ALWAYS_INLINE bool IsRead() {
    return task_flags_.Any(TASK_READ);
  }

  /** Unset remote */
  HSHM_ALWAYS_INLINE void UnsetRead() {
    task_flags_.UnsetBits(TASK_READ);
  }

  /** Mark this task as remote */
  HSHM_ALWAYS_INLINE void SetWrite() {
    task_flags_.SetBits(TASK_WRITE);
  }

  /** Check if task is remote */
  HSHM_ALWAYS_INLINE bool IsWrite() {
    return task_flags_.Any(TASK_WRITE);
  }

  /** Unset remote */
  HSHM_ALWAYS_INLINE void UnsetWrite() {
    task_flags_.UnsetBits(TASK_WRITE);
  }

  /** Mark this task as remote */
  HSHM_ALWAYS_INLINE void SetRemote() {
    task_flags_.SetBits(TASK_REMOTE);
  }

  /** Check if task is remote */
  HSHM_ALWAYS_INLINE bool IsRemote() {
    return task_flags_.Any(TASK_REMOTE);
  }

  /** Unset remote */
  HSHM_ALWAYS_INLINE void UnsetRemote() {
    task_flags_.UnsetBits(TASK_REMOTE);
  }

  /** Determine if time has elapsed */
  HSHM_ALWAYS_INLINE bool ShouldRun(hshm::Timepoint &cur_time, bool flushing) {
    if (!IsLongRunning()) {
      return true;
    }
    if (!IsStarted() || flushing) {
      start_ = cur_time;
      return true;
    }
    bool should_start = start_.GetNsecFromStart(cur_time) >= period_ns_;
    if (should_start) {
      start_ = cur_time;
    }
    return should_start;
  }

  /** Mark this task as having been run */
  HSHM_ALWAYS_INLINE void DidRun(hshm::Timepoint &cur_time) {
    start_ = cur_time;
  }

  /** Yield the task */
  template<int THREAD_MODEL = 0>
  HSHM_ALWAYS_INLINE
  void Yield() {
    if constexpr (THREAD_MODEL == TASK_YIELD_STD) {
      HERMES_THREAD_MODEL->Yield();
    } else if constexpr (THREAD_MODEL == TASK_YIELD_CO) {
      ctx_.jmp_ = bctx::jump_fcontext(ctx_.jmp_.fctx, nullptr);
    } else if constexpr (THREAD_MODEL == TASK_YIELD_ABT) {
      ABT_thread_yield();
    }
    // NOTE(llogan): TASK_YIELD_NOCO is not here because it shouldn't
    // actually yield anything. Would longjmp be worthwhile here?
  }

  /** Yield a task to a different task */
  HSHM_ALWAYS_INLINE
  void YieldInit(Task *parent_task) {
    if (parent_task &&
        !IsFireAndForget() &&
        !IsLongRunning()) {
      ctx_.pending_to_ = parent_task;
      SetSignalUnblock();
    }
  }

  /** Wait for task to complete */
  template<int THREAD_MODEL = TASK_YIELD_STD>
  void Wait() {
    while (!IsComplete()) {
      if constexpr (THREAD_MODEL == TASK_YIELD_STD) {
        for (;;) {
          std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
          if (IsComplete()) {
            // std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
            return;
          }
        }
      }
      Yield<THREAD_MODEL>();
    }
  }

  /** Parent yields until this task completes */
  template<int THREAD_MODEL = TASK_YIELD_STD>
  HSHM_ALWAYS_INLINE
  void Yield(Task *parent_task) {
    if constexpr (THREAD_MODEL == TASK_YIELD_CO) {
      parent_task->SetBlocked();
    }
    parent_task->Yield<THREAD_MODEL>();
  }

  /** Parent waits for this task to complete */
  template<int THREAD_MODEL = 0>
  void Wait(Task *parent_task) {
    while (!IsComplete()) {
      Yield<THREAD_MODEL>(parent_task);
    }
  }

  /**====================================
   * Default Constructor
   * ===================================*/

  /** Default constructor */
  HSHM_ALWAYS_INLINE explicit
  Task(int) {}

  /** Default SHM constructor */
  HSHM_ALWAYS_INLINE explicit
  Task(hipc::Allocator *alloc) {
    shm_init_container(alloc);
  }

  /** SHM constructor */
  HSHM_ALWAYS_INLINE explicit
  Task(hipc::Allocator *alloc,
       const TaskNode &task_node) {
    shm_init_container(alloc);
    task_node_ = task_node;
  }

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  Task(hipc::Allocator *alloc,
       const TaskNode &task_node,
       const DomainId &domain_id,
       const TaskStateId &task_state,
       u32 lane_hash,
       u32 method,
       bitfield32_t task_flags) {
    shm_init_container(alloc);
    task_node_ = task_node;
    lane_hash_ = lane_hash;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = task_state;
    method_ = method;
    domain_id_ = domain_id;
    task_flags_ = task_flags;
    // atask_flags_ = task_flags.bits_;
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** SHM copy constructor */
  HSHM_ALWAYS_INLINE explicit Task(hipc::Allocator *alloc, const Task &other) {}

  /** SHM copy assignment operator */
  HSHM_ALWAYS_INLINE Task& operator=(const Task &other) {
    return *this;
  }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** SHM move constructor. */
  HSHM_ALWAYS_INLINE Task(hipc::Allocator *alloc, Task &&other) noexcept {}

  /** SHM move assignment operator. */
  HSHM_ALWAYS_INLINE Task& operator=(Task &&other) noexcept {
    return *this;
  }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  HSHM_ALWAYS_INLINE void shm_destroy_main() {}

  /** Check if the Task is empty */
  HSHM_ALWAYS_INLINE bool IsNull() const { return false; }

  /** Sets this Task as empty */
  HSHM_ALWAYS_INLINE void SetNull() {}

  /**====================================
   * Serialization
   * ===================================*/
  template<typename Ar>
  void task_serialize(Ar &ar) {
    // NOTE(llogan): don't serialize start_ because of clock drift
    ar(task_state_, task_node_, domain_id_, lane_hash_, prio_, method_,
       task_flags_, period_ns_);  // , atask_flags_);
  }

  template<typename TaskT>
  void task_dup(TaskT &other) {
    task_state_ = other.task_state_;
    task_node_ = other.task_node_;
    domain_id_ = other.domain_id_;
    lane_hash_ = other.lane_hash_;
    prio_ = other.prio_;
    method_ = other.method_;
    task_flags_ = other.task_flags_;
    // atask_flags_ = other.atask_flags_.load();
    period_ns_ = other.period_ns_;
    start_ = other.start_;
  }
};

/** Decorator macros */
#define IN
#define OUT
#define INOUT
#define TEMP

}  // namespace chm

#endif  // HRUN_INCLUDE_HRUN_QUEUE_MANAGER_REQUEST_H_
