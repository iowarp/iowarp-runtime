#ifndef CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "chimaera/pool_query.h"
#include "chimaera/task.h"
#include "chimaera/task_archives.h"
#include "chimaera/task_queue.h"
#include "chimaera/types.h"

// Forward declarations to avoid circular dependencies
namespace chi {
class WorkOrchestrator;
}

/**
 * Container Base Class with Default Implementations
 *
 * Provides default implementations of ChiContainer methods for simpler modules.
 * Modules can inherit from this class instead of ChiContainer to get basic
 * queue and lane management functionality out of the box.
 */

namespace chi {

/**
 * Monitor mode identifiers for task scheduling
 */
enum class MonitorModeId : u32 {
  kLocalSchedule = 0,   ///< Route task to local container queue lane
  kGlobalSchedule = 1,  ///< Coordinate global task distribution
  kEstLoad = 2,         ///< Estimate task execution time for waiting
};

/**
 * Queue identifier
 */
using QueueId = u32;

/**
 * Container - Base class for all containers
 *
 * Unified container class that provides all functionality for task processing,
 * monitoring, and scheduling. Replaces the previous ChiContainer/Container
 * split.
 */
class Container {
 public:
  PoolId pool_id_;         ///< The unique ID of this pool
  std::string pool_name_;  ///< The semantic name of this pool
  u32 container_id_;       ///< The logical ID of this container instance

 protected:
  PoolQuery pool_query_;

 public:
  Container() = default;
  virtual ~Container() {
    // Note: Lane mappings are managed by WorkOrchestrator lifecycle
    // No explicit cleanup needed since lanes are mapped, not registered
  }

  /**
   * Initialize container with pool information
   * @param pool_id The unique ID of this pool
   * @param pool_name The semantic name of this pool (user-provided)
   *
   * ChiMod runtime classes should override this method to initialize their client member.
   */
  virtual void Init(const PoolId& pool_id, const std::string& pool_name) {
    pool_id_ = pool_id;
    pool_name_ = pool_name;
    container_id_ = 0;
    pool_query_ = PoolQuery();  // Default pool query
  }

  /**
   * Execute a method on a task - must be implemented by derived classes
   */
  virtual void Run(u32 method, hipc::FullPtr<Task> task_ptr,
                   RunContext& rctx) = 0;

  /**
   * Monitor a method execution for scheduling/coordination - must be
   * implemented by derived classes
   */
  virtual void Monitor(MonitorModeId mode, u32 method,
                       hipc::FullPtr<Task> task_ptr, RunContext& rctx) = 0;

  /**
   * Delete/cleanup a task - must be implemented by derived classes
   */
  virtual void Del(u32 method, hipc::FullPtr<Task> task_ptr) = 0;

  /**
   * Get remaining work count for this container - PURE VIRTUAL
   * Must be implemented by all derived container classes
   * @return Number of work units remaining in this container
   */
  virtual u64 GetWorkRemaining() const = 0;

  /**
   * Update work count for a task - should be overridden by derived classes
   * @param task_ptr Task being executed
   * @param rctx Current run context
   * @param increment Work count change (positive or negative)
   */
  virtual void UpdateWork(hipc::FullPtr<Task> task_ptr, RunContext& rctx,
                          i64 increment) {
    // Default: no work tracking
    (void)task_ptr;
    (void)rctx;
    (void)increment;  // Suppress unused warnings
  }

  /**
   * Serialize task IN parameters for network transfer - must be implemented by
   * derived classes Uses switch-case structure based on method ID to dispatch
   * to appropriate serialization
   */
  virtual void SaveIn(u32 method, TaskSaveInArchive& archive,
                      hipc::FullPtr<Task> task_ptr) = 0;

  /**
   * Deserialize task IN parameters from network transfer - must be implemented
   * by derived classes Uses switch-case structure based on method ID to
   * dispatch to appropriate deserialization
   */
  virtual void LoadIn(u32 method, TaskLoadInArchive& archive,
                      hipc::FullPtr<Task> task_ptr) = 0;

  /**
   * Serialize task OUT parameters for network transfer - must be implemented by
   * derived classes Uses switch-case structure based on method ID to dispatch
   * to appropriate serialization
   */
  virtual void SaveOut(u32 method, TaskSaveOutArchive& archive,
                       hipc::FullPtr<Task> task_ptr) = 0;

  /**
   * Deserialize task OUT parameters from network transfer - must be implemented
   * by derived classes Uses switch-case structure based on method ID to
   * dispatch to appropriate deserialization
   */
  virtual void LoadOut(u32 method, TaskLoadOutArchive& archive,
                       hipc::FullPtr<Task> task_ptr) = 0;

  /**
   * Create a new copy of a task (deep copy for distributed execution) - must be
   * implemented by derived classes Uses switch-case structure based on method
   * ID to dispatch to appropriate task type copying
   */
  HSHM_DLL virtual void NewCopy(u32 method, 
                               const hipc::FullPtr<Task> &orig_task,
                               hipc::FullPtr<Task> &dup_task, bool deep) = 0;

 protected:
  /**
   * Get the allocator for this container
   */
  hipc::CtxAllocator<CHI_MAIN_ALLOC_T> GetAllocator() const {
    return HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
  }

  /**
   * Check if the container's pool ID is null/invalid
   * @return true if pool_id_ is null, false otherwise
   */
  bool IsNull() const {
    return pool_id_.IsNull();
  }
};

/**
 * Container Client Interface (Client-Side)
 *
 * Minimal client interface for task submission.
 * Executes in user processes, performs only task allocation and queueing.
 */
class ContainerClient {
 public:
  PoolId pool_id_;  ///< The unique ID of the pool this client connects to
  u32 return_code_; ///< Return code from the last Create operation (0=success, non-zero=error)

  /**
   * Default constructor
   */
  ContainerClient() : pool_id_(), return_code_(0) {}

  /**
   * Initialize client with pool ID
   * @param pool_id Pool identifier to connect to
   */
  virtual void Init(const PoolId& pool_id) { 
    pool_id_ = pool_id; 
    return_code_ = 0;
  }

  /**
   * Virtual destructor
   */
  virtual ~ContainerClient() = default;

  /**
   * Serialization support
   */
  template <typename Ar>
  void serialize(Ar& ar) {
    ar(pool_id_, return_code_);
  }

  /**
   * Check if the client's pool ID is null/invalid
   * @return true if pool_id_ is null, false otherwise
   */
  bool IsNull() const {
    return pool_id_.IsNull();
  }

  /**
   * Get the return code from the last Create operation
   * @return Return code (0=success, non-zero=error)
   */
  u32 GetReturnCode() const {
    return return_code_;
  }

  /**
   * Set the return code for the client
   * @param return_code Return code to set (0=success, non-zero=error)
   */
  void SetReturnCode(u32 return_code) {
    return_code_ = return_code;
  }

 protected:
  /**
   * Helper method to allocate and enqueue a task
   * @param task_ptr Allocated task to enqueue
   * @param priority Queue priority for the task
   */
  void EnqueueTask(hipc::FullPtr<Task>& task_ptr,
                   QueuePriority priority = kLowLatency);

  /**
   * Helper method to allocate a new task
   * @param args Arguments for task construction
   * @return Full pointer to allocated task
   */
  template <typename TaskT, typename... Args>
  hipc::FullPtr<TaskT> AllocateTask(MemorySegment segment, Args&&... args);
};

}  // namespace chi

/**
 * ChiMod Entry Point Macros
 *
 * These macros must be used in the runtime implementation file to
 * export the required C symbols for dynamic loading.
 */

extern "C" {
// Required ChiMod entry points
typedef chi::Container* (*alloc_chimod_t)();
typedef chi::Container* (*new_chimod_t)(const chi::PoolId* pool_id,
                                        const char* pool_name);
typedef const char* (*get_chimod_name_t)(void);
typedef void (*destroy_chimod_t)(chi::Container* container);
}

/**
 * Macro to define ChiMod entry points in runtime source file (deprecated)
 *
 * Usage: CHI_CHIMOD_CC(MyContainerClass, "my_chimod_name")
 * Note: Use CHI_TASK_CC instead for new modules
 */
#define CHI_CHIMOD_CC(CONTAINER_CLASS, MOD_NAME)                     \
  extern "C" {                                                       \
  chi::Container* alloc_chimod() {                                   \
    return reinterpret_cast<chi::Container*>(new CONTAINER_CLASS()); \
  }                                                                  \
                                                                     \
  chi::Container* new_chimod(const chi::PoolId* pool_id,             \
                             const char* pool_name) {                \
    chi::Container* container =                                      \
        reinterpret_cast<chi::Container*>(new CONTAINER_CLASS());    \
    /* Initialization is handled by the container's Create method */ \
    return container;                                                \
  }                                                                  \
                                                                     \
  const char* get_chimod_name() { return MOD_NAME; }                 \
                                                                     \
  void destroy_chimod(chi::Container* container) {                   \
    delete reinterpret_cast<CONTAINER_CLASS*>(container);            \
  }                                                                  \
                                                                     \
  bool is_chimaera_chimod_ = true;                                   \
  }

/**
 * Macro to define ChiMod entry points for task-based modules
 *
 * Usage: CHI_TASK_CC(MyContainerClass)
 * This macro provides a cleaner interface for modules that use the Container
 * base class. The ChiMod name is automatically retrieved from
 * CONTAINER_CLASS::CreateParams::chimod_lib_name.
 */
#define CHI_TASK_CC(CONTAINER_CLASS)                                 \
  extern "C" {                                                       \
  chi::Container* alloc_chimod() {                                   \
    return reinterpret_cast<chi::Container*>(new CONTAINER_CLASS()); \
  }                                                                  \
                                                                     \
  chi::Container* new_chimod(const chi::PoolId* pool_id,             \
                             const char* pool_name) {                \
    auto* container = new CONTAINER_CLASS();                         \
    /* Initialization is handled by the container's Create method */ \
    return reinterpret_cast<chi::Container*>(container);             \
  }                                                                  \
                                                                     \
  const char* get_chimod_name() {                                    \
    return CONTAINER_CLASS::CreateParams::chimod_lib_name;           \
  }                                                                  \
                                                                     \
  void destroy_chimod(chi::Container* container) {                   \
    delete reinterpret_cast<CONTAINER_CLASS*>(container);            \
  }                                                                  \
                                                                     \
  bool is_chimaera_chimod_ = true;                                   \
  }

#endif  // CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_