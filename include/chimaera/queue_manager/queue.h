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

#ifndef CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_H_
#define CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_H_

#include <vector>

#include "chimaera/chimaera_types.h"
#include "chimaera/module_registry/task.h"

/** Requests in this queue can be processed in any order */
#define QUEUE_READY BIT_OPT(chi::IntFlag, 0)
/** This queue contains primarily latency-sensitive tasks */
#define QUEUE_LOW_LATENCY BIT_OPT(chi::IntFlag, 1)
/** This queue contains primarily throughput-intense tasks */
#define QUEUE_HIGH_LATENCY 0
/** This queue is currently being resized */
#define QUEUE_RESIZE BIT_OPT(chi::IntFlag, 2)
/** This queue is currently processing updates */
#define QUEUE_UPDATE BIT_OPT(chi::IntFlag, 3)
/** Requests in this queue can be processed in any order */
#define QUEUE_UNORDERED BIT_OPT(chi::IntFlag, 4)
/** Requests in this queue are long-running */
#define QUEUE_LONG_RUNNING BIT_OPT(chi::IntFlag, 5)
/** Requests in this queue should not be scheduled on a traditional worker */
#define QUEUE_DISABLED BIT_OPT(chi::IntFlag, 6)

namespace chi::ingress {

/** Prioritization info needed to be set by client */
struct PriorityInfo {
  TaskPrio prio_;   /**< Priority ID */
  u32 max_lanes_;   /**< Maximum number of lanes in the queue */
  u32 num_lanes_;   /**< Current number of lanes in use */
  u32 depth_;       /**< The maximum depth of individual lanes */
  ibitfield flags_; /**< Scheduling hints for the queue */
  u32 tether_; /**< Lanes should be pinned to the same workers as the tether */

  /** Default constructor */
  HSHM_CROSS_FUN
  PriorityInfo() = default;

  /** Emplace constructor */
  HSHM_CROSS_FUN
  PriorityInfo(TaskPrio prio, u32 num_lanes, u32 max_lanes, u32 depth,
               chi::IntFlag flags, u32 tether = 0) {
    prio_ = prio;
    max_lanes_ = max_lanes;
    num_lanes_ = num_lanes;
    depth_ = depth;
    flags_ = ibitfield(flags);
    tether_ = tether;
  }

  /** Emplace constructor */
  HSHM_CROSS_FUN
  PriorityInfo(TaskPrio prio, u32 num_lanes, u32 max_lanes, u32 depth,
               ibitfield flags, u32 tether = 0) {
    prio_ = prio;
    max_lanes_ = max_lanes;
    num_lanes_ = num_lanes;
    depth_ = depth;
    flags_ = flags;
    tether_ = tether;
  }

  /** Copy constructor */
  HSHM_CROSS_FUN
  PriorityInfo(const PriorityInfo &priority) {
    prio_ = priority.prio_;
    max_lanes_ = priority.max_lanes_;
    num_lanes_ = priority.num_lanes_;
    depth_ = priority.depth_;
    flags_ = priority.flags_;
    tether_ = priority.tether_;
  }

  /** Move constructor */
  HSHM_CROSS_FUN
  PriorityInfo(PriorityInfo &&priority) noexcept {
    prio_ = priority.prio_;
    max_lanes_ = priority.max_lanes_;
    num_lanes_ = priority.num_lanes_;
    depth_ = priority.depth_;
    flags_ = priority.flags_;
    tether_ = priority.tether_;
  }

  /** Copy assignment operator */
  HSHM_CROSS_FUN
  PriorityInfo &operator=(const PriorityInfo &priority) {
    if (this != &priority) {
      prio_ = priority.prio_;
      max_lanes_ = priority.max_lanes_;
      num_lanes_ = priority.num_lanes_;
      depth_ = priority.depth_;
      flags_ = priority.flags_;
      tether_ = priority.tether_;
    }
    return *this;
  }

  /** Move assignment operator */
  HSHM_CROSS_FUN
  PriorityInfo &operator=(PriorityInfo &&priority) noexcept {
    if (this != &priority) {
      prio_ = priority.prio_;
      max_lanes_ = priority.max_lanes_;
      num_lanes_ = priority.num_lanes_;
      depth_ = priority.depth_;
      flags_ = priority.flags_;
      tether_ = priority.tether_;
    }
    return *this;
  }

  /** Serialize Priority Info */
  template <typename Ar>
  HSHM_CROSS_FUN void serialize(Ar &ar) {
    ar & prio_;
    ar & max_lanes_;
    ar & num_lanes_;
    ar & depth_;
    ar & flags_;
    ar & tether_;
  }
};

/** The data stored in a lane */
typedef hipc::Pointer LaneData;

/** Queue token*/
using hshm::qtok_t;

/** Represents a lane tasks can be stored */
class Lane : public hipc::ShmContainer {
 public:
  hipc::mpsc_queue<LaneData, CHI_ALLOC_T> queue_;
  QueueId id_;
  i32 worker_id_ = -1;

 public:
  /**====================================
   * Default Constructor
   * ===================================*/

  /** Default constructor. */
  HSHM_INLINE_CROSS_FUN
  Lane() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Lane() = default;

  /** SHM constructor. Default. */
  HSHM_INLINE_CROSS_FUN
  explicit Lane(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                size_t depth = 1024, QueueId id = QueueId::GetNull())
      : queue_(alloc, depth) {
    id_ = id;
    SetNull();
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Lane(const Lane &other)
      : queue_(other.queue_.GetCtxAllocator(), other.queue_) {}

  /** SHM copy constructor */
  HSHM_INLINE_CROSS_FUN
  explicit Lane(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const Lane &other)
      : queue_(alloc, other.queue_) {}

  /** SHM copy assignment operator */
  HSHM_INLINE_CROSS_FUN
  Lane &operator=(const Lane &other) {
    if (this != &other) {
      queue_ = other.queue_;
    }
    return *this;
  }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** Move constructor. */
  HSHM_INLINE_CROSS_FUN
  Lane(Lane &&other) noexcept
      : queue_(other.queue_.GetCtxAllocator(), std::move(other.queue_)) {}

  /** SHM move constructor. */
  HSHM_INLINE_CROSS_FUN
  Lane(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, Lane &&other) noexcept
      : queue_(alloc, std::move(other.queue_)) {}

  /** SHM move assignment operator. */
  HSHM_INLINE_CROSS_FUN
  Lane &operator=(Lane &&other) noexcept {
    if (this != &other) {
      queue_ = std::move(other.queue_);
    }
    return *this;
  }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  HSHM_INLINE_CROSS_FUN
  void shm_destroy() { queue_.shm_destroy(); }

  /** Check if the list is empty */
  HSHM_INLINE_CROSS_FUN
  bool IsNull() const { return queue_.IsNull(); }

  /** Sets this list as empty */
  HSHM_INLINE_CROSS_FUN
  void SetNull() { queue_.SetNull(); }

  /**====================================
   * MPSC Queue Methods
   * ===================================*/

  /** Construct an element at \a pos position in the list */
  template <typename... Args>
  HSHM_INLINE_CROSS_FUN qtok_t emplace(Args &&...args) {
    return queue_.emplace(std::forward<Args>(args)...);
  }

 public:
  /** Consumer pops the head object */
  HSHM_INLINE_CROSS_FUN
  qtok_t pop(LaneData &val) { return queue_.pop(val); }

  /** Consumer pops the head object */
  HSHM_INLINE_CROSS_FUN
  qtok_t pop() { return queue_.pop(); }

  /** Consumer peeks an object */
  HSHM_INLINE_CROSS_FUN
  qtok_t peek(chi::ipc::pair<bitfield64_t, LaneData> *&val, int off = 0) {
    return queue_.peek(val, off);
  }

  /** Consumer peeks an object */
  HSHM_INLINE_CROSS_FUN
  qtok_t peek(LaneData *&val, int off = 0) { return queue_.peek(val, off); }

  /** Current size of queue */
  HSHM_INLINE_CROSS_FUN
  size_t GetSize() { return queue_.GetSize(); }

  /** Max depth of queue */
  HSHM_INLINE_CROSS_FUN
  size_t GetDepth() { return queue_.GetDepth(); }
};

/** Prioritization of different lanes in the queue */
struct LaneGroup : public PriorityInfo, public hipc::ShmContainer {
  chi::ipc::vector<Lane> lanes_; /**< The lanes of the queue */
  u32 prio_;          /**< The priority of the lane group */
  u32 num_scheduled_; /**< The number of lanes currently scheduled on workers */
  u32 tether_; /**< Lanes should be pinned to the same workers as the tether's
                  prio group */

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  LaneGroup(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : lanes_(alloc) {}

  /** Set priority info */
  HSHM_INLINE_CROSS_FUN
  LaneGroup(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
            const PriorityInfo &priority)
      : lanes_(alloc) {
    prio_ = priority.prio_;
    max_lanes_ = priority.max_lanes_;
    num_lanes_ = priority.num_lanes_;
    num_scheduled_ = 0;
    depth_ = priority.depth_;
    flags_ = priority.flags_;
    tether_ = priority.tether_;
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** SHM Copy constructor. Should never actually be called. */
  HSHM_INLINE_CROSS_FUN
  LaneGroup(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
            const LaneGroup &other)
      : lanes_(alloc, other.lanes_) {
    prio_ = other.prio_;
    max_lanes_ = other.max_lanes_;
    num_lanes_ = other.num_lanes_;
    num_scheduled_ = other.num_scheduled_;
    depth_ = other.depth_;
    flags_ = other.flags_;
    tether_ = other.tether_;
  }

  /** SHM copy assignment operator */
  HSHM_INLINE_CROSS_FUN
  LaneGroup &operator=(const LaneGroup &other) {
    if (this != &other) {
      prio_ = other.prio_;
      max_lanes_ = other.max_lanes_;
      num_lanes_ = other.num_lanes_;
      num_scheduled_ = other.num_scheduled_;
      depth_ = other.depth_;
      flags_ = other.flags_;
      tether_ = other.tether_;
    }
    return *this;
  }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** SHM move constructor. */
  HSHM_INLINE_CROSS_FUN
  LaneGroup(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
            LaneGroup &&other) noexcept
      : lanes_(alloc, std::move(other.lanes_)) {
    prio_ = other.prio_;
    max_lanes_ = other.max_lanes_;
    num_lanes_ = other.num_lanes_;
    num_scheduled_ = other.num_scheduled_;
    depth_ = other.depth_;
    flags_ = other.flags_;
    tether_ = other.tether_;
    lanes_ = std::move(other.lanes_);
    other.SetNull();
  }

  /** SHM move assignment operator. */
  HSHM_INLINE_CROSS_FUN
  LaneGroup &operator=(LaneGroup &&other) noexcept {
    if (this != &other) {
      prio_ = other.prio_;
      max_lanes_ = other.max_lanes_;
      num_lanes_ = other.num_lanes_;
      num_scheduled_ = other.num_scheduled_;
      depth_ = other.depth_;
      flags_ = other.flags_;
      tether_ = other.tether_;
      lanes_ = std::move(other.lanes_);
      other.SetNull();
    }
    return *this;
  }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  HSHM_INLINE_CROSS_FUN
  void shm_destroy() { lanes_.shm_destroy(); }

  /** Check if the list is empty */
  HSHM_INLINE_CROSS_FUN
  bool IsNull() const { return lanes_.IsNull(); }

  /** Sets this list as empty */
  HSHM_INLINE_CROSS_FUN
  void SetNull() {}

  /**====================================
   * Helpers
   * ===================================*/

  /** Check if this group is long-running or ADMIN */
  HSHM_INLINE_CROSS_FUN
  bool IsLowPriority() { return flags_.Any(QUEUE_LONG_RUNNING) || prio_ == 0; }

  /** Check if this group is long-running or ADMIN */
  HSHM_INLINE_CROSS_FUN
  bool IsLowLatency() { return flags_.Any(QUEUE_LOW_LATENCY); }

  /** Get lane */
  HSHM_INLINE_CROSS_FUN
  Lane &GetLane(LaneId lane_id) { return lanes_[lane_id]; }
};

/**
 * The shared-memory representation of a Queue
 * */
struct MultiQueue : public hipc::ShmContainer {
  chi::ipc::vector<LaneGroup> groups_; /**< Divide the lanes into groups */
  QueueId id_;                         /**< Globally unique ID of this queue */
  ibitfield flags_;                    /**< Flags for the queue */

 public:
  /**====================================
   * Constructor
   * ===================================*/

  /** SHM constructor. Default. */
  HSHM_CROSS_FUN
  explicit MultiQueue(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : groups_(alloc), id_(QueueId::GetNull()) {
    SetNull();
  }

  /** SHM constructor. */
  explicit MultiQueue(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                      const QueueId &id, const std::vector<PriorityInfo> &prios)
      : groups_(alloc, prios.size()), id_(id) {
    for (const PriorityInfo &prio_info : prios) {
      groups_.replace(groups_.begin() + prio_info.prio_, prio_info);
      LaneGroup &lane_group = groups_[prio_info.prio_];
      // Initialize lanes
      lane_group.lanes_.reserve(prio_info.max_lanes_);
      for (LaneId lane_id = 0; lane_id < lane_group.num_lanes_; ++lane_id) {
        lane_group.lanes_.emplace_back(lane_group.depth_,
                                       QueueId{prio_info.prio_, lane_id});
        Lane &lane = lane_group.lanes_.back();
        lane.queue_.flags_ = prio_info.flags_;
      }
    }
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** SHM copy constructor */
  HSHM_CROSS_FUN
  explicit MultiQueue(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                      const MultiQueue &other)
      : groups_(alloc) {
    SetNull();
    shm_strong_copy_construct_and_op(other);
  }

  /** SHM copy assignment operator */
  HSHM_CROSS_FUN
  MultiQueue &operator=(const MultiQueue &other) {
    if (this != &other) {
      shm_destroy();
      shm_strong_copy_construct_and_op(other);
    }
    return *this;
  }

  /** SHM copy constructor + operator main */
  HSHM_CROSS_FUN
  void shm_strong_copy_construct_and_op(const MultiQueue &other) {
    groups_ = other.groups_;
  }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** SHM move constructor. */
  HSHM_CROSS_FUN
  MultiQueue(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
             MultiQueue &&other) noexcept
      : groups_(alloc) {
    groups_ = std::move(other.groups_);
    other.SetNull();
  }

  /** SHM move assignment operator. */
  HSHM_CROSS_FUN
  MultiQueue &operator=(MultiQueue &&other) noexcept {
    if (this != &other) {
      groups_ = std::move(other.groups_);
      other.SetNull();
    }
    return *this;
  }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  HSHM_INLINE_CROSS_FUN
  void shm_destroy() { groups_.shm_destroy(); }

  /** Check if the list is empty */
  HSHM_INLINE_CROSS_FUN
  bool IsNull() const { return groups_.IsNull(); }

  /** Sets this list as empty */
  HSHM_INLINE_CROSS_FUN
  void SetNull() {}

  /**====================================
   * Helpers
   * ===================================*/

  /** Get the priority struct */
  HSHM_INLINE_CROSS_FUN LaneGroup &GetGroup(u32 prio) { return groups_[prio]; }

  /** Get a lane of the queue */
  HSHM_INLINE_CROSS_FUN Lane &GetLane(u32 prio, LaneId lane_id) {
    return GetLane(GetGroup(prio), lane_id);
  }

  /** Get a lane of the queue */
  HSHM_INLINE_CROSS_FUN Lane &GetLane(LaneGroup &lane_group, LaneId lane_id) {
    return lane_group.GetLane(lane_id);
  }

  /** Emplace a SHM pointer to a task */
  HSHM_CROSS_FUN
  bool Emplace(u32 prio, u32 lane_hash, hipc::Pointer &p) {
    return Emplace(prio, lane_hash, LaneData(p));
  }

  /** Emplace a SHM pointer to a task */
  HSHM_INLINE_CROSS_FUN
  bool Emplace(u32 prio, u32 lane_hash, const LaneData &data) {
    if (IsEmplacePlugged()) {
      WaitForEmplacePlug();
    }
    LaneGroup &lane_group = GetGroup(prio);
    LaneId lane_id = lane_hash % lane_group.num_lanes_;
    Lane &lane = GetLane(lane_group, lane_id);
    hshm::qtok_t ret = lane.emplace(data);
    return !ret.IsNull();
  }

  /**
   * Change the number of active lanes
   * This assumes that PlugForResize and UnplugForResize are called externally.
   * */
  void Resize(u32 num_lanes) {}

  /** Begin plugging the queue for resize */
  HSHM_INLINE_CROSS_FUN bool PlugForResize() { return true; }

  /** Begin plugging the queue for update tasks */
  HSHM_INLINE_CROSS_FUN bool PlugForUpdateTask() { return true; }

  /** Check if emplace operations are plugged */
  HSHM_INLINE_CROSS_FUN bool IsEmplacePlugged() {
    return flags_.Any(QUEUE_RESIZE);
  }

  /** Check if pop operations are plugged */
  HSHM_INLINE_CROSS_FUN bool IsPopPlugged() {
    return flags_.Any(QUEUE_UPDATE | QUEUE_RESIZE);
  }

  /** Wait for emplace plug to complete */
  HSHM_INLINE_CROSS_FUN
  void WaitForEmplacePlug() {
    // NOTE(llogan): will this infinite loop due to CPU caching?
    while (flags_.Any(QUEUE_UPDATE)) {
      HSHM_THREAD_MODEL->Yield();
    }
  }

  /** Enable emplace & pop */
  HSHM_INLINE void UnplugForResize() { flags_.UnsetBits(QUEUE_RESIZE); }

  /** Enable pop */
  HSHM_INLINE void UnplugForUpdateTask() { flags_.UnsetBits(QUEUE_UPDATE); }
};

}  // namespace chi::ingress

#endif  // CHI_INCLUDE_CHI_QUEUE_MANAGER_QUEUE_H_
