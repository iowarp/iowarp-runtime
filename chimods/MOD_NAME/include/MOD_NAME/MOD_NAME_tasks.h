#ifndef MOD_NAME_TASKS_H_
#define MOD_NAME_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/MOD_NAME_methods.h"
// Include admin tasks for BaseCreateTask
#include <admin/admin_tasks.h>

/**
 * Task struct definitions for MOD_NAME
 * 
 * Defines the tasks for Create and Custom methods.
 */

namespace chimaera::MOD_NAME {

/**
 * CreateParams for MOD_NAME chimod
 * Contains configuration parameters for MOD_NAME container creation
 */
struct CreateParams {
  // MOD_NAME-specific parameters
  std::string config_data_;
  chi::u32 worker_count_;
  
  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME_runtime";
  
  // Default constructor
  CreateParams() : worker_count_(1) {}
  
  // Constructor with allocator and parameters
  CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, 
               const std::string& config_data = "", 
               chi::u32 worker_count = 1)
      : config_data_(config_data), worker_count_(worker_count) {
    // MOD_NAME parameters use standard types, so allocator isn't needed directly
    // but it's available for future use with HSHM containers
  }
  
  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    ar(config_data_, worker_count_);
  }
};

/**
 * CreateTask - Initialize the MOD_NAME container
 * Type alias for BaseCreateTask with CreateParams and Method::kCreate
 */
using CreateTask = chimaera::admin::BaseCreateTask<CreateParams, chimaera::MOD_NAME::Method::kCreate>;

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
    method_ = Method::kCustom;
    task_flags_.Clear();
    dom_query_ = dom_query;
  }
};

/**
 * Standard DestroyTask for MOD_NAME
 * All ChiMods should use the same DestroyTask structure from admin
 */
using DestroyTask = chimaera::admin::DestroyTask;

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_TASKS_H_