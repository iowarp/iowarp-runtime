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

// Pool and task identifiers
using PoolId = u32;
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

// HSHM Thread-local storage keys for current run context and worker  
extern hshm::ThreadLocalKey chi_cur_rctx_key_;
extern hshm::ThreadLocalKey chi_cur_worker_key_;

// Template aliases for full pointers using HSHM
template<typename T>
using FullPtr = hipc::FullPtr<T>;

// Create HSHM data structures template for chi namespace
HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_TYPES_H_