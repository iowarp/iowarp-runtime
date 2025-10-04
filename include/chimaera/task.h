#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_H_

#include <atomic>
#include <boost/context/detail/fcontext.hpp>
#include <sstream>
#include <vector>

#include "chimaera/pool_query.h"
#include "chimaera/task_queue.h"
#include "chimaera/types.h"

// Include cereal for serialization
#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

// TaskQueue types are now available via include

namespace chi {

// Forward declarations
class Task;
class Container;
struct RunContext;

// Define macros for container template
#define CLASS_NAME Task
#define CLASS_NEW_ARGS

/**
 * Base task class for Chimaera distributed execution
 *
 * Inherits from hipc::ShmContainer to support shared memory operations.
 * All tasks represent C++ functions similar to RPCs that can be executed
 * across the distributed system.
 */
class Task : public hipc::ShmContainer {
public:
  IN PoolId pool_id_;       /**< Pool identifier for task execution */
  IN TaskNode task_node_;   /**< Node identifier for task routing */
  IN PoolQuery pool_query_; /**< Pool query for execution location */
  IN MethodId method_;      /**< Method identifier for task type */
  IN ibitfield task_flags_; /**< Task properties and flags */
  IN double period_ns_;     /**< Period in nanoseconds for periodic tasks */
  IN RunContext *run_ctx_; /**< Pointer to runtime context for task execution */
  IN u32 net_key_; /**< Network identification key for distributed scheduling */
  std::atomic<u32> is_complete; /**< Atomic flag indicating task completion
                                   (0=not complete, 1=complete) */
  std::atomic<u32>
      return_code_; /**< Task return code (0=success, non-zero=error) */

  /**
   * SHM default constructor
   */
  explicit Task(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : hipc::ShmContainer() {
    SetNull();
  }

  /**
   * Emplace constructor with task initialization
   */
  explicit Task(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                const TaskNode &task_node, const PoolId &pool_id,
                const PoolQuery &pool_query, const MethodId &method)
      : hipc::ShmContainer() {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = method;
    task_flags_.SetBits(0);
    pool_query_ = pool_query;
    period_ns_ = 0.0;
    run_ctx_ = nullptr;
    net_key_ = 0;
    is_complete.store(0);  // Initialize as not complete
    return_code_.store(0); // Initialize as success
  }

  /**
   * Copy constructor
   */
  HSHM_CROSS_FUN explicit Task(const Task &other) {
    SetNull();
    shm_strong_copy_main(other);
  }

  /**
   * Strong copy implementation
   */
  template <typename ContainerT>
  HSHM_CROSS_FUN void shm_strong_copy_main(const ContainerT &other) {
    pool_id_ = other.pool_id_;
    task_node_ = other.task_node_;
    pool_query_ = other.pool_query_;
    method_ = other.method_;
    task_flags_ = other.task_flags_;
    period_ns_ = other.period_ns_;
    run_ctx_ = other.run_ctx_;
    net_key_ = other.net_key_;
    return_code_.store(other.return_code_.load());
    // Explicitly initialize as not complete for copied tasks
    is_complete.store(0);
  }

  /**
   * Move constructor
   */
  HSHM_CROSS_FUN Task(Task &&other) {
    shm_move_op<false>(
        HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>(),
        std::move(other));
  }

  template <bool IS_ASSIGN>
  HSHM_CROSS_FUN void
  shm_move_op(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
              Task &&other) noexcept {
    // For simplified Task class, just copy the data
    shm_strong_copy_main(other);
    other.SetNull();
  }

  /**
   * IsNull check
   */
  HSHM_INLINE_CROSS_FUN bool IsNull() const {
    return false; // Base task is never null
  }

  /**
   * SetNull implementation
   */
  HSHM_INLINE_CROSS_FUN void SetNull() {
    pool_id_ = PoolId::GetNull();
    task_node_ = 0;
    pool_query_ = PoolQuery();
    method_ = 0;
    task_flags_.Clear();
    period_ns_ = 0.0;
    run_ctx_ = nullptr;
    net_key_ = 0;
    is_complete.store(0);  // Initialize as not complete
    return_code_.store(0); // Initialize as success
  }

  /**
   * Destructor implementation
   */
  HSHM_INLINE_CROSS_FUN void shm_destroy_main() {
    // Base task has no dynamic resources to clean up
  }

  /**
   * Virtual destructor
   */
  HSHM_CROSS_FUN virtual ~Task() = default;

  /**
   * Wait for task completion (blocking)
   * @param from_yield If true, do not add subtasks to RunContext (default:
   * false)
   */
  HSHM_CROSS_FUN void Wait(bool from_yield = false);

  /**
   * Check if task is complete
   * @return true if task is complete, false otherwise
   */
  HSHM_CROSS_FUN bool IsComplete() const;

  /**
   * Yield execution back to worker by waiting for task completion
   */
  HSHM_CROSS_FUN void Yield();

  /**
   * Check if task is periodic
   * @return true if task has periodic flag set
   */
  HSHM_CROSS_FUN bool IsPeriodic() const {
    return task_flags_.Any(TASK_PERIODIC);
  }

  /**
   * Check if task is fire-and-forget
   * @return true if task has fire-and-forget flag set
   */
  HSHM_CROSS_FUN bool IsFireAndForget() const {
    return task_flags_.Any(TASK_FIRE_AND_FORGET);
  }

  /**
   * Check if task has been routed
   * @return true if task has routed flag set
   */
  HSHM_CROSS_FUN bool IsRouted() const { return task_flags_.Any(TASK_ROUTED); }

  /**
   * Check if task is the data owner
   * @return true if task has data owner flag set
   */
  HSHM_CROSS_FUN bool IsDataOwner() const {
    return task_flags_.Any(TASK_DATA_OWNER);
  }

  /**
   * Get task execution period in specified time unit
   * @param unit Time unit constant (kNano, kMicro, kMilli, kSec, kMin, kHour)
   * @return Period in specified unit, 0 if not periodic
   */
  HSHM_CROSS_FUN double GetPeriod(double unit) const {
    return period_ns_ / unit;
  }

  /**
   * Set task execution period in specified time unit
   * @param period Period value in the specified unit
   * @param unit Time unit constant (kNano, kMicro, kMilli, kSec, kMin, kHour)
   */
  HSHM_CROSS_FUN void SetPeriod(double period, double unit) {
    period_ns_ = period * unit;
  }

  /**
   * Set task flags
   * @param flags Bitfield of task flags to set
   */
  HSHM_CROSS_FUN void SetFlags(u32 flags) { task_flags_.SetBits(flags); }

  /**
   * Clear task flags
   * @param flags Bitfield of task flags to clear
   */
  HSHM_CROSS_FUN void ClearFlags(u32 flags) { task_flags_.UnsetBits(flags); }

  /**
   * Get shared memory pointer representation
   */
  HSHM_CROSS_FUN hipc::Pointer GetShmPointer() const {
    return hipc::Pointer::GetNull();
  }

  /**
   * Get the allocator (stub implementation for compatibility)
   */
  HSHM_CROSS_FUN hipc::CtxAllocator<CHI_MAIN_ALLOC_T> GetAllocator() const {
    return HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
  }

  /**
   * Get context allocator (stub implementation for compatibility)
   */
  HSHM_CROSS_FUN hipc::CtxAllocator<CHI_MAIN_ALLOC_T> GetCtxAllocator() const {
    return HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
  }

  /**
   * Serialize data structures to chi::ipc::string using cereal
   * @param alloc Context allocator for memory management
   * @param output_str The string to store serialized data
   * @param args The arguments to serialize
   */
  template <typename... Args>
  static void Serialize(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                        hipc::string &output_str, const Args &...args) {
    std::ostringstream os;
    cereal::BinaryOutputArchive archive(os);
    archive(args...);

    std::string serialized = os.str();
    output_str = hipc::string(alloc, serialized);
  }

  /**
   * Deserialize data structure from chi::ipc::string using cereal
   * @param input_str The string containing serialized data
   * @return The deserialized object
   */
  template <typename OutT>
  static OutT Deserialize(const hipc::string &input_str) {
    std::string data = input_str.str();
    std::istringstream is(data);
    cereal::BinaryInputArchive archive(is);

    OutT result;
    archive(result);
    return result;
  }

  /**
   * Serialize base task fields for incoming network transfer (IN and INOUT
   * parameters) This method serializes the common task fields that are shared
   * across all task types. Called automatically by archives when they detect
   * Task inheritance.
   * @param ar Archive to serialize to
   */
  template <typename Archive> void BaseSerializeIn(Archive &ar) {
    // Handle atomic return_code_ by loading/storing its value
    u32 return_code_value = return_code_.load();
    ar(pool_id_, task_node_, pool_query_, method_, task_flags_, period_ns_,
       net_key_, return_code_value);
    return_code_.store(return_code_value);
  }

  /**
   * Serialize base task fields for outgoing network transfer (OUT and INOUT
   * parameters) This method serializes the common task fields that are shared
   * across all task types. Called automatically by archives when they detect
   * Task inheritance.
   * @param ar Archive to serialize to
   */
  template <typename Archive> void BaseSerializeOut(Archive &ar) {
    // Handle atomic return_code_ by loading/storing its value
    u32 return_code_value = return_code_.load();
    ar(pool_id_, task_node_, pool_query_, method_, task_flags_, period_ns_,
       net_key_, return_code_value);
    return_code_.store(return_code_value);
  }

  /**
   * Serialize task for incoming network transfer (IN and INOUT parameters)
   * This method should be implemented by each specific task type.
   * Archives automatically call BaseSerializeIn first, then this method.
   * @param ar Archive to serialize to
   */
  template <typename Archive> void SerializeIn(Archive &ar) {
    // Base implementation does nothing - derived classes override to serialize
    // their IN/INOUT fields
  }

  /**
   * Serialize task for outgoing network transfer (OUT and INOUT parameters)
   * This method should be implemented by each specific task type.
   * Archives automatically call BaseSerializeOut first, then this method.
   * @param ar Archive to serialize to
   */
  template <typename Archive> void SerializeOut(Archive &ar) {
    // Base implementation does nothing - derived classes override to serialize
    // their OUT/INOUT fields
  }

  /**
   * Yield execution back to worker (runtime) or sleep briefly (non-runtime)
   * In runtime: Jumps back to worker fiber context with estimated completion
   * time Outside runtime: Uses SleepForUs when worker is null
   */
  HSHM_CROSS_FUN void YieldBase();

  /**
   * Get the task return code
   * @return Return code (0=success, non-zero=error)
   */
  HSHM_CROSS_FUN u32 GetReturnCode() const { return return_code_.load(); }

  /**
   * Set the task return code
   * @param return_code Return code to set (0=success, non-zero=error)
   */
  HSHM_CROSS_FUN void SetReturnCode(u32 return_code) {
    return_code_.store(return_code);
  }
};

/**
 * Context passed to task execution methods
 */
struct RunContext {
  void *stack_ptr; // Stack pointer (positioned for boost::context based on
                   // stack growth)
  void *stack_base_for_free; // Original malloc pointer for freeing
  size_t stack_size;
  ThreadType thread_type;
  u32 worker_id;
  FullPtr<Task> task;                  // Task being executed by this context
  bool is_blocked;                     // Task is waiting for completion
  double estimated_completion_time_us; // Estimated completion time in
                                       // microseconds
  hshm::Timepoint
      block_time; // Time when task was blocked (for timing measurements)
  boost::context::detail::transfer_t
      yield_context; // boost::context transfer from FiberExecutionFunction
                     // parameter - used for yielding back
  boost::context::detail::transfer_t
      resume_context;    // boost::context transfer for resuming into yield
                         // function
  Container *container;  // Current container being executed
  TaskLane *lane;        // Current lane being processed
  TaskLane *route_lane_; // Lane pointer set by kLocalSchedule for task routing
  std::vector<FullPtr<Task>>
      waiting_for_tasks; // Tasks this task is waiting for completion
  std::vector<PoolQuery> pool_queries; // Pool queries for task distribution

  RunContext()
      : stack_ptr(nullptr), stack_base_for_free(nullptr), stack_size(0),
        thread_type(kLowLatencyWorker), worker_id(0), is_blocked(false),
        estimated_completion_time_us(0.0), yield_context{}, resume_context{},
        container(nullptr), lane(nullptr), route_lane_(nullptr) {}

  /**
   * Check if all subtasks this task is waiting for are completed
   * @return true if all subtasks are completed, false otherwise
   */
  bool AreSubtasksCompleted() const {
    // Check each task in the waiting_for_tasks vector
    for (const auto &waiting_task : waiting_for_tasks) {
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

// Cleanup macros
#undef CLASS_NAME
#undef CLASS_NEW_ARGS

} // namespace chi

// Namespace alias for convenience - removed to avoid circular reference

#endif // CHIMAERA_INCLUDE_CHIMAERA_TASK_H_