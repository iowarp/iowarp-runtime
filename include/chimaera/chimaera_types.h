#ifndef CHI_CHIMAERA_TYPES_H_
#define CHI_CHIMAERA_TYPES_H_

#include <hermes_shm/data_structures/all.h>
#include <hermes_shm/types/bitfield.h>

namespace chi {

using hshm::u32;
using hshm::u64;
using hshm::size_t;

typedef hshm::u32 PoolId;
typedef hshm::u32 ContainerId;
typedef hshm::u32 QueueId;
typedef hshm::u32 LaneId;
typedef hshm::u32 MethodId;
typedef hshm::u32 TaskNode;
typedef hshm::u32 MonitorModeId;
typedef hshm::u32 SubDomainGroup;
typedef hshm::u32 SubDomainMinor;
typedef hshm::ibitfield IntFlag;

struct SubDomainId {
  SubDomainGroup major_;
  SubDomainMinor minor_;

  SubDomainId() = default;
  SubDomainId(SubDomainGroup major, SubDomainMinor minor)
      : major_(major), minor_(minor) {}

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(major_, minor_);
  }
};

struct DomainId {
  PoolId pool_id_;
  SubDomainId sub_id_;

  DomainId() = default;
  DomainId(PoolId pool_id, SubDomainId sub_id)
      : pool_id_(pool_id), sub_id_(sub_id) {}

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(pool_id_, sub_id_);
  }
};

namespace SubDomain {
static const SubDomainGroup kPhysicalNode = 0;
static const SubDomainGroup kGlobal = 1;
static const SubDomainGroup kLocal = 2;
}  // namespace SubDomain

namespace MonitorMode {
static const MonitorModeId kGlobalSchedule = 0;
static const MonitorModeId kLocalSchedule = 1;
}  // namespace MonitorMode

#define TASK_PERIODIC BIT_OPT(u32, 0)
#define TASK_FIRE_AND_FORGET BIT_OPT(u32, 1)

namespace QueuePriority {
static const u32 kLowLatency = 0;
static const u32 kHighLatency = 1;
}  // namespace QueuePriority

class DomainQuery {
public:
  enum Type {
    kLocalId,
    kGlobalId,
    kLocalHash,
    kGlobalHash,
    kGlobalBcast,
    kDynamic
  };

  Type type_;
  u32 value_;

  DomainQuery() = default;

  static DomainQuery GetLocalId(u32 id) {
    DomainQuery dq;
    dq.type_ = kLocalId;
    dq.value_ = id;
    return dq;
  }

  static DomainQuery GetGlobalId(u32 id) {
    DomainQuery dq;
    dq.type_ = kGlobalId;
    dq.value_ = id;
    return dq;
  }

  static DomainQuery GetLocalHash(u32 hash) {
    DomainQuery dq;
    dq.type_ = kLocalHash;
    dq.value_ = hash;
    return dq;
  }

  static DomainQuery GetGlobalHash(u32 hash) {
    DomainQuery dq;
    dq.type_ = kGlobalHash;
    dq.value_ = hash;
    return dq;
  }

  static DomainQuery GetGlobalBcast() {
    DomainQuery dq;
    dq.type_ = kGlobalBcast;
    dq.value_ = 0;
    return dq;
  }

  static DomainQuery GetDynamic() {
    DomainQuery dq;
    dq.type_ = kDynamic;
    dq.value_ = 0;
    return dq;
  }

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(type_, value_);
  }
};

}  // namespace chi

#define CHI_MAIN_ALLOC_T hipc::ThreadLocalAllocator
HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);

#define CHI_BEGIN(X)
#define CHI_END(X)

#endif  // CHI_CHIMAERA_TYPES_H_