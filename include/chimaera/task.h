#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_H_

#include <vector>
#include "chimaera/types.h"
#include "chimaera/domain_query.h"

namespace chi {

/**
 * Base task class for Chimaera distributed execution
 * 
 * Inherits from hipc::ShmContainer to support shared memory operations.
 * All tasks represent C++ functions similar to RPCs that can be executed
 * across the distributed system.
 */
class Task : public hipc::ShmContainer {
 public:
  IN PoolId pool_id_;           /**< Pool identifier for task execution */
  IN TaskNode task_node_;       /**< Node identifier for task routing */
  IN DomainQuery dom_query_;    /**< Domain query for execution location */
  IN MethodId method_;          /**< Method identifier for task type */
  IN ibitfield task_flags_;     /**< Task properties and flags */
  IN double period_ns_;         /**< Period in nanoseconds for periodic tasks */

  /**
   * Constructor with allocator context
   * @param alloc Allocator context for shared memory operations
   */
  template<typename AllocT>
  explicit Task(hipc::CtxAllocator<AllocT>& alloc) : hipc::ShmContainer(), 
        pool_id_(0), task_node_(0), dom_query_(), method_(0), 
        task_flags_(0), period_ns_(0.0) {
    // Store allocator context if needed
  }

  /**
   * Default destructor
   */
  virtual ~Task();

  /**
   * Wait for task completion (blocking)
   */
  void Wait();

  /**
   * Wait for specific subtask completion
   * @param subtask Pointer to subtask to wait for
   */
  void Wait(Task* subtask);

  /**
   * Wait for multiple subtasks completion
   * @param subtasks Vector of subtask pointers to wait for
   */
  template<typename TaskT>
  inline void Wait(std::vector<FullPtr<TaskT>>& subtasks);

  /**
   * Check if task is periodic
   * @return true if task has periodic flag set
   */
  bool IsPeriodic() const;

  /**
   * Check if task is fire-and-forget
   * @return true if task has fire-and-forget flag set
   */
  bool IsFireAndForget() const;

  /**
   * Get task execution period
   * @return Period in nanoseconds, 0 if not periodic
   */
  double GetPeriod() const;

  /**
   * Set task flags
   * @param flags Bitfield of task flags to set
   */
  void SetFlags(u32 flags);

  /**
   * Clear task flags
   * @param flags Bitfield of task flags to clear
   */
  void ClearFlags(u32 flags);

 protected:
  /**
   * Default constructor (protected)
   */
  Task();

 private:
  // Disable copy operations for now
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
};

/**
 * Template implementation for waiting on multiple subtasks
 */
template<typename TaskT>
inline void Task::Wait(std::vector<FullPtr<TaskT>>& subtasks) {
  // Stub implementation - iterate through and wait for each
  for (auto& subtask : subtasks) {
    if (subtask.get() != nullptr) {
      subtask->Wait();
    }
  }
}

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_TASK_H_