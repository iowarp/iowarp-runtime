#ifndef CHIMAERA_INCLUDE_CHIMAERA_TYPES_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TYPES_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <thread>

// Main HSHM include
#include <hermes_shm/hermes_shm.h>

// Boost Fiber includes
#include <boost/context/fiber_fcontext.hpp>

/**
 * Core type definitions for Chimaera distributed task execution framework
 */

namespace chi {

// Basic type aliases using HSHM types
using u32 = hshm::u32;
using u64 = hshm::u64;
using ibitfield = hshm::ibitfield;

// Time unit constants for period conversions (divisors from nanoseconds)
constexpr double kNano = 1.0;                    // 1 nanosecond
constexpr double kMicro = 1000.0;               // 1000 nanoseconds = 1 microsecond
constexpr double kMilli = 1000000.0;            // 1,000,000 nanoseconds = 1 millisecond  
constexpr double kSec = 1000000000.0;           // 1,000,000,000 nanoseconds = 1 second
constexpr double kMin = 60000000000.0;          // 60 seconds = 1 minute
constexpr double kHour = 3600000000000.0;       // 3600 seconds = 1 hour

// Forward declarations
class Task;
class DomainQuery;
class Worker;
class WorkOrchestrator;
class PoolManager;
class IpcManager;
class ConfigManager;
class ModuleManager;
class Chimaera;

/**
 * Unique identifier with major and minor components
 * Serializable and supports null values
 */
struct UniqueId {
  u32 major_;
  u32 minor_;

  constexpr UniqueId() : major_(0), minor_(0) {}
  constexpr UniqueId(u32 major, u32 minor) : major_(major), minor_(minor) {}

  // Equality operators
  bool operator==(const UniqueId& other) const {
    return major_ == other.major_ && minor_ == other.minor_;
  }

  bool operator!=(const UniqueId& other) const {
    return !(*this == other);
  }

  // Comparison operators for ordering
  bool operator<(const UniqueId& other) const {
    if (major_ != other.major_) return major_ < other.major_;
    return minor_ < other.minor_;
  }

  // Convert to u64 for compatibility and hashing
  u64 ToU64() const {
    return (static_cast<u64>(major_) << 32) | static_cast<u64>(minor_);
  }

  // Create from u64
  static UniqueId FromU64(u64 value) {
    return UniqueId(static_cast<u32>(value >> 32), static_cast<u32>(value & 0xFFFFFFFF));
  }

  // Get null/invalid instance
  static constexpr UniqueId GetNull() {
    return UniqueId(0, 0);
  }

  // Check if this is a null/invalid ID
  bool IsNull() const {
    return major_ == 0 && minor_ == 0;
  }

  // Serialization support
  template<typename Ar>
  void serialize(Ar& ar) {
    ar(major_, minor_);
  }
};

/**
 * Pool identifier inheriting from UniqueId
 */
struct PoolId : public UniqueId {
  constexpr PoolId() : UniqueId() {}
  constexpr PoolId(u32 major, u32 minor) : UniqueId(major, minor) {}
  constexpr PoolId(const UniqueId& uid) : UniqueId(uid) {}

  // Backward compatibility with u32
  constexpr PoolId(u32 simple_id) : UniqueId(simple_id, 0) {}
  operator u32() const { return major_; }  // For backward compatibility

  // Increment operators for pool ID generation
  PoolId& operator++() {  // prefix ++
    ++major_;
    return *this;
  }
  
  PoolId operator++(int) {  // postfix ++
    PoolId temp(*this);
    ++major_;
    return temp;
  }

  // Static methods
  static constexpr PoolId GetNull() {
    return PoolId(UniqueId::GetNull());
  }
};

// Task and method identifiers
using TaskNode = u32;
using MethodId = u32;

// Worker and Lane identifiers
using WorkerId = u32;
using LaneId = u32;

// Domain system types
using SubDomainGroup = u32;
using SubDomainMinor = u32;

/**
 * Predefined subdomain groups
 */
namespace SubDomain {
static constexpr SubDomainGroup kPhysicalNode = 0;
static constexpr SubDomainGroup kGlobal = 1;
static constexpr SubDomainGroup kLocal = 2;
}  // namespace SubDomain

/**
 * Subdomain identifier containing major and minor components
 */
struct SubDomainId {
  SubDomainGroup major_;
  SubDomainMinor minor_;

  SubDomainId() : major_(0), minor_(0) {}
  SubDomainId(SubDomainGroup major, SubDomainMinor minor)
      : major_(major), minor_(minor) {}

  // Equality operator
  bool operator==(const SubDomainId& other) const {
    return major_ == other.major_ && minor_ == other.minor_;
  }

  // Inequality operator
  bool operator!=(const SubDomainId& other) const {
    return !(*this == other);
  }
};

/**
 * Complete domain identifier including pool and subdomain
 */
struct DomainId {
  PoolId pool_id_;
  SubDomainId sub_id_;

  DomainId() : pool_id_(0), sub_id_() {}
  DomainId(PoolId pool_id, const SubDomainId& sub_id)
      : pool_id_(pool_id), sub_id_(sub_id) {}

  // Equality operator
  bool operator==(const DomainId& other) const {
    return pool_id_ == other.pool_id_ && sub_id_ == other.sub_id_;
  }

  // Inequality operator
  bool operator!=(const DomainId& other) const {
    return !(*this == other);
  }
};

// Task flags using HSHM BIT_OPT macro
#define TASK_PERIODIC BIT_OPT(u32, 0)
#define TASK_FIRE_AND_FORGET BIT_OPT(u32, 1)

// Queue priorities
enum QueuePriority {
  kLowLatency = 0,
  kHighLatency = 1
};

// Thread types for work orchestrator
enum ThreadType {
  kLowLatencyWorker = 0,
  kHighLatencyWorker = 1,
  kReinforcementWorker = 2,
  kProcessReaper = 3
};

// Special pool IDs
constexpr PoolId kAdminPoolId = 1;  // Admin ChiMod pool ID (reserved)

// Allocator type aliases using HSHM conventions
#define CHI_MAIN_ALLOC_T hipc::ThreadLocalAllocator
#define CHI_CDATA_ALLOC_T hipc::ThreadLocalAllocator  
#define CHI_RDATA_ALLOC_T hipc::ThreadLocalAllocator

// Memory segment identifiers
enum MemorySegment {
  kMainSegment = 0,
  kClientDataSegment = 1,
  kRuntimeDataSegment = 2
};

// Input/Output parameter macros
#define IN
#define OUT
#define INOUT
#define TEMP

// HSHM Thread-local storage key for current worker  
extern hshm::ThreadLocalKey chi_cur_worker_key_;

// Template aliases for full pointers using HSHM
template<typename T>
using FullPtr = hipc::FullPtr<T>;

// Create HSHM data structures template for chi namespace
HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);

}  // namespace chi

// Hash function specializations for std::unordered_map
namespace std {
  template <>
  struct hash<chi::UniqueId> {
    size_t operator()(const chi::UniqueId& id) const {
      return hash<chi::u32>()(id.major_) ^ 
             (hash<chi::u32>()(id.minor_) << 1);
    }
  };

  template <>
  struct hash<chi::PoolId> {
    size_t operator()(const chi::PoolId& id) const {
      return hash<chi::UniqueId>()(id);
    }
  };

  template <>
  struct hash<chi::SubDomainId> {
    size_t operator()(const chi::SubDomainId& id) const {
      return hash<chi::SubDomainGroup>()(id.major_) ^ 
             (hash<chi::SubDomainMinor>()(id.minor_) << 1);
    }
  };

  template <>
  struct hash<chi::DomainId> {
    size_t operator()(const chi::DomainId& id) const {
      return hash<chi::PoolId>()(id.pool_id_) ^ 
             (hash<chi::SubDomainId>()(id.sub_id_) << 1);
    }
  };
}

#endif  // CHIMAERA_INCLUDE_CHIMAERA_TYPES_H_