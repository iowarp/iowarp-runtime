//
// Created by llogan on 7/1/23.
//

#ifndef HRUN_INCLUDE_HRUN_QUEUE_MANAGER_HSHM_QUEUE_H_
#define HRUN_INCLUDE_HRUN_QUEUE_MANAGER_HSHM_QUEUE_H_

#include "chimaera/queue_manager/queue.h"
#include "mpsc_queue.h"

namespace chm {

/** The data stored in a lane */
struct LaneData {
  hipc::Pointer p_;  /**< Pointer to SHM request */

  LaneData() = default;

  LaneData(hipc::Pointer &p) {
    p_ = p;
  }
};

/** Represents a lane tasks can be stored */
typedef chm::mpsc_queue<LaneData> Lane;

/** Prioritization of different lanes in the queue */
struct LaneGroup : public PriorityInfo, public hipc::ShmContainer {
 SHM_CONTAINER_TEMPLATE((LaneGroup), (LaneGroup))
  u32 prio_;            /**< The priority of the lane group */
  u32 num_scheduled_;   /**< The number of lanes currently scheduled on workers */
  hipc::vector<Lane> lanes_;  /**< The lanes of the queue */
  u32 tether_;       /**< Lanes should be pinned to the same workers as the tether's prio group */

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  LaneGroup(Allocator *alloc) : lanes_(alloc) {}

  /** Set priority info */
  HSHM_ALWAYS_INLINE
  LaneGroup(Allocator *alloc, const PriorityInfo &priority)
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
  HSHM_ALWAYS_INLINE
  LaneGroup(Allocator *alloc, const LaneGroup &other)
  : lanes_(alloc, other.lanes_) {
    shm_init_container(alloc);
    prio_ = other.prio_;
    max_lanes_ = other.max_lanes_;
    num_lanes_ = other.num_lanes_;
    num_scheduled_ = other.num_scheduled_;
    depth_ = other.depth_;
    flags_ = other.flags_;
    tether_ = other.tether_;
  }

  /** SHM copy assignment operator */
  LaneGroup& operator=(const LaneGroup &other) {
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
  LaneGroup(hipc::Allocator *alloc,
            LaneGroup &&other) noexcept
  : lanes_(alloc, std::move(other.lanes_)) {
    shm_init_container(alloc);
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
  LaneGroup& operator=(LaneGroup &&other) noexcept {
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
  void shm_destroy_main() {
    lanes_.shm_destroy();
  }

  /** Check if the list is empty */
  HSHM_ALWAYS_INLINE
  bool IsNull() const {
    return lanes_.IsNull();
  }

  /** Sets this list as empty */
  HSHM_ALWAYS_INLINE
  void SetNull() {}

  /**====================================
   * Helpers
   * ===================================*/

  /** Check if this group is long-running or ADMIN */
  HSHM_ALWAYS_INLINE
  bool IsLowPriority() {
    return flags_.Any(QUEUE_LONG_RUNNING) || prio_ == 0;
  }

  /** Check if this group is long-running or ADMIN */
  HSHM_ALWAYS_INLINE
  bool IsLowLatency() {
    return flags_.Any(QUEUE_LOW_LATENCY);
  }

  /** Get lane */
  Lane& GetLane(LaneId lane_id) {
    return lanes_[lane_id];
  }
};

/** Represents the HSHM queue type */
class Hshm {};

/**
 * The shared-memory representation of a Queue
 * */
template<>
struct MultiQueueT<Hshm> : public hipc::ShmContainer {
  SHM_CONTAINER_TEMPLATE((MultiQueueT), (MultiQueueT))
  QueueId id_;          /**< Globally unique ID of this queue */
  hipc::vector<LaneGroup> groups_;  /**< Divide the lanes into groups */
  bitfield32_t flags_;  /**< Flags for the queue */

 public:
  /**====================================
   * Constructor
   * ===================================*/

  /** SHM constructor. Default. */
  explicit MultiQueueT(hipc::Allocator *alloc) : groups_(alloc) {
    shm_init_container(alloc);
    SetNull();
  }

  /** SHM constructor. */
  explicit MultiQueueT(hipc::Allocator *alloc, const QueueId &id,
                       const std::vector<PriorityInfo> &prios)
  : groups_(alloc, prios.size()) {
    shm_init_container(alloc);
    id_ = id;
    for (const PriorityInfo &prio_info : prios) {
      groups_.replace(groups_.begin() + prio_info.prio_, prio_info);
      LaneGroup &lane_group = groups_[prio_info.prio_];
      // Initialize lanes
      lane_group.lanes_.reserve(
          prio_info.max_lanes_);
      for (LaneId lane_id = 0;
           lane_id < lane_group.num_lanes_;
           ++lane_id) {
        lane_group.lanes_.emplace_back(lane_group.depth_, id_);
        Lane &lane = lane_group.lanes_.back();
        lane.flags_ = prio_info.flags_;
      }
    }
  }

  /**====================================
   * Copy Constructors
   * ===================================*/

  /** SHM copy constructor */
  explicit MultiQueueT(hipc::Allocator *alloc, const MultiQueueT &other)
  : groups_(alloc) {
    shm_init_container(alloc);
    SetNull();
    shm_strong_copy_construct_and_op(other);
  }

  /** SHM copy assignment operator */
  MultiQueueT& operator=(const MultiQueueT &other) {
    if (this != &other) {
      shm_destroy();
      shm_strong_copy_construct_and_op(other);
    }
    return *this;
  }

  /** SHM copy constructor + operator main */
  void shm_strong_copy_construct_and_op(const MultiQueueT &other) {
    groups_ = other.groups_;
  }

  /**====================================
   * Move Constructors
   * ===================================*/

  /** SHM move constructor. */
  MultiQueueT(hipc::Allocator *alloc,
              MultiQueueT &&other) noexcept : groups_(alloc) {
    shm_init_container(alloc);
    if (GetAllocator() == other.GetAllocator()) {
      groups_ = std::move(other.groups_);
      other.SetNull();
    } else {
      shm_strong_copy_construct_and_op(other);
      other.shm_destroy();
    }
  }

  /** SHM move assignment operator. */
  MultiQueueT& operator=(MultiQueueT &&other) noexcept {
    if (this != &other) {
      shm_destroy();
      if (GetAllocator() == other.GetAllocator()) {
        groups_ = std::move(other.groups_);
        other.SetNull();
      } else {
        shm_strong_copy_construct_and_op(other);
        other.shm_destroy();
      }
    }
    return *this;
  }

  /**====================================
   * Destructor
   * ===================================*/

  /** SHM destructor.  */
  void shm_destroy_main() {
    groups_.shm_destroy();
  }

  /** Check if the list is empty */
  HSHM_ALWAYS_INLINE
  bool IsNull() const {
    return groups_.IsNull();
  }

  /** Sets this list as empty */
  HSHM_ALWAYS_INLINE
  void SetNull() {}

  /**====================================
   * Helpers
   * ===================================*/

  /** Get the priority struct */
  HSHM_ALWAYS_INLINE LaneGroup& GetGroup(u32 prio) {
    return groups_[prio];
  }

  /** Get a lane of the queue */
  HSHM_ALWAYS_INLINE Lane& GetLane(u32 prio, LaneId lane_id) {
    return GetLane(GetGroup(prio), lane_id);
  }

  /** Get a lane of the queue */
  HSHM_ALWAYS_INLINE Lane& GetLane(LaneGroup &lane_group,
                                   LaneId lane_id) {
    return lane_group.GetLane(lane_id);
  }

  /** Emplace a SHM pointer to a task */
  HSHM_ALWAYS_INLINE
  bool Emplace(u32 prio, u32 lane_hash,
               hipc::Pointer &p) {
    return Emplace(prio, lane_hash, LaneData(p));
  }

  /** Emplace a SHM pointer to a task */
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
  void Resize(u32 num_lanes) {
  }

  /** Begin plugging the queue for resize */
  HSHM_ALWAYS_INLINE bool PlugForResize() {
    return true;
  }

  /** Begin plugging the queue for update tasks */
  HSHM_ALWAYS_INLINE bool PlugForUpdateTask() {
    return true;
  }

  /** Check if emplace operations are plugged */
  HSHM_ALWAYS_INLINE bool IsEmplacePlugged() {
    return flags_.Any(QUEUE_RESIZE);
  }

  /** Check if pop operations are plugged */
  HSHM_ALWAYS_INLINE bool IsPopPlugged() {
    return flags_.Any(QUEUE_UPDATE | QUEUE_RESIZE);
  }

  /** Wait for emplace plug to complete */
  void WaitForEmplacePlug() {
    // NOTE(llogan): will this infinite loop due to CPU caching?
    while (flags_.Any(QUEUE_UPDATE)) {
      HERMES_THREAD_MODEL->Yield();
    }
  }

  /** Enable emplace & pop */
  HSHM_ALWAYS_INLINE void UnplugForResize() {
    flags_.UnsetBits(QUEUE_RESIZE);
  }

  /** Enable pop */
  HSHM_ALWAYS_INLINE void UnplugForUpdateTask() {
    flags_.UnsetBits(QUEUE_UPDATE);
  }
};

}  // namespace chm


#endif  // HRUN_INCLUDE_HRUN_QUEUE_MANAGER_HSHM_QUEUE_H_
