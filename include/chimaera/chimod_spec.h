#ifndef CHIMAERA_INCLUDE_CHIMAERA_CHIMOD_SPEC_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CHIMOD_SPEC_H_

#include <string>
#include <vector>
#include <boost/context/detail/fcontext.hpp>
#include "chimaera/types.h"
#include "chimaera/pool_query.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"

/**
 * ChiMod Specification - Client/Server Interface
 * 
 * This header defines the interface between client and server components
 * of ChiMod modules. ChiMods implement distributed task execution with
 * minimal client-side code and full logic on the server/runtime side.
 */

namespace chi {

// Forward declarations
class RunContext;

// ChiContainer forward declaration - available in all contexts
class ChiContainer;

/**
 * Monitor mode identifiers for task scheduling
 */
enum class MonitorModeId : u32 {
  kLocalSchedule = 0,    ///< Route task to local container queue lane
  kGlobalSchedule = 1,   ///< Coordinate global task distribution
  kEstLoad = 2,          ///< Estimate task execution time for waiting
};

/**
 * Queue identifier
 */
using QueueId = u32;


/**
 * Context passed to task execution methods
 */
struct RunContext {
  void* stack_ptr;              // Stack pointer (positioned for boost::context based on stack growth)
  void* stack_base_for_free;    // Original malloc pointer for freeing
  size_t stack_size;
  ThreadType thread_type;
  u32 worker_id;
  FullPtr<Task> task;  // Task being executed by this context
  bool is_blocked;             // Task is waiting for completion
  double estimated_completion_time_us; // Estimated completion time in microseconds
  hshm::Timepoint block_time;          // Time when task was blocked (for timing measurements)
  boost::context::detail::transfer_t fiber_transfer; // boost::context transfer data for fiber execution
  boost::context::detail::fcontext_t fiber_context;  // boost::context fiber context for task execution
  void* container;             // Current container being executed (ChiContainer* in runtime)
  void* lane;                  // Current lane being processed (TaskQueue::TaskLane* in runtime)
  std::vector<FullPtr<Task>> waiting_for_tasks; // Tasks this task is waiting for completion
  std::vector<ResolvedPoolQuery> resolved_queries; // Resolved pool queries for task distribution
  
  RunContext() : stack_ptr(nullptr), stack_base_for_free(nullptr), stack_size(0), 
                 thread_type(kLowLatencyWorker), worker_id(0),
                 is_blocked(false), estimated_completion_time_us(0.0),
                 fiber_transfer{}, fiber_context{}, container(nullptr), lane(nullptr) {}

  /**
   * Check if all subtasks this task is waiting for are completed
   * @return true if all subtasks are completed, false otherwise
   */
  bool AreSubtasksCompleted() const {
    // Check each task in the waiting_for_tasks vector
    for (const auto& waiting_task : waiting_for_tasks) {
      if (!waiting_task.IsNull()) {
        // Check if the waiting task is completed using atomic flag
        if (waiting_task->is_complete.load() == 0) {
          return false; // Found a subtask that's not completed yet
        }
      }
    }
    return true; // All subtasks are completed (or no subtasks)
  }
};

#ifdef CHIMAERA_RUNTIME

/**
 * Container Runtime Interface (Server-Side)
 * 
 * Executes only in the Chimaera runtime process.
 * Contains the main logic for task processing, monitoring, and scheduling.
 */
class ChiContainer {
 public:
  PoolId pool_id_;           ///< The unique ID of this pool
  std::string pool_name_;    ///< The semantic name of this pool  
  u32 container_id_;         ///< The logical ID of this container instance

  /**
   * Create a local queue with specified lanes
   * @param queue_id Unique queue identifier
   * @param num_lanes Number of processing lanes
   * @param flags Queue configuration flags
   */
  virtual void CreateLocalQueue(QueueId queue_id, u32 num_lanes, u32 flags) = 0;

  /**
   * Get specific lane by ID
   * @param queue_id Queue identifier
   * @param lane_id Lane identifier within queue
   * @return Pointer to lane or nullptr if not found
   */
  virtual TaskQueue::TaskLane* GetLane(QueueId queue_id, LaneId lane_id) = 0;

  /**
   * Get lane by hash for load balancing
   * @param queue_id Queue identifier  
   * @param hash Hash value for lane selection
   * @return Pointer to selected lane
   */
  virtual TaskQueue::TaskLane* GetLaneByHash(QueueId queue_id, u32 hash) = 0;

  /**
   * Initialize container with pool information
   * @param pool_id Pool identifier
   * @param pool_name Pool name
   */
  virtual void Init(const PoolId& pool_id, const std::string& pool_name) {
    pool_id_ = pool_id;
    pool_name_ = pool_name;
    container_id_ = 0; // Set by container manager
  }

  /**
   * Virtual destructor
   */
  virtual ~ChiContainer() = default;

  /**
   * Execute a method on a task
   * @param method Method identifier
   * @param task_ptr Full pointer to task to execute
   * @param rctx Runtime execution context
   */
  virtual void Run(u32 method, hipc::FullPtr<Task> task_ptr, RunContext& rctx) = 0;

  /**
   * Monitor a method execution for scheduling/coordination
   * @param mode Monitoring mode (local/global scheduling, cleanup)
   * @param method Method identifier
   * @param task_ptr Full pointer to task for shared memory access
   * @param rctx Runtime execution context
   */
  virtual void Monitor(MonitorModeId mode, u32 method, hipc::FullPtr<Task> task_ptr,
                      RunContext& rctx) = 0;

  /**
   * Delete/cleanup a task
   * @param method Method identifier that created the task
   * @param task_ptr Full pointer to task to delete
   */
  virtual void Del(u32 method, hipc::FullPtr<Task> task_ptr) = 0;
};

#endif // CHIMAERA_RUNTIME

/**
 * Container Client Interface (Client-Side)
 * 
 * Minimal client interface for task submission.
 * Executes in user processes, performs only task allocation and queueing.
 */
class ChiContainerClient {
 public:
  PoolId pool_id_; ///< The unique ID of the pool this client connects to

  /**
   * Default constructor
   */
  ChiContainerClient() : pool_id_(0) {}

  /**
   * Initialize client with pool ID
   * @param pool_id Pool identifier to connect to
   */
  virtual void Init(const PoolId& pool_id) {
    pool_id_ = pool_id;
  }

  /**
   * Virtual destructor
   */
  virtual ~ChiContainerClient() = default;

  /**
   * Serialization support
   */
  template <typename Ar> 
  void serialize(Ar& ar) { 
    ar(pool_id_); 
  }

 protected:
  /**
   * Helper method to allocate and enqueue a task
   * @param task_ptr Allocated task to enqueue
   * @param priority Queue priority for the task
   */
  void EnqueueTask(hipc::FullPtr<Task>& task_ptr, QueuePriority priority = kLowLatency);

  /**
   * Helper method to allocate a new task
   * @param args Arguments for task construction
   * @return Full pointer to allocated task
   */
  template<typename TaskT, typename... Args>
  hipc::FullPtr<TaskT> AllocateTask(MemorySegment segment, Args&&... args);
};

} // namespace chi

/**
 * ChiMod Entry Point Macros
 * 
 * These macros must be used in the runtime implementation file to
 * export the required C symbols for dynamic loading.
 */

extern "C" {
  // Required ChiMod entry points
  typedef chi::ChiContainer* (*alloc_chimod_t)();
  typedef chi::ChiContainer* (*new_chimod_t)(const chi::PoolId* pool_id, 
                                             const char* pool_name);
  typedef const char* (*get_chimod_name_t)(void);
  typedef void (*destroy_chimod_t)(chi::ChiContainer* container);
}

/**
 * Macro to define ChiMod entry points in runtime source file (deprecated)
 * 
 * Usage: CHI_CHIMOD_CC(MyContainerClass, "my_chimod_name")
 * Note: Use CHI_TASK_CC instead for new modules
 */
#define CHI_CHIMOD_CC(CONTAINER_CLASS, MOD_NAME)                              \
  extern "C" {                                                                \
    chi::ChiContainer* alloc_chimod() {                                       \
      return reinterpret_cast<chi::ChiContainer*>(new CONTAINER_CLASS());     \
    }                                                                         \
                                                                              \
    chi::ChiContainer* new_chimod(const chi::PoolId* pool_id,                 \
                                 const char* pool_name) {                     \
      chi::ChiContainer* container =                                          \
        reinterpret_cast<chi::ChiContainer*>(new CONTAINER_CLASS());          \
      container->Init(*pool_id, std::string(pool_name));                     \
      return container;                                                       \
    }                                                                         \
                                                                              \
    const char* get_chimod_name() {                                           \
      return MOD_NAME;                                                        \
    }                                                                         \
                                                                              \
    void destroy_chimod(chi::ChiContainer* container) {                       \
      delete reinterpret_cast<CONTAINER_CLASS*>(container);                   \
    }                                                                         \
                                                                              \
    bool is_chimaera_chimod_ = true;                                          \
  }

/**
 * Macro to define ChiMod entry points for task-based modules
 * 
 * Usage: CHI_TASK_CC(MyContainerClass)
 * This macro provides a cleaner interface for modules that use the Container base class.
 * The ChiMod name is automatically retrieved from CONTAINER_CLASS::CreateParams::chimod_lib_name.
 */
#define CHI_TASK_CC(CONTAINER_CLASS)                                         \
  extern "C" {                                                               \
    chi::ChiContainer* alloc_chimod() {                                      \
      return reinterpret_cast<chi::ChiContainer*>(new CONTAINER_CLASS());    \
    }                                                                        \
                                                                             \
    chi::ChiContainer* new_chimod(const chi::PoolId* pool_id,                \
                                 const char* pool_name) {                    \
      auto* container = new CONTAINER_CLASS();                              \
      /* Use base Container Init for compatibility */                       \
      container->chi::Container::Init(*pool_id, std::string(pool_name));    \
      return reinterpret_cast<chi::ChiContainer*>(container);                \
    }                                                                        \
                                                                             \
    const char* get_chimod_name() {                                          \
      return CONTAINER_CLASS::CreateParams::chimod_lib_name;                 \
    }                                                                        \
                                                                             \
    void destroy_chimod(chi::ChiContainer* container) {                      \
      delete reinterpret_cast<CONTAINER_CLASS*>(container);                  \
    }                                                                        \
                                                                             \
    bool is_chimaera_chimod_ = true;                                         \
  }

#endif // CHIMAERA_INCLUDE_CHIMAERA_CHIMOD_SPEC_H_