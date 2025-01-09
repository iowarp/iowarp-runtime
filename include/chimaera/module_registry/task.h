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

#ifndef CHI_TASK_DEFN_H
#define CHI_TASK_DEFN_H

#include <csetjmp>

#include "chimaera/chimaera_types.h"

namespace chi {

template <typename DataT = hshm::charwrap>
using LocalSerialize = hipc::LocalSerialize<DataT>;

class Module;
class Lane;

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
/** The remote task has completed */
#define TASK_REMOTE_COMPLETE BIT_OPT(u32, 7)
/** This task has began execution */
#define TASK_HAS_STARTED BIT_OPT(u32, 8)
/** This task is completed */
#define TASK_COMPLETE BIT_OPT(u32, 9)
/** This task was marked completed outside of the worker thread */
#define TASK_MODULE_COMPLETE BIT_OPT(u32, 10)
/** This task is long-running */
#define TASK_LONG_RUNNING BIT_OPT(u32, 11)
/** This task is fire and forget. Free when completed */
#define TASK_FIRE_AND_FORGET BIT_OPT(u32, 12)
/** This task should not be run at this time (deprecated) */
#define TASK_DISABLE_RUN BIT_OPT(u32, 13)
/** This task owns the data in the task */
#define TASK_DATA_OWNER BIT_OPT(u32, 14)
/** This task uses co-routine wait */
#define TASK_COROUTINE BIT_OPT(u32, 15)
/** Monitor performance of this task */
#define TASK_SHOULD_SAMPLE BIT_OPT(u32, 18)
/** This task should be scheduled on all lanes (deprecated) */
#define TASK_LANE_ALL BIT_OPT(u32, 19)
/** This task flushes the runtime */
#define TASK_FLUSH BIT_OPT(u32, 20)
/** This task signals its completion */
#define TASK_SIGNAL_COMPLETE BIT_OPT(u32, 21)
/** This task is a remote task */
#define TASK_REMOTE BIT_OPT(u32, 22)
/** This task has been scheduled to a lane (deprecated) */
#define TASK_IS_ROUTED BIT_OPT(u32, 23)
/** This task is apart of remote debugging */
#define TASK_REMOTE_DEBUG_MARK BIT_OPT(u32, 31)

/** Used to define task methods */
#define TASK_METHOD_T CLS_CONST chi::MethodId

/** Used to indicate Yield to use */
#define TASK_YIELD_STD 0
#define TASK_YIELD_CO 1
#define TASK_YIELD_ABT 2
#define TASK_YIELD_EMPTY 3

/** The baseline set of tasks */
struct TaskMethod {
  TASK_METHOD_T kCreate = 0;       /**< The constructor of the task */
  TASK_METHOD_T kDestroy = 1;      /**< The destructor of the task */
  TASK_METHOD_T kNodeFailure = 2;  /**< The node failure method */
  TASK_METHOD_T kRecover = 3;      /**< The recovery method */
  TASK_METHOD_T kMigrate = 4;      /**< The migrate method */
  TASK_METHOD_T kUpgrade = 5;      /**< The update method */
  TASK_METHOD_T kCustomBegin = 10; /**< First index of custom methods */
};

/** Monitoring modes */
class MonitorMode {
 public:
  TASK_METHOD_T kEstLoad = 0;
  TASK_METHOD_T kSampleLoad = 1;
  TASK_METHOD_T kReinforceLoad = 2;
  TASK_METHOD_T kReplicaStart = 3;
  TASK_METHOD_T kReplicaAgg = 4;
};

/**
 * Let's say we have an I/O request to a device
 * I/O requests + MD operations need to be controlled for correctness
 * Is there a case where root tasks from different Containers need to be
 * ordered? No. Tasks spawned from the same root task need to be keyed to the
 * same worker stack Tasks apart of the same task group need to be ordered
 * */

/** An identifier used for representing the location of a task in a task graph
 */
struct TaskNode {
  TaskId root_;    /**< The id of the root task */
  u32 node_depth_; /**< The depth of the task in the task graph */

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  TaskNode() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~TaskNode() = default;

  /** Emplace constructor for root task */
  HSHM_INLINE_CROSS_FUN
  TaskNode(TaskId root) {
    root_ = root;
    node_depth_ = 0;
  }

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  TaskNode(const TaskNode &other) {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
  }

  /** Copy assignment operator */
  HSHM_INLINE_CROSS_FUN
  TaskNode &operator=(const TaskNode &other) {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  TaskNode(TaskNode &&other) noexcept {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
  }

  /** Move assignment operator */
  HSHM_INLINE_CROSS_FUN
  TaskNode &operator=(TaskNode &&other) noexcept {
    root_ = other.root_;
    node_depth_ = other.node_depth_;
    return *this;
  }

  /** Addition operator*/
  HSHM_INLINE_CROSS_FUN
  TaskNode operator+(int i) const {
    TaskNode ret;
    ret.root_ = root_;
    ret.node_depth_ = node_depth_ + i;
    return ret;
  }

  /** Addition operator*/
  HSHM_INLINE_CROSS_FUN
  TaskNode &operator+=(int i) {
    node_depth_ += i;
    return *this;
  }

  /** Null task node */
  HSHM_INLINE_CROSS_FUN
  static TaskNode GetNull() {
    TaskNode ret;
    ret.root_ = TaskId::GetNull();
    ret.node_depth_ = 0;
    return ret;
  }

  /** Check if null */
  HSHM_INLINE_CROSS_FUN
  bool IsNull() const { return root_.IsNull(); }

  /** Check if the root task */
  HSHM_INLINE_CROSS_FUN
  bool IsRoot() const { return node_depth_ == 0; }

  /** Serialization*/
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(root_, node_depth_);
  }

  /** Allow TaskNode to be printed as strings */
  friend std::ostream &operator<<(std::ostream &os, const TaskNode &obj) {
    return os << obj.root_ << "/" << std::to_string(obj.node_depth_);
  }
};

/** This task supports replication */
#define TF_REPLICA BIT_OPT(u32, 31)
/** This task uses SerializeStart */
#define TF_SRL_SYM_START BIT_OPT(u32, 0) | TF_REPLICA
/** This task uses SaveStart + LoadStart */
#define TF_SRL_ASYM_START BIT_OPT(u32, 1) | TF_REPLICA
/** This task uses SerializeEnd */
#define TF_SRL_SYM_END BIT_OPT(u32, 2) | TF_REPLICA
/** This task uses SaveEnd + LoadEnd */
#define TF_SRL_ASYM_END BIT_OPT(u32, 3) | TF_REPLICA
/** This task uses symmetric serialization */
#define TF_SRL_SYM (TF_SRL_SYM_START | TF_SRL_SYM_END)
/** This task uses asymmetric serialization */
#define TF_SRL_ASYM (TF_SRL_ASYM_START | TF_SRL_ASYM_END)
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
#define IS_TASK(T) std::is_base_of_v<chi::IsTask, T>
/** Determine this task supports serialization */
#define IS_SRL(T) T::SUPPORTS_SRL
/** Determine this task uses SerializeStart */
#define USES_SRL_START(T) T::SRL_SYM_START
/** Determine this task uses SerializeEnd */
#define USES_SRL_END(T) T::SRL_SYM_END

/** Compile-time flags indicating task methods and operation support */
template <u32 FLAGS>
struct TaskFlags : public IsTask {
 public:
  TASK_FLAG_T IS_LOCAL = FLAGS & TF_LOCAL;
  TASK_FLAG_T REPLICA = FLAGS & TF_REPLICA;
  TASK_FLAG_T SUPPORTS_SRL = FLAGS & (TF_SRL_SYM | TF_SRL_ASYM);
  TASK_FLAG_T SRL_SYM_START = FLAGS & TF_SRL_SYM_START;
  TASK_FLAG_T SRL_SYM_END = FLAGS & TF_SRL_SYM_END;
  TASK_FLAG_T MONITOR = FLAGS & TF_MONITOR;
  TASK_FLAG_T CMPGRP = FLAGS & TF_CMPGRP;
};

/** Prioritization of tasks */
class TaskPrioOpt {
 public:
  CLS_CONST TaskPrio kLowLatency = 0;  /**< Low latency task lane */
  CLS_CONST TaskPrio kHighLatency = 1; /**< High latency task lane */
  CLS_CONST TaskPrio kNumPrio = 2;     /**< Number of priorities */
};

/** Used to indicate the amount of work remaining to do when flushing */
struct WorkPending {
  size_t count_ = 0;
  size_t work_done_ = 0;
  std::atomic<bool> flushing_ = false;
  size_t flush_iter_ = 0;
};

struct Task;

/** Load definition */
struct Load {
  size_t cpu_load_ = 0;
  size_t mem_load_ = 0;
  size_t io_load_ = 0;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Load() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Load() = default;

  /** Addition operator */
  HSHM_INLINE_CROSS_FUN
  Load operator+(const Load &other) const {
    Load ret;
    ret.cpu_load_ = cpu_load_ + other.cpu_load_;
    ret.mem_load_ = mem_load_ + other.mem_load_;
    ret.io_load_ = io_load_ + other.io_load_;
    return ret;
  }

  /** Subtraction operator */
  HSHM_INLINE_CROSS_FUN
  Load operator-(const Load &other) const {
    Load ret;
    ret.cpu_load_ = cpu_load_ - other.cpu_load_;
    ret.mem_load_ = mem_load_ - other.mem_load_;
    ret.io_load_ = io_load_ - other.io_load_;
    return ret;
  }

  /** Addition assignment operator */
  HSHM_INLINE_CROSS_FUN
  Load &operator+=(const Load &other) {
    cpu_load_ += other.cpu_load_;
    mem_load_ += other.mem_load_;
    io_load_ += other.io_load_;
    return *this;
  }

  /** Subtraction assignment operator */
  HSHM_INLINE_CROSS_FUN
  Load &operator-=(const Load &other) {
    cpu_load_ -= other.cpu_load_;
    mem_load_ -= other.mem_load_;
    io_load_ -= other.io_load_;
    return *this;
  }
};

/** Context passed to the Run method of a task */
struct RunContext {
  bitfield32_t run_flags_;    /**< Properties of the task */
  bitfield32_t worker_props_; /**< Properties of the worker */
  WorkerId worker_id_;        /**< The worker id of the task */
  bctx::transfer_t jmp_;      /**< Stack info for coroutines */
  void *stack_ptr_;           /**< Stack pointer (coroutine) */
  Module *exec_;
  WorkPending *flush_;
  hshm::Timer timer_;
  Task *co_task_;
  Task *pending_to_;
  std::vector<FullPtr<Task>> *replicas_;
  size_t ret_task_addr_;
  NodeId ret_node_;
  hipc::atomic<ssize_t> block_count_ = 0;
  ContainerId route_container_id_;
  chi::Lane *route_lane_;
  Load load_;
};

/** A generic task base class */
struct Task : public hipc::ShmContainer, public hipc::list_queue_entry {
 public:
  PoolId pool_;             /**< The unique name of a pool */
  TaskNode task_node_;      /**< The unique ID of this task in the graph */
  DomainQuery dom_query_;   /**< The nodes that the task should run on */
  MethodId method_;         /**< The method to call in the state */
  TaskPrio prio_;           /**< Priority of the request */
  bitfield32_t task_flags_; /**< Properties of the task */
  double period_ns_;        /**< The period of the task */
  size_t start_;            /**< The time the task started */
  RunContext rctx_;
  // #ifdef CHIMAERA_TASK_DEBUG
  std::atomic<int> delcnt_ = 0; /**< # of times deltask called */
                                // #endif

  /**====================================
   * Task Bits
   * ===================================*/

  /** Get lane hash */
  HSHM_INLINE_CROSS_FUN
  const u32 &GetContainerId() const { return dom_query_.sel_.id_; }

  /** Set task as externally complete */
  HSHM_INLINE_CROSS_FUN
  void SetModuleComplete() {
    task_flags_.SetBits(TASK_MODULE_COMPLETE);
    UnsetStarted();
  }

  /** Check if a task marked complete externally */
  HSHM_INLINE_CROSS_FUN
  bool IsModuleComplete() const {
    return task_flags_.Any(TASK_MODULE_COMPLETE);
  }

  /** Unset task as complete */
  HSHM_INLINE_CROSS_FUN
  void UnsetModuleComplete() {
    task_flags_.UnsetBits(TASK_MODULE_COMPLETE);
    SetStarted();
  }

  /** Set task as complete */
  HSHM_INLINE_CROSS_FUN
  void SetComplete() {
    task_flags_.SetBits(TASK_MODULE_COMPLETE | TASK_COMPLETE);
  }

  /** Check if task is complete */
  HSHM_INLINE_CROSS_FUN
  bool IsComplete() const { return task_flags_.Any(TASK_COMPLETE); }

  /** Unset task as complete */
  HSHM_INLINE_CROSS_FUN
  void UnsetComplete() {
    task_flags_.UnsetBits(TASK_MODULE_COMPLETE | TASK_COMPLETE);
  }

  /** Set task as fire & forget */
  HSHM_INLINE_CROSS_FUN
  void SetFireAndForget() { task_flags_.SetBits(TASK_FIRE_AND_FORGET); }

  /** Check if a task is fire & forget */
  HSHM_INLINE_CROSS_FUN
  bool IsFireAndForget() const { return task_flags_.Any(TASK_FIRE_AND_FORGET); }

  /** Unset fire & forget */
  HSHM_INLINE_CROSS_FUN
  void UnsetFireAndForget() { task_flags_.UnsetBits(TASK_FIRE_AND_FORGET); }

  /** Check if task is long running */
  HSHM_INLINE_CROSS_FUN
  bool IsLongRunning() const { return task_flags_.All(TASK_LONG_RUNNING); }

  /** Set task as data owner */
  HSHM_INLINE_CROSS_FUN
  void SetDataOwner() { task_flags_.SetBits(TASK_DATA_OWNER); }

  /** Check if task is data owner */
  HSHM_INLINE_CROSS_FUN
  bool IsDataOwner() const { return task_flags_.Any(TASK_DATA_OWNER); }

  /** Unset task as data owner */
  HSHM_INLINE_CROSS_FUN
  void UnsetDataOwner() { task_flags_.UnsetBits(TASK_DATA_OWNER); }

  /** Set this task as started */
  HSHM_INLINE_CROSS_FUN
  void SetRemoteDebug() { task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK); }

  /** Set this task as started */
  HSHM_INLINE_CROSS_FUN
  void UnsetRemoteDebug() { task_flags_.UnsetBits(TASK_REMOTE_DEBUG_MARK); }

  /** Check if task has started */
  HSHM_INLINE_CROSS_FUN
  bool IsRemoteDebug() const { return task_flags_.Any(TASK_REMOTE_DEBUG_MARK); }

  /** Set this task as started */
  HSHM_INLINE_CROSS_FUN
  void UnsetLongRunning() { task_flags_.UnsetBits(TASK_LONG_RUNNING); }

  /** Set this task as blocking */
  HSHM_INLINE_CROSS_FUN
  bool IsCoroutine() const { return task_flags_.Any(TASK_COROUTINE); }

  /** Set this task as blocking */
  HSHM_INLINE_CROSS_FUN
  void UnsetCoroutine() { task_flags_.UnsetBits(TASK_COROUTINE); }

  /** Set period in nanoseconds */
  HSHM_INLINE_CROSS_FUN
  void SetPeriodNs(double ns) { period_ns_ = ns; }

  /** Set period in microseconds */
  HSHM_INLINE_CROSS_FUN
  void SetPeriodUs(double us) { period_ns_ = us * 1000; }

  /** Set period in milliseconds */
  HSHM_INLINE_CROSS_FUN
  void SetPeriodMs(double ms) { period_ns_ = ms * 1000000; }

  /** Set period in seconds */
  HSHM_INLINE_CROSS_FUN
  void SetPeriodSec(double sec) { period_ns_ = sec * 1000000000; }

  /** Set period in minutes */
  HSHM_INLINE_CROSS_FUN
  void SetPeriodMin(double min) { period_ns_ = min * 60000000000; }

  /** This task flushes the runtime */
  HSHM_INLINE_CROSS_FUN
  bool IsFlush() const { return task_flags_.Any(TASK_FLUSH); }

  /** Set this task as flushing */
  HSHM_INLINE_CROSS_FUN
  void SetFlush() { task_flags_.SetBits(TASK_FLUSH); }

  /** Unset this task as flushing */
  HSHM_INLINE_CROSS_FUN
  void UnsetFlush() { task_flags_.UnsetBits(TASK_FLUSH); }

  /** Mark this task as remote */
  HSHM_INLINE_CROSS_FUN
  void SetRead() { task_flags_.SetBits(TASK_READ); }

  /** Check if task is remote */
  HSHM_INLINE_CROSS_FUN
  bool IsRead() const { return task_flags_.Any(TASK_READ); }

  /** Unset remote */
  HSHM_INLINE_CROSS_FUN
  void UnsetRead() { task_flags_.UnsetBits(TASK_READ); }

  /** Mark this task as remote */
  HSHM_INLINE_CROSS_FUN
  void SetWrite() { task_flags_.SetBits(TASK_WRITE); }

  /** Check if task is remote */
  HSHM_INLINE_CROSS_FUN
  bool IsWrite() const { return task_flags_.Any(TASK_WRITE); }

  /** Unset remote */
  HSHM_INLINE_CROSS_FUN
  void UnsetWrite() { task_flags_.UnsetBits(TASK_WRITE); }

  /**====================================
   * RunContext Bits
   * ===================================*/

  /** Set this task as routed */
  void SetRouted() { rctx_.run_flags_.SetBits(TASK_IS_ROUTED); }

  /** Check if task is routed */
  bool IsRouted() const { return rctx_.run_flags_.Any(TASK_IS_ROUTED); }

  /** Unset this task as routed */
  void UnsetRouted() { rctx_.run_flags_.UnsetBits(TASK_IS_ROUTED); }

  /** Set this task as started */
  void SetStarted() { rctx_.run_flags_.SetBits(TASK_HAS_STARTED); }

  /** Set this task as started */
  void UnsetStarted() { rctx_.run_flags_.UnsetBits(TASK_HAS_STARTED); }

  /** Check if task has started */
  bool IsStarted() const { return rctx_.run_flags_.Any(TASK_HAS_STARTED); }

  /** Set blocked */
  void SetBlocked(size_t count) { rctx_.block_count_ += count; }

  /** Check if task is blocked */
  bool IsBlocked() const { return rctx_.block_count_.load() > 0; }

  /** Mark task as routed */
  void SetShouldSample() { rctx_.run_flags_.SetBits(TASK_SHOULD_SAMPLE); }

  /** Check if task is routed */
  bool ShouldSample() const { return rctx_.run_flags_.Any(TASK_SHOULD_SAMPLE); }

  /** Unset task as routed */
  void UnsetShouldSample() { rctx_.run_flags_.UnsetBits(TASK_SHOULD_SAMPLE); }

  /** Set signal complete */
  void SetSignalUnblock() { rctx_.run_flags_.SetBits(TASK_SIGNAL_COMPLETE); }

  /** Check if task should signal complete */
  bool ShouldSignalUnblock() const {
    return rctx_.run_flags_.Any(TASK_SIGNAL_COMPLETE);
  }

  /** Unset signal complete */
  void UnsetSignalUnblock() {
    rctx_.run_flags_.UnsetBits(TASK_SIGNAL_COMPLETE);
  }

  /** Set signal remote complete */
  void SetSignalRemoteComplete() {
    rctx_.run_flags_.SetBits(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Check if task should signal complete */
  bool ShouldSignalRemoteComplete() {
    return rctx_.run_flags_.Any(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Unset signal complete */
  void UnsetSignalRemoteComplete() {
    rctx_.run_flags_.UnsetBits(TASK_SIGNAL_REMOTE_COMPLETE);
  }

  /** Mark this task as remote */
  void SetRemote() { rctx_.run_flags_.SetBits(TASK_REMOTE); }

  /** Check if task is remote */
  bool IsRemote() const { return rctx_.run_flags_.Any(TASK_REMOTE); }

  /** Unset remote */
  void UnsetRemote() { rctx_.run_flags_.UnsetBits(TASK_REMOTE); }

  /** Determine if time has elapsed */
  bool ShouldRun(CacheTimer &cur_time, bool flushing) {
    if (!IsLongRunning()) {
      return true;
    }
    if (!IsStarted() || flushing) {
      start_ = cur_time.GetNsecFromStart();
      return true;
    }
    bool should_start = cur_time.GetNsecFromStart(start_) >= period_ns_;
    if (should_start) {
      start_ = cur_time.GetNsecFromStart();
    }
    return should_start;
  }

  /** Mark this task as having been run */
  void DidRun(CacheTimer &cur_time) { start_ = cur_time.GetNsecFromStart(); }

  /**====================================
   * Yield and Wait Routines
   * ===================================*/

  /** Yield (coroutine) */
  HSHM_INLINE_CROSS_FUN
  void YieldCo() {
#ifdef HSHM_IS_HOST
    rctx_.jmp_ = bctx::jump_fcontext(rctx_.jmp_.fctx, nullptr);
#endif
  }

  /** Yield (standard) */
  HSHM_INLINE_CROSS_FUN
  static void YieldStd() { HERMES_THREAD_MODEL->Yield(); }

  /** Yield (argobots) */
  HSHM_CROSS_FUN
  static void YieldArgo();

  /** Yield in general */
  template <int THREAD_MODEL = 0>
  HSHM_INLINE_CROSS_FUN static void StaticYieldFactory() {
    if constexpr (THREAD_MODEL == TASK_YIELD_STD) {
      YieldStd();
    } else if constexpr (THREAD_MODEL == TASK_YIELD_ABT) {
      YieldArgo();
    }
  }

  /** Yield the task */
  template <int THREAD_MODEL = 0>
  HSHM_INLINE_CROSS_FUN void YieldFactory() {
    if constexpr (THREAD_MODEL == TASK_YIELD_CO) {
      YieldCo();
    } else {
      StaticYieldFactory<THREAD_MODEL>();
    }
    // NOTE(llogan): TASK_YIELD_NOCO is not here because it shouldn't
    // actually yield anything. Would longjmp be worthwhile here?
  }

  HSHM_INLINE_CROSS_FUN
  void Yield() {
#ifdef CHIMAERA_RUNTIME
    YieldFactory<TASK_YIELD_CO>();
#else
    YieldFactory<TASK_YIELD_STD>();
#endif
  }

  /** Yield a task to a different task (runtime-only) */
  void YieldInit(Task *parent_task) {
#if defined(CHIMAERA_RUNTIME) and defined(HSHM_IS_HOST)
    if (parent_task && !IsFireAndForget() && !IsLongRunning()) {
      rctx_.pending_to_ = parent_task;
      SetSignalUnblock();
    }
#endif
  }

  /** Wait for task to complete */
  HSHM_CROSS_FUN
  void Wait(u32 flags = TASK_COMPLETE);

  /** Spin wait */
  HSHM_INLINE_CROSS_FUN
  void SpinWait(u32 flags = TASK_COMPLETE) {
    for (;;) {
      // std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
      if (task_flags_.All(flags)) {
        return;
      }
    }
  }

  /** Spin wait */
  HSHM_INLINE_CROSS_FUN
  void SpinWaitCo(u32 flags = TASK_COMPLETE) {
    for (;;) {
      if (task_flags_.All(flags)) {
        // std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
        return;
      }
      Yield();
    }
  }

  /** This task waits for subtask to complete */
  template <typename TaskT = Task>
  HSHM_INLINE_CROSS_FUN void Wait(FullPtr<TaskT> &subtask,
                                  u32 flags = TASK_COMPLETE) {
    Wait(subtask.ptr_, flags);
  }

  /** This task waits for subtask to complete */
  HSHM_INLINE_CROSS_FUN
  void Wait(Task *subtask, u32 flags = TASK_COMPLETE) {
    Wait(&subtask, 1, flags);
  }

  /** This task waits for a set of tasks to complete */
  template <typename TaskT>
  HSHM_INLINE void Wait(std::vector<FullPtr<TaskT>> &subtasks,
                        u32 flags = TASK_COMPLETE) {
#ifdef CHIMAERA_RUNTIME
    if (!subtasks.empty()) {
      SetBlocked(subtasks.size());
      Yield();
    }
#else
    for (FullPtr<TaskT> &subtask : subtasks) {
      while (!subtask->task_flags_.All(flags)) {
        Yield();
      }
    }
#endif
  }

  /** This task waits for subtask to complete */
  HSHM_INLINE_CROSS_FUN
  void Wait(Task **subtasks, size_t count, u32 flags = TASK_COMPLETE) {
#ifdef CHIMAERA_RUNTIME
    if (count) {
      SetBlocked(count);
      Yield();
    }
#else
    for (size_t i = 0; i < count; ++i) {
      while (!subtasks[i]->task_flags_.All(flags)) {
        Yield();
      }
    }
#endif
  }

  /**====================================
   * Default Constructor
   * ===================================*/

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Task(int) {}

  /** Default SHM constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Task(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  /** SHM constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Task(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                const TaskNode &task_node) {
    task_node_ = task_node;
  }

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Task(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                const TaskNode &task_node, const DomainQuery &dom_query,
                const PoolId &task_state, u32 lane_hash, u32 method,
                bitfield32_t task_flags) {
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = task_state;
    method_ = method;
    dom_query_ = dom_query;
    task_flags_ = task_flags;
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** SHM copy constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Task(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                const Task &other) {}

  /** SHM copy assignment operator */
  HSHM_INLINE_CROSS_FUN
  Task &operator=(const Task &other) { return *this; }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** SHM move constructor. */
  HSHM_INLINE_CROSS_FUN
  Task(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, Task &&other) noexcept {}

  /** SHM move assignment operator. */
  HSHM_INLINE_CROSS_FUN
  Task &operator=(Task &&other) noexcept { return *this; }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  HSHM_INLINE_CROSS_FUN
  void shm_destroy_main() {}

  /** Check if the Task is empty */
  HSHM_INLINE_CROSS_FUN
  bool IsNull() const { return false; }

  /** Sets this Task as empty */
  HSHM_INLINE_CROSS_FUN
  void SetNull() {}

  /**====================================
   * Serialization
   * ===================================*/
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void task_serialize(Ar &ar) {
    // NOTE(llogan): don't serialize start_ because of clock drift
    ar(pool_, task_node_, dom_query_, prio_, method_, task_flags_, period_ns_);
  }

  template <typename TaskT>
  HSHM_INLINE_CROSS_FUN void task_dup(TaskT &other) {
    pool_ = other.pool_;
    task_node_ = other.task_node_;
    dom_query_ = other.dom_query_;
    prio_ = other.prio_;
    method_ = other.method_;
    task_flags_ = other.task_flags_;
    UnsetComplete();
    period_ns_ = other.period_ns_;
    start_ = other.start_;
  }
};

/** Decorator macros */
#define IN
#define OUT
#define INOUT
#define TEMP

}  // namespace chi

#endif  // CHI_TASK_DEFN_H
