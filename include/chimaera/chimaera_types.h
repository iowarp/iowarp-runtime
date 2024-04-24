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

#ifndef HRUN_INCLUDE_HRUN_HRUN_TYPES_H_
#define HRUN_INCLUDE_HRUN_HRUN_TYPES_H_

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/atomic.hpp>

#include <hermes_shm/data_structures/ipc/unordered_map.h>
#include <hermes_shm/data_structures/ipc/pod_array.h>
#include <hermes_shm/data_structures/ipc/vector.h>
#include <hermes_shm/data_structures/ipc/list.h>
#include <hermes_shm/data_structures/ipc/slist.h>
#include <hermes_shm/data_structures/data_structure.h>
#include <hermes_shm/data_structures/ipc/string.h>
#include <hermes_shm/data_structures/ipc/mpsc_queue.h>
#include <hermes_shm/data_structures/ipc/mpsc_ptr_queue.h>
#include <hermes_shm/data_structures/ipc/ticket_queue.h>
#include <hermes_shm/data_structures/containers/converters.h>
#include <hermes_shm/data_structures/containers/charbuf.h>
#include <hermes_shm/data_structures/containers/spsc_queue.h>
#include <hermes_shm/data_structures/containers/mpsc_queue.h>
#include <hermes_shm/data_structures/containers/split_ticket_queue.h>
#include <hermes_shm/data_structures/containers/converters.h>
#include "hermes_shm/data_structures/serialization/shm_serialize.h"
#include <hermes_shm/util/auto_trace.h>
#include <hermes_shm/thread/lock.h>
#include <hermes_shm/thread/thread_model_manager.h>
#include <hermes_shm/types/atomic.h>
#include "hermes_shm/util/singleton.h"
#include "hermes_shm/constants/macros.h"

#include <boost/context/fiber_fcontext.hpp>

namespace bctx = boost::context::detail;

typedef uint8_t u8;   /**< 8-bit unsigned integer */
typedef uint16_t u16; /**< 16-bit unsigned integer */
typedef uint32_t u32; /**< 32-bit unsigned integer */
typedef uint64_t u64; /**< 64-bit unsigned integer */
typedef int8_t i8;    /**< 8-bit signed integer */
typedef int16_t i16;  /**< 16-bit signed integer */
typedef int32_t i32;  /**< 32-bit signed integer */
typedef int64_t i64;  /**< 64-bit signed integer */
typedef float f32;    /**< 32-bit float */
typedef double f64;   /**< 64-bit float */

namespace chm {

using hshm::RwLock;
using hshm::Mutex;
using hshm::bitfield;
using hshm::bitfield32_t;
typedef hshm::bitfield<uint64_t> bitfield64_t;
using hshm::ScopedRwReadLock;
using hshm::ScopedRwWriteLock;
using hipc::LPointer;

typedef u32 LaneId;  /**< The ID of a lane */

/** Determine the mode that HRUN is initialized for */
enum class HrunMode {
  kNone,
  kClient,
  kServer
};

#define DOMAIN_FLAG_T static inline const int

union Affinity {
  u32 id_;
  struct {
    u32 tree_root_;
    u16 tree_depth_;
    u16 tree_idx_;
  } tree_;
  u64 int_;

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(int_);
  }
};

/**
 * Represents the scheduling domain of a task.
 * Modes:
 * 1. Node: Schedule across a set of nodes
 * 2. Processor: Schedule across a set of processors
 * 3. Node + Processor: Schedule across processors in a node
 * 4. Lane: Schedule across lanes
 * */
struct DomainQuery {
  bitfield32_t flags_;  /**< Flags indicating how to interpret id */
  Affinity node_;       /**< The range of nodes */
  Affinity lane_;       /**< The range of lanes */

  /** Mode flags */
  DOMAIN_FLAG_T kNode =
      BIT_OPT(u32, 0);    /**< Domain includes node ID */
  DOMAIN_FLAG_T kLane =
      BIT_OPT(u32, 1);    /**< Domain includes lane ID */

  /** Range flags */
  DOMAIN_FLAG_T kLocal =
      BIT_OPT(u32, 15);  /**< Use local node in scheduling decision */
  DOMAIN_FLAG_T kGlobal =
      BIT_OPT(u32, 16);  /**< Use all nodes in scheduling decision */
  DOMAIN_FLAG_T kGlobalMinusLocal =
      BIT_OPT(u32, 17);  /**< Don't use local node in scheduling decision */
  DOMAIN_FLAG_T kDirect =
      BIT_OPT(u32, 18);  /**< Use specific node/lane for decision */
  DOMAIN_FLAG_T kTree =
      BIT_OPT(u32, 19);  /**< Use specific node/lane for decision */

  /** Serialize domain id */
  template<typename Ar>
  void serialize(Ar &ar) {
    ar(flags_, major_.int_, minor_.int_, lane_hash_);
  }

  /** Default constructor. */
  HSHM_ALWAYS_INLINE
  DomainQuery() {
    major_.int_ = 0;
    minor_.int_ = 0;
    lane_hash_ = 0;
  }

  /** Copy constructor */
  HSHM_ALWAYS_INLINE
  DomainQuery(const DomainQuery &other) {
    flags_ = other.flags_;
    major_.node_id_ = other.major_.node_id_;
    minor_.cpu_id_ = other.minor_.cpu_id_;
    lane_hash_ = other.lane_hash_;
  }

  /** Copy operator */
  HSHM_ALWAYS_INLINE
  DomainQuery& operator=(const DomainQuery &other) {
    if (this != &other) {
      flags_ = other.flags_;
      major_.node_id_ = other.major_.node_id_;
      minor_.cpu_id_ = other.minor_.cpu_id_;
      lane_hash_ = other.lane_hash_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_ALWAYS_INLINE
  DomainQuery(DomainQuery &&other) noexcept {
    flags_ = other.flags_;
    major_.node_id_ = other.major_.node_id_;
    minor_.cpu_id_ = other.minor_.cpu_id_;
    lane_hash_ = other.lane_hash_;
  }

  /** Move operator */
  HSHM_ALWAYS_INLINE
  DomainQuery& operator=(DomainQuery &&other) noexcept {
    if (this != &other) {
      flags_ = other.flags_;
      major_.node_id_ = other.major_.node_id_;
      minor_.cpu_id_ = other.minor_.cpu_id_;
      lane_hash_ = other.lane_hash_;
    }
    return *this;
  }

  /** Equality operator */
  HSHM_ALWAYS_INLINE
  bool operator==(const DomainQuery &other) const {
    return flags_.bits_ == other.flags_.bits_ &&
        major_.int_ == other.major_.int_ &&
        minor_.int_ == other.minor_.int_ &&
        lane_hash_ == other.lane_hash_;
  }

  /** Inequality operator */
  HSHM_ALWAYS_INLINE
  bool operator!=(const DomainQuery &other) const {
    return flags_.bits_ == other.flags_.bits_ &&
        major_.int_ == other.major_.int_ &&
        minor_.int_ == other.minor_.int_ &&
        lane_hash_ == other.lane_hash_;
  }

  /** DomainQuery representing this processor */
  HSHM_ALWAYS_INLINE
  static DomainQuery GetLocal() {
    DomainQuery id;
    id.flags_.SetBits(kNode | kProcessor | kLocal);
    return id;
  }

  /** DomainQuery representing a specific node */
  HSHM_ALWAYS_INLINE
  static DomainQuery GetNode(u32 node_id) {
    DomainQuery id;
    id.flags_.SetBits(kNode | kDirect);
    id.major_.node_id_ = node_id;
    return id;
  }

  /** DomainQuery representing all nodes */
  HSHM_ALWAYS_INLINE
  static DomainQuery GetGlobal() {
    DomainQuery id;
    id.flags_.SetBits(kNode | kGlobal);
    return id;
  }

  /** DomainQuery representing all nodes, except this one */
  HSHM_ALWAYS_INLINE
  static DomainQuery GetGlobalMinusLocal() {
    DomainQuery id;
    id.flags_.SetBits(kGlobalMinusLocal);
    return id;
  }
};

/** Represents unique ID for states + queues */
template<int TYPE>
struct UniqueId {
  u32 node_id_;  /**< The node the content is on */
  u32 hash_;     /**< The hash of the content the ID represents */
  u64 unique_;   /**< A unique id for the blob */

  /** Serialization */
  template<typename Ar>
  void serialize(Ar &ar) {
    ar & node_id_;
    ar & hash_;
    ar & unique_;
  }

  /** Default constructor */
  HSHM_ALWAYS_INLINE
  UniqueId() = default;

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  UniqueId(u32 node_id, u64 unique)
  : node_id_(node_id), hash_(0), unique_(unique) {}

  /** Emplace constructor (+hash) */
  HSHM_ALWAYS_INLINE explicit
  UniqueId(u32 node_id, u32 hash, u64 unique)
  : node_id_(node_id), hash_(hash), unique_(unique) {}

  /** Copy constructor */
  HSHM_ALWAYS_INLINE
  UniqueId(const UniqueId &other) {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Copy constructor */
  template<int OTHER_TYPE=TYPE>
  HSHM_ALWAYS_INLINE
  UniqueId(const UniqueId<OTHER_TYPE> &other) {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Copy assignment */
  HSHM_ALWAYS_INLINE
  UniqueId& operator=(const UniqueId &other) {
    if (this != &other) {
      node_id_ = other.node_id_;
      hash_ = other.hash_;
      unique_ = other.unique_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_ALWAYS_INLINE
  UniqueId(UniqueId &&other) noexcept {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Move assignment */
  HSHM_ALWAYS_INLINE
  UniqueId& operator=(UniqueId &&other) noexcept {
    if (this != &other) {
      node_id_ = other.node_id_;
      hash_ = other.hash_;
      unique_ = other.unique_;
    }
    return *this;
  }

  /** Check if null */
  [[nodiscard]]
  HSHM_ALWAYS_INLINE bool IsNull() const {
    return node_id_ == 0;
  }

  /** Get null id */
  HSHM_ALWAYS_INLINE
  static UniqueId GetNull() {
    static const UniqueId id(0, 0);
    return id;
  }

  /** Set to null id */
  HSHM_ALWAYS_INLINE
  void SetNull() {
    node_id_ = 0;
    hash_ = 0;
    unique_ = 0;
  }

  /** Get id of node from this id */
  [[nodiscard]]
  HSHM_ALWAYS_INLINE
  u32 GetNodeId() const { return node_id_; }

  /** Compare two ids for equality */
  HSHM_ALWAYS_INLINE
  bool operator==(const UniqueId &other) const {
    return unique_ == other.unique_ && node_id_ == other.node_id_;
  }

  /** Compare two ids for inequality */
  HSHM_ALWAYS_INLINE
  bool operator!=(const UniqueId &other) const {
    return unique_ != other.unique_ || node_id_ != other.node_id_;
  }

  friend std::ostream& operator<<(std::ostream &os, const UniqueId &id) {
    return os << (std::to_string(id.node_id_) + "."
        + std::to_string(id.unique_));
  }
};

/** Uniquely identify a task state */
using TaskStateId = UniqueId<1>;
/** Uniquely identify a queue */
using QueueId = UniqueId<2>;
/** Uniquely identify a task */
using TaskId = UniqueId<3>;

/** Stateful lane ID */
struct StateLaneId {
  TaskStateId state_id_;
  LaneId lane_id_;

  /** Serialization */
  template<typename Ar>
  void serialize(Ar &ar) {
    ar(state_id_, lane_id_);
  }

  /** Equality operator */
  HSHM_ALWAYS_INLINE
  bool operator==(const StateLaneId &other) const {
    return state_id_ == other.state_id_ && lane_id_ == other.lane_id_;
  }

  /** Inequality operator */
  HSHM_ALWAYS_INLINE
  bool operator!=(const StateLaneId &other) const {
    return state_id_ != other.state_id_ || lane_id_ != other.lane_id_;
  }
};

/** The types of I/O that can be performed (for IoCall RPC) */
enum class IoType {
  kRead,
  kWrite,
  kNone
};

}  // namespace chm

namespace std {

/** Hash function for UniqueId */
template <int TYPE>
struct hash<chm::UniqueId<TYPE>> {
  HSHM_ALWAYS_INLINE
  std::size_t operator()(const chm::UniqueId<TYPE> &key) const {
    return
      std::hash<u64>{}(key.unique_) +
      std::hash<u32>{}(key.node_id_);
  }
};

/** Hash function for DomainQuery */
template<>
struct hash<chm::DomainQuery> {
  HSHM_ALWAYS_INLINE
  std::size_t operator()(const chm::DomainQuery &key) const {
    return
        std::hash<u32>{}(key.GetId()) +
        std::hash<u32>{}(key.flags_.bits_);
  }
};

/** Hash function for StateLaneId */
template<>
struct hash<chm::StateLaneId> {
  HSHM_ALWAYS_INLINE
  std::size_t operator()(const chm::StateLaneId &key) const {
    return
        std::hash<chm::TaskStateId>{}(key.state_id_) +
        std::hash<chm::LaneId>{}(key.lane_id_);
  }
};

}  // namespace std

#endif  // HRUN_INCLUDE_HRUN_HRUN_TYPES_H_
