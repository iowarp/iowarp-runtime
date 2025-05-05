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

#ifndef CHI_INCLUDE_CHI_CHI_TYPES_H_
#define CHI_INCLUDE_CHI_CHI_TYPES_H_

#include <hermes_shm/constants/macros.h>

#include <cereal/archives/binary.hpp>
#include <cereal/types/atomic.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/vector.hpp>

// Undefine default allocator
#ifdef HSHM_DEFAULT_ALLOC_T
#undef HSHM_DEFAULT_ALLOC_T
#endif

// #define HSHM_DEFAULT_ALLOC_T hipc::ScalablePageAllocator
#define HSHM_DEFAULT_ALLOC_T hipc::ThreadLocalAllocator
#define CHI_ALLOC_T HSHM_DEFAULT_ALLOC_T
#define CHI_SHM_GPU_ALLOC_T CHI_ALLOC_T
// #define CHI_DATA_GPU_ALLOC_T hipc::GpuStackAllocator
#define CHI_DATA_GPU_ALLOC_T hipc::StackAllocator

#define CHI_MAIN_ALLOC_T CHI_ALLOC_T
#ifdef HSHM_IS_HOST
#define CHI_DATA_ALLOC_T CHI_ALLOC_T
#else
#define CHI_DATA_ALLOC_T CHI_DATA_GPU_ALLOC_T
#endif
#define CHI_RDATA_ALLOC_T CHI_DATA_ALLOC_T

#include <hermes_shm/data_structures/all.h>
#include <hermes_shm/memory/allocator/allocator_factory_.h>
#include <hermes_shm/thread/lock.h>
#include <hermes_shm/thread/thread_model_manager.h>
#include <hermes_shm/types/atomic.h>
#include <hermes_shm/util/auto_trace.h>
#include <hermes_shm/util/config_parse.h>
#include <hermes_shm/util/singleton.h>

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

HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);
HSHM_DATA_STRUCTURES_TEMPLATE(chi::data, CHI_DATA_ALLOC_T);

namespace chi {

#define CHI_LANE_SIZE 8192

using hshm::bitfield;
using hshm::bitfield16_t;
using hshm::bitfield8_t;
using hshm::ibitfield;

using hshm::abitfield;
using hshm::abitfield16_t;
using hshm::abitfield8_t;
using hshm::aibitfield;

typedef int IntFlag;

using hshm::Mutex;
using hshm::RwLock;
typedef hshm::bitfield<uint64_t> bitfield64_t;
using hipc::FullPtr;
using hshm::ScopedMutex;
using hshm::ScopedRwReadLock;
using hshm::ScopedRwWriteLock;

typedef u32 NodeId;        /**< The ID of a node */
typedef u32 LaneId;        /**< The ID of a lane */
typedef u32 LaneGroupId;   /**< The ID of a lane group */
typedef u32 ContainerId;   /**< The ID of a container */
typedef u32 WorkerId;      /**< The ID of a worker */
typedef u32 MethodId;      /**< The ID of a container method */
typedef u32 MonitorModeId; /**< The ID of a container monitor mode */
typedef u32 TaskPrio;      /**< The priority of a task */

/** Represents unique ID for states + queues */
template <int TYPE>
struct UniqueId {
  union {
    NodeId node_id_;       /**< The node the content is on */
    LaneGroupId group_id_; /**< The group the content is on */
    TaskPrio prio_;        /**< The priority of the lane */
  };
  u32 hash_;   /**< The hash of the content the ID represents */
  u64 unique_; /**< A unique id within the node */

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar & node_id_;
    ar & hash_;
    ar & unique_;
  }

  /** Default constructor */
  constexpr HSHM_INLINE_CROSS_FUN UniqueId() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~UniqueId() = default;

  /** Emplace constructor */
  constexpr HSHM_INLINE_CROSS_FUN explicit UniqueId(NodeId node_id, u64 unique)
      : node_id_(node_id), hash_(0), unique_(unique) {}

  /** Emplace constructor (+hash) */
  constexpr HSHM_INLINE_CROSS_FUN explicit UniqueId(NodeId node_id, u32 hash,
                                                    u64 unique)
      : node_id_(node_id), hash_(hash), unique_(unique) {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  UniqueId(const UniqueId &other) {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Copy constructor */
  template <int OTHER_TYPE = TYPE>
  HSHM_INLINE_CROSS_FUN UniqueId(const UniqueId<OTHER_TYPE> &other) {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN
  UniqueId &operator=(const UniqueId &other) {
    if (this != &other) {
      node_id_ = other.node_id_;
      hash_ = other.hash_;
      unique_ = other.unique_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  UniqueId(UniqueId &&other) noexcept {
    node_id_ = other.node_id_;
    hash_ = other.hash_;
    unique_ = other.unique_;
  }

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN
  UniqueId &operator=(UniqueId &&other) noexcept {
    if (this != &other) {
      node_id_ = other.node_id_;
      hash_ = other.hash_;
      unique_ = other.unique_;
    }
    return *this;
  }

  /** Check if null */
  [[nodiscard]]
  HSHM_INLINE_CROSS_FUN bool IsNull() const {
    return node_id_ == 0;
  }

  /** Get null id */
  HSHM_INLINE_CROSS_FUN
  static UniqueId GetNull() { return UniqueId(0, 0); }

  /** Set to null id */
  HSHM_INLINE_CROSS_FUN
  void SetNull() {
    node_id_ = 0;
    hash_ = 0;
    unique_ = 0;
  }

  /** Get id of node from this id */
  [[nodiscard]]
  HSHM_INLINE_CROSS_FUN u32 GetNodeId() const {
    return node_id_;
  }

  /** Compare two ids for equality */
  HSHM_INLINE_CROSS_FUN
  bool operator==(const UniqueId &other) const {
    return unique_ == other.unique_ && node_id_ == other.node_id_;
  }

  /** Compare two ids for inequality */
  HSHM_INLINE_CROSS_FUN
  bool operator!=(const UniqueId &other) const {
    return unique_ != other.unique_ || node_id_ != other.node_id_;
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os, const UniqueId &id) {
    return os << (std::to_string(id.node_id_) + "." +
                  std::to_string(id.unique_)) +
                     "::" + std::to_string(id.hash_);
  }

  /** Hash function */
  HSHM_INLINE_CROSS_FUN
  size_t Hash() const {
    return hshm::hash<u64>{}(unique_) + hshm::hash<u32>{}(node_id_);
  }
};

/** Uniquely identify a pool */
using PoolId = UniqueId<1>;
/** Uniquely identify a queue */
using QueueId = UniqueId<2>;
/** Uniquely identify a task */
using TaskId = UniqueId<3>;

/** The types of I/O that can be performed (for IoCall RPC) */
enum class IoType { kRead, kWrite, kNone };

/** Context used for creating objects */
struct CreateContext {
  PoolId id_ = PoolId::GetNull();
  u32 global_containers_ = 0;
  u32 local_containers_pn_ = 0;
  u32 lanes_per_container_ = 0;

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(id_, global_containers_, local_containers_pn_, lanes_per_container_);
  }
};

/** Major identifier of subdomain */
typedef u32 SubDomainGroup;

/** Minor identifier of subdomain */
typedef u32 SubDomainMinor;

/** Various subdomain constants */
namespace SubDomain {
static GLOBAL_CROSS_CONST SubDomainGroup kPhysicalNode = 0;
static GLOBAL_CROSS_CONST SubDomainGroup kContainerSet = 1;
static GLOBAL_CROSS_CONST SubDomainGroup kGlobalContainers =
    1;  // Alias for kContainerSet
static GLOBAL_CROSS_CONST SubDomainGroup kLocalContainers = 3;
static GLOBAL_CROSS_CONST SubDomainGroup kLast = 4;
}  // namespace SubDomain

/** An unscoped subdomain of nodes or lanes */
struct SubDomainId {
  SubDomainGroup major_; /**< NodeSet, ContainerSet, ... */
  SubDomainMinor minor_; /**< NodeId, ContainerId, ... */

  /** Subdomain major groups */
  CLS_CONST SubDomainGroup kPhysicalNode = 0;
  CLS_CONST SubDomainGroup kContainerSet = 1;
  CLS_CONST SubDomainGroup kGlobalContainers = 1;  // Alias for kContainerSet
  CLS_CONST SubDomainGroup kLocalContainers = 3;
  CLS_CONST SubDomainGroup kLast = 4;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainId() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~SubDomainId() = default;

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainId(SubDomainGroup major, SubDomainMinor minor) {
    major_ = major;
    minor_ = minor;
  }

  /** Emplace constructor (2) */
  HSHM_INLINE_CROSS_FUN
  SubDomainId(SubDomainGroup major) {
    major_ = major;
    minor_ = 0;
  }

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainId(const SubDomainId &other) {
    major_ = other.major_;
    minor_ = other.minor_;
  }

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN
  SubDomainId &operator=(const SubDomainId &other) {
    if (this != &other) {
      major_ = other.major_;
      minor_ = other.minor_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainId(SubDomainId &&other) noexcept {
    major_ = other.major_;
    minor_ = other.minor_;
  }

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN
  SubDomainId &operator=(SubDomainId &&other) noexcept {
    if (this != &other) {
      major_ = other.major_;
      minor_ = other.minor_;
    }
    return *this;
  }

  /** ID represents a physical node ID */
  HSHM_INLINE_CROSS_FUN
  bool IsPhysical() const { return major_ == kPhysicalNode; }

  /** ID represents a major+minor ID */
  HSHM_INLINE_CROSS_FUN
  bool IsMajor() const { return minor_ == 0; }

  /** ID represents a major ID */
  HSHM_INLINE_CROSS_FUN
  bool IsMinor() const { return minor_ > 0; }

  /** Create a physical ID subdomain */
  HSHM_INLINE_CROSS_FUN
  static SubDomainId CreatePhysical(SubDomainMinor minor) {
    return {kPhysicalNode, minor};
  }

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(major_, minor_);
  }

  /** Equality operator */
  HSHM_INLINE_CROSS_FUN
  bool operator==(const SubDomainId &other) const {
    return major_ == other.major_ && minor_ == other.minor_;
  }

  /** Hash function */
  HSHM_INLINE_CROSS_FUN
  size_t Hash() const {
    return hshm::hash<u32>{}(major_) + hshm::hash<u32>{}(minor_);
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os,
                                  const SubDomainId &subdom_id) {
    return os << subdom_id.major_ << "." << subdom_id.minor_;
  }
};

/** Represents a scoped domain */
struct DomainId {
  PoolId scope_;
  SubDomainId sub_id_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  DomainId() = default;

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  DomainId(const PoolId &scope, const SubDomainId &sub_id) {
    scope_ = scope;
    sub_id_ = sub_id;
  }

  /** Emplace constructor (2) */
  HSHM_INLINE_CROSS_FUN
  DomainId(const PoolId &scope, const SubDomainGroup &group) {
    scope_ = scope;
    sub_id_ = {group};
  }

  /** Emplace constructor (3) */
  HSHM_INLINE_CROSS_FUN
  DomainId(const PoolId &scope, const SubDomainGroup &group,
           const SubDomainMinor &minor) {
    scope_ = scope;
    sub_id_ = {group, minor};
  }

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  DomainId(const DomainId &other) {
    scope_ = other.scope_;
    sub_id_ = other.sub_id_;
  }

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN
  DomainId &operator=(const DomainId &other) {
    scope_ = other.scope_;
    sub_id_ = other.sub_id_;
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  DomainId(DomainId &&other) noexcept {
    scope_ = other.scope_;
    sub_id_ = other.sub_id_;
  }

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN
  DomainId &operator=(DomainId &&other) noexcept {
    scope_ = other.scope_;
    sub_id_ = other.sub_id_;
    return *this;
  }

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(scope_, sub_id_);
  }

  /** Equality operator */
  HSHM_INLINE_CROSS_FUN
  bool operator==(const DomainId &other) const {
    return scope_ == other.scope_ && sub_id_ == other.sub_id_;
  }

  /** Hash function */
  HSHM_INLINE_CROSS_FUN
  size_t Hash() const { return scope_.Hash() + sub_id_.Hash(); }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os, const DomainId &subdom_id) {
    return os << subdom_id.scope_ << "." << subdom_id.sub_id_;
  }
};

/** Select a region of a domain */
union DomainSelection {
  u32 hash_;
  u32 id_;
  struct {
    u32 off_;
    u32 count_;
  } range_;
  u64 int_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  DomainSelection() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~DomainSelection() = default;

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(int_);
  }

  /** Equality operator */
  HSHM_INLINE_CROSS_FUN
  bool operator==(const DomainSelection &other) const {
    return int_ == other.int_;
  }
};

/** Operations that can be performed on a domain */
enum class UpdateDomainOp { kContract, kExpand };

/** Subdomain ID range */
struct SubDomainIdRange {
  SubDomainGroup group_;
  u32 off_, count_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~SubDomainIdRange() = default;

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange(SubDomainGroup group, u32 off, u32 count) {
    group_ = group;
    off_ = off;
    count_ = count;
  }

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange(const SubDomainIdRange &other) {
    group_ = other.group_;
    off_ = other.off_;
    count_ = other.count_;
  }

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange &operator=(const SubDomainIdRange &other) {
    if (this != &other) {
      group_ = other.group_;
      off_ = other.off_;
      count_ = other.count_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange(SubDomainIdRange &&other) noexcept {
    group_ = other.group_;
    off_ = other.off_;
    count_ = other.count_;
  }

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN
  SubDomainIdRange &operator=(SubDomainIdRange &&other) noexcept {
    if (this != &other) {
      group_ = other.group_;
      off_ = other.off_;
      count_ = other.count_;
    }
    return *this;
  }

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(group_, off_, count_);
  }
};

/** Info required for updating a domain */
struct UpdateDomainInfo {
  DomainId domain_id_;
  UpdateDomainOp op_;
  SubDomainIdRange range_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  UpdateDomainInfo() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~UpdateDomainInfo() = default;

  /** serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(domain_id_, op_, range_);
  }
};

/** Flags for domain query */
typedef u32 DomainFlag;

/**
 * Represents the scheduling domain of a task.
 * */
struct DomainQuery {
  bitfield<DomainFlag> flags_; /**< Flags */
  SubDomainGroup sub_id_;      /**< The subdomain to query */
  DomainSelection sel_;        /**< The subset of the subdomain to query */

  /** Range flags */
  CLS_CONST DomainFlag kLocal = BIT_OPT(DomainFlag, 1);
  CLS_CONST DomainFlag kDirect = BIT_OPT(DomainFlag, 2);
  CLS_CONST DomainFlag kGlobal = BIT_OPT(DomainFlag, 3);
  CLS_CONST DomainFlag kSchedule = BIT_OPT(DomainFlag, 4);

  /** Selection flags */
  CLS_CONST DomainFlag kId = BIT_OPT(DomainFlag, 8);
  CLS_CONST DomainFlag kHash = BIT_OPT(DomainFlag, 9);
  CLS_CONST DomainFlag kRange = BIT_OPT(DomainFlag, 10);

  /** Iteration algos */
  CLS_CONST DomainFlag kBroadcast = BIT_OPT(DomainFlag, 15);
  CLS_CONST DomainFlag kRepUntilSuccess = BIT_OPT(DomainFlag, 18);
  CLS_CONST DomainFlag kChooseOne = BIT_OPT(DomainFlag, 19);
  CLS_CONST DomainFlag kForwardToLeader = BIT_OPT(DomainFlag, 17);

  /** Serialize domain id */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(flags_, sub_id_, sel_);
  }

  /** Get iteration flags */
  HSHM_INLINE_CROSS_FUN
  DomainFlag GetIterFlags() const {
    return flags_.bits_ &
           (kBroadcast | kRepUntilSuccess | kChooseOne | kForwardToLeader);
  }

  /** Default constructor. */
  HSHM_INLINE_CROSS_FUN
  DomainQuery() {}

  /** Destruct */
  HSHM_INLINE_CROSS_FUN
  ~DomainQuery() {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  DomainQuery(const DomainQuery &other) {
    flags_ = other.flags_;
    sub_id_ = other.sub_id_;
    sel_ = other.sel_;
  }

  /** Copy operator */
  HSHM_INLINE_CROSS_FUN
  DomainQuery &operator=(const DomainQuery &other) {
    if (this != &other) {
      flags_ = other.flags_;
      sub_id_ = other.sub_id_;
      sel_ = other.sel_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  DomainQuery(DomainQuery &&other) noexcept {
    flags_ = other.flags_;
    sub_id_ = other.sub_id_;
    sel_ = other.sel_;
  }

  /** Move operator */
  HSHM_INLINE_CROSS_FUN
  DomainQuery &operator=(DomainQuery &&other) noexcept {
    if (this != &other) {
      flags_ = other.flags_;
      sub_id_ = other.sub_id_;
      sel_ = other.sel_;
    }
    return *this;
  }

  /** Equality operator */
  HSHM_INLINE_CROSS_FUN
  bool operator==(const DomainQuery &other) const {
    return flags_.bits_ == other.flags_.bits_ && sub_id_ == other.sub_id_ &&
           sel_ == other.sel_;
  }

  /** Get the local node domain */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetLocalId(const SubDomainGroup &sub_id, u32 id) {
    DomainQuery query;
    query.flags_.SetBits(kLocal | kId);
    query.sub_id_ = sub_id;
    query.sel_.id_ = id;
    return query;
  }

  /**
   * The scope of the query is unkown and will be determined dynamically
   * This assumes the module dev has implemented the Monitor(kSchedule) method.
   */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetDynamic() {
    DomainQuery query = GetDirectHash(SubDomain::kLocalContainers, 0);
    query.flags_.SetBits(kSchedule);
    return query;
  }

  /** Check if this query is dynamic */
  HSHM_INLINE_CROSS_FUN
  bool IsDynamic() const { return flags_.Any(kSchedule); }

  /**
   * The scope of the query is the entire subdomain
   * @param sub_id The subdomain to query
   * @param iter_flags The iteration flags to set (e.g., kBroadcast)
   * */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetGlobal(const SubDomainGroup &sub_id, u32 iter_flags) {
    DomainQuery query;
    query.flags_.SetBits(kGlobal | iter_flags);
    query.sub_id_ = sub_id;
    return query;
  }

  /**
   * The scope of the query is a subset of the domain
   * @param sub_id The subdomain to query
   * @param iter_flags The iteration flags to set (e.g., kBroadcast)
   * */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetRange(const SubDomainGroup &sub_id, u32 off, u32 count,
                              u32 iter_flags) {
    DomainQuery query;
    query.flags_.SetBits(kGlobal | kRange | iter_flags);
    query.sub_id_ = sub_id;
    query.sel_.range_.off_ = off;
    query.sel_.range_.count_ = count;
    return query;
  }

  /**
   * Get local hash
   */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetLocalHash(u32 hash) {
    return GetDirectHash(SubDomain::kLocalContainers, hash);
  }

  /**
   * The scope of the query is the entire set of containers
   * */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetGlobalBcast() {
    DomainQuery query;
    query.flags_.SetBits(kGlobal | kBroadcast);
    query.sub_id_ = SubDomain::kGlobalContainers;
    return query;
  }

  /**
   * Id the query to an offset in the subdomain vector
   * @param sub_id The subdomain to query
   * @param id The ID to resolve in the subdomain
   * @param flags The iteration flags to set (e.g., kBroadcast)
   * */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetDirectId(const SubDomainGroup &sub_id, u32 id,
                                 u32 iter_flags = kChooseOne) {
    DomainQuery query;
    query.flags_.SetBits(kDirect | kId | iter_flags);
    query.sub_id_ = sub_id;
    query.sel_.id_ = id;
    return query;
  }

  /**
   * Hash the query to an offset in the subdomain vector
   * @param sub_id The subdomain to query
   * @param hash The offset hash to resolve in the subdomain
   * @param flags The iteration flags to set (e.g., kBroadcast)
   * */
  HSHM_INLINE_CROSS_FUN
  static DomainQuery GetDirectHash(const SubDomainGroup &sub_id, u32 hash,
                                   u32 iter_flags = kBroadcast) {
    DomainQuery query;
    query.flags_.SetBits(kDirect | kHash | iter_flags);
    query.sub_id_ = sub_id;
    query.sel_.hash_ = hash;
    return query;
  }

  /** Hash function */
  HSHM_INLINE_CROSS_FUN
  size_t Hash() const {
    return hshm::hash<DomainFlag>{}(flags_.bits_) +
           hshm::hash<SubDomainGroup>{}(sub_id_) + hshm::hash<u64>{}(sel_.int_);
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os,
                                  const DomainQuery &dom_query) {
    // Get scope string
    std::string scope;
    if (dom_query.flags_.Any(kLocal)) {
      scope = "local";
    } else if (dom_query.flags_.Any(kGlobal)) {
      scope = "global";
    } else if (dom_query.flags_.Any(kDirect)) {
      scope = "direct";
    }

    // Get iteration string
    std::string iter;
    if (dom_query.flags_.Any(kBroadcast)) {
      iter = "broadcast";
    } else if (dom_query.flags_.Any(kRepUntilSuccess)) {
      iter = "rep_until_success";
    } else if (dom_query.flags_.Any(kChooseOne)) {
      iter = "choose_one";
    } else if (dom_query.flags_.Any(kForwardToLeader)) {
      iter = "forward_to_leader";
    }

    // Get selection string
    std::string sel;
    if (dom_query.flags_.Any(kId)) {
      sel = "id";
    } else if (dom_query.flags_.Any(kHash)) {
      sel = "hash";
    } else if (dom_query.flags_.Any(kRange)) {
      sel = "range";
    }

    // Get range string
    std::string range;
    if (dom_query.flags_.Any(kRange)) {
      range = std::to_string(dom_query.sel_.range_.off_) + ":" +
              std::to_string(dom_query.sel_.range_.count_);
    } else if (dom_query.flags_.Any(kId)) {
      range = std::to_string(dom_query.sel_.id_);
    } else if (dom_query.flags_.Any(kHash)) {
      range = std::to_string(dom_query.sel_.hash_);
    }

    std::string format = hshm::Formatter::format(
        "DomainQuery(iter_algo={}, scope={}, sel={}, range={}, subdom={})",
        iter, scope, sel, range, dom_query.sub_id_);
    return os << format;
  }
};

/**
 * Represents a resolved domain query
 * */
struct ResolvedDomainQuery {
  NodeId node_;
  DomainQuery dom_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery() {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery(NodeId node, const DomainQuery &dom)
      : node_(node), dom_(dom) {}

  /** Emplace constructor (2) */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery(NodeId node) : node_(node) {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery(const ResolvedDomainQuery &other) {
    node_ = other.node_;
    dom_ = other.dom_;
  }

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery &operator=(const ResolvedDomainQuery &other) {
    if (this != &other) {
      node_ = other.node_;
      dom_ = other.dom_;
    }
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery(ResolvedDomainQuery &&other) noexcept {
    node_ = other.node_;
    dom_ = other.dom_;
  }

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN
  ResolvedDomainQuery &operator=(ResolvedDomainQuery &&other) noexcept {
    if (this != &other) {
      node_ = other.node_;
      dom_ = other.dom_;
    }
    return *this;
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os,
                                  const ResolvedDomainQuery &id) {
    return os << std::to_string(id.node_) << " / " << id.dom_;
  }

  /** Serialize */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(node_, dom_);
  }
};

class CacheTimer {
 public:
  hshm::Timer timer_;
  size_t cur_ns_ = 0;
  size_t start_ns_ = 0;
  size_t net_ns_ = 0;

 public:
  CacheTimer() { timer_.Now(); }

  void Wrap(CacheTimer &other) { cur_ns_ = other.cur_ns_; }

  void Refresh() { cur_ns_ = timer_.GetNsecFromStart(); }

  void Tick(size_t nanosec) { cur_ns_ += nanosec; }

  size_t GetNsecFromStart() { return cur_ns_ - start_ns_; }

  size_t GetNsecFromStart(size_t start) {
    if (start < cur_ns_) {
      return cur_ns_ - start;
    }
    return 0;
  }

  size_t GetNsec() { return net_ns_; }

  void Resume() { start_ns_ = cur_ns_; }

  void Pause() {
    net_ns_ += cur_ns_ - start_ns_;
    start_ns_ = cur_ns_;
  }

  void Reset() {
    start_ns_ = cur_ns_;
    net_ns_ = 0;
  }
};

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

  /** Check if unset */
  bool CalculateLoad() {
    // return cpu_load_ == 0 && mem_load_ == 0 && io_load_ == 0;
    return true;
  }

  /** Serialization */
  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(cpu_load_, mem_load_, io_load_);
  }
};

struct LaneStats {
  Load load_;
  size_t num_tasks_;
  size_t lane_depth_;
  LaneId lane_id_;

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(load_, num_tasks_, lane_depth_, lane_id_);
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os, const LaneStats &stats) {
    return os << "LaneStats(lane_id=" << stats.lane_id_
              << ", num_tasks=" << stats.num_tasks_
              << ", lane_depth=" << stats.lane_depth_
              << ", load={cpu: " << stats.load_.cpu_load_
              << ", mem: " << stats.load_.mem_load_
              << ", io: " << stats.load_.io_load_ << "})";
  }
};

struct WorkerStats {
  int worker_id_;
  int num_tasks_;
  chi::ipc::vector<LaneStats> lanes_;

  WorkerStats() = default;
  WorkerStats(hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : lanes_(alloc) {}

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(worker_id_, num_tasks_, lanes_);
  }

  /** Print operator */
  friend std::ostream &operator<<(std::ostream &os, const WorkerStats &stats) {
    os << "WorkerStats(worker_id=" << stats.worker_id_
       << ", num_tasks=" << stats.num_tasks_ << ", lanes=[";
    for (size_t i = 0; i < stats.lanes_.size(); ++i) {
      if (i > 0) os << ", ";
      os << stats.lanes_[i];
    }
    return os << "])";
  }
};

}  // namespace chi

namespace hshm {

/** Hash function for UniqueId */
template <int TYPE>
struct hash<chi::UniqueId<TYPE>> {
  HSHM_INLINE
  std::size_t operator()(const chi::UniqueId<TYPE> &key) const {
    return key.Hash();
  }
};

/** Hash function for SubDomainId */
template <>
struct hash<chi::SubDomainId> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::SubDomainId &key) const {
    return key.Hash();
  }
};

/** Hash function for DomainId */
template <>
struct hash<chi::DomainId> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::DomainId &key) const { return key.Hash(); }
};

/** Hash function for DomainQuery */
template <>
struct hash<chi::DomainQuery> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::DomainQuery &key) const {
    return key.Hash();
  }
};

}  // namespace hshm

namespace std {

/** Hash function for UniqueId */
template <int TYPE>
struct hash<chi::UniqueId<TYPE>> {
  HSHM_INLINE
  std::size_t operator()(const chi::UniqueId<TYPE> &key) const {
    return key.Hash();
  }
};

/** Hash function for SubDomainId */
template <>
struct hash<chi::SubDomainId> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::SubDomainId &key) const {
    return key.Hash();
  }
};

/** Hash function for DomainId */
template <>
struct hash<chi::DomainId> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::DomainId &key) const { return key.Hash(); }
};

/** Hash function for DomainQuery */
template <>
struct hash<chi::DomainQuery> {
  HSHM_INLINE_CROSS_FUN
  std::size_t operator()(const chi::DomainQuery &key) const {
    return key.Hash();
  }
};

}  // namespace std

#endif  // CHI_INCLUDE_CHI_CHI_TYPES_H_
