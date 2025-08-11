#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_H_

#include <vector>

#include "chimaera/domain_query.h"
#include "chimaera/types.h"

namespace chi {

// Forward declaration
class Task;

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
  IN PoolId pool_id_;        /**< Pool identifier for task execution */
  IN TaskNode task_node_;    /**< Node identifier for task routing */
  IN DomainQuery dom_query_; /**< Domain query for execution location */
  IN MethodId method_;       /**< Method identifier for task type */
  IN ibitfield task_flags_;  /**< Task properties and flags */
  IN double period_ns_;      /**< Period in nanoseconds for periodic tasks */

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
                const DomainQuery &dom_query, const MethodId &method)
      : hipc::ShmContainer() {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = method;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
    period_ns_ = 0.0;
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
    dom_query_ = other.dom_query_;
    method_ = other.method_;
    task_flags_ = other.task_flags_;
    period_ns_ = other.period_ns_;
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
  HSHM_CROSS_FUN void shm_move_op(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      Task &&other) noexcept {
    // For simplified Task class, just copy the data
    shm_strong_copy_main(other);
    other.SetNull();
  }

  /**
   * IsNull check
   */
  HSHM_INLINE_CROSS_FUN bool IsNull() const {
    return false;  // Base task is never null
  }

  /**
   * SetNull implementation
   */
  HSHM_INLINE_CROSS_FUN void SetNull() {
    pool_id_ = 0;
    task_node_ = 0;
    dom_query_ = DomainQuery();
    method_ = 0;
    task_flags_.Clear();
    period_ns_ = 0.0;
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
   */
  HSHM_CROSS_FUN void Wait() {
    // Stub implementation
  }

  /**
   * Wait for specific subtask completion
   * @param subtask Pointer to subtask to wait for
   */
  template <typename TaskT>
  HSHM_CROSS_FUN void Wait(TaskT *subtask) {
    if (subtask) {
      subtask->Wait();
    }
  }

  /**
   * Wait for multiple subtasks completion
   * @param subtasks Vector of subtask pointers to wait for
   */
  template <typename TaskT>
  HSHM_CROSS_FUN void Wait(std::vector<FullPtr<TaskT>> &subtasks) {
    // Iterate through and wait for each
    for (auto &subtask : subtasks) {
      if (!subtask.IsNull()) {
        subtask->Wait();
      }
    }
  }

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
   * Get task execution period
   * @return Period in nanoseconds, 0 if not periodic
   */
  HSHM_CROSS_FUN double GetPeriod() const { return period_ns_; }

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
};

// Cleanup macros
#undef CLASS_NAME
#undef CLASS_NEW_ARGS

}  // namespace chi

// Namespace alias for convenience - removed to avoid circular reference

#endif  // CHIMAERA_INCLUDE_CHIMAERA_TASK_H_