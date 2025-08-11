#ifndef MOD_NAME_TASKS_H_
#define MOD_NAME_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/MOD_NAME_methods.h"

/**
 * Task struct definitions for MOD_NAME
 * 
 * Defines the tasks for Create and Custom methods.
 */

namespace chimaera::MOD_NAME {

/**
 * CreateTask - Initialize the container
 */
struct CreateTask : public chi::Task {
  /** SHM default constructor */
  explicit CreateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc) {}

  /** Emplace constructor */
  explicit CreateTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, 
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::DomainQuery &dom_query)
      : chi::Task(alloc, task_node, pool_id, dom_query, 0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = static_cast<chi::u32>(Method::kCreate);
    task_flags_.Clear();
    dom_query_ = dom_query;
  }
};

/**
 * CustomTask - Example custom operation
 */
struct CustomTask : public chi::Task {
  // Task-specific data
  INOUT hipc::string data_;
  IN chi::u32 operation_id_;
  OUT chi::u32 result_code_;

  /** SHM default constructor */
  explicit CustomTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), 
        data_(alloc), operation_id_(0), result_code_(0) {}

  /** Emplace constructor */
  explicit CustomTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::DomainQuery &dom_query,
      const std::string &data,
      chi::u32 operation_id)
      : chi::Task(alloc, task_node, pool_id, dom_query, 10),
        data_(alloc, data), operation_id_(operation_id), result_code_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = static_cast<chi::u32>(Method::kCustom);
    task_flags_.Clear();
    dom_query_ = dom_query;
  }
};

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_TASKS_H_