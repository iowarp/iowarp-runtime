#ifndef MOD_NAME_TASKS_H_
#define MOD_NAME_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/MOD_NAME_methods.h"
// Include admin tasks for BaseCreateTask
#include <chimaera/admin/admin_tasks.h>

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
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME";
  
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
 * Type alias for GetOrCreatePoolTask with CreateParams (uses kGetOrCreatePool method)
 * Non-admin modules should use GetOrCreatePoolTask instead of BaseCreateTask
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;


/**
 * CustomTask - Example custom operation
 */
struct CustomTask : public chi::Task {
  // Task-specific data
  INOUT chi::ipc::string data_;
  IN chi::u32 operation_id_;

  /** SHM default constructor */
  explicit CustomTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), 
        data_(alloc), operation_id_(0) {}

  /** Emplace constructor */
  explicit CustomTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      const std::string &data,
      chi::u32 operation_id)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        data_(alloc, data), operation_id_(operation_id) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCustom;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: data_, operation_id_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(data_, operation_id_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: data_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(data_);
  }
};

/**
 * CoMutexTestTask - Test CoMutex functionality
 */
struct CoMutexTestTask : public chi::Task {
  IN chi::u32 test_id_;         // Test identifier
  IN chi::u32 hold_duration_ms_; // How long to hold the mutex

  /** SHM default constructor */
  explicit CoMutexTestTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), test_id_(0), hold_duration_ms_(0) {}

  /** Emplace constructor */
  explicit CoMutexTestTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      chi::u32 test_id,
      chi::u32 hold_duration_ms)
      : chi::Task(alloc, task_node, pool_id, pool_query, 20),
        test_id_(test_id), hold_duration_ms_(hold_duration_ms) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCoMutexTest;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(test_id_, hold_duration_ms_);
  }
  
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    // No output parameters for this task
  }
};

/**
 * CoRwLockTestTask - Test CoRwLock functionality
 */
struct CoRwLockTestTask : public chi::Task {
  IN chi::u32 test_id_;         // Test identifier
  IN bool is_writer_;           // True for write lock, false for read lock
  IN chi::u32 hold_duration_ms_; // How long to hold the lock

  /** SHM default constructor */
  explicit CoRwLockTestTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), test_id_(0), is_writer_(false), hold_duration_ms_(0) {}

  /** Emplace constructor */
  explicit CoRwLockTestTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      chi::u32 test_id,
      bool is_writer,
      chi::u32 hold_duration_ms)
      : chi::Task(alloc, task_node, pool_id, pool_query, 21),
        test_id_(test_id), is_writer_(is_writer), hold_duration_ms_(hold_duration_ms) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCoRwLockTest;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(test_id_, is_writer_, hold_duration_ms_);
  }
  
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    // No output parameters for this task
  }
};

/**
 * FireAndForgetTestTask - Test fire-and-forget task functionality
 * This task will be automatically deleted after completion
 */
struct FireAndForgetTestTask : public chi::Task {
  IN chi::u32 test_id_;         // Test identifier
  IN chi::u32 processing_time_ms_; // How long to process
  IN chi::ipc::string log_message_; // Message to log

  /** SHM default constructor */
  explicit FireAndForgetTestTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), test_id_(0), processing_time_ms_(0), log_message_(alloc) {}

  /** Emplace constructor */
  explicit FireAndForgetTestTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      chi::u32 test_id,
      chi::u32 processing_time_ms,
      const std::string &log_message)
      : chi::Task(alloc, task_node, pool_id, pool_query, 22),
        test_id_(test_id), processing_time_ms_(processing_time_ms), log_message_(alloc, log_message) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kFireAndForgetTest;
    task_flags_.Clear();
    task_flags_.SetBits(TASK_FIRE_AND_FORGET);
    pool_query_ = pool_query;
  }

  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(test_id_, processing_time_ms_, log_message_);
  }
  
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    // Fire-and-forget tasks typically don't have output parameters
  }
};

/**
 * WaitTestTask - Test recursive task->Wait() functionality
 * This task calls itself recursively "depth" times to test nested Wait() calls
 */
struct WaitTestTask : public chi::Task {
  IN chi::u32 depth_;              // Number of recursive calls to make
  IN chi::u32 test_id_;            // Test identifier for tracking
  INOUT chi::u32 current_depth_;   // Current recursion level (starts at 0)

  /** SHM default constructor */
  explicit WaitTestTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), depth_(0), test_id_(0), current_depth_(0) {}

  /** Emplace constructor */
  explicit WaitTestTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      chi::u32 depth,
      chi::u32 test_id)
      : chi::Task(alloc, task_node, pool_id, pool_query, 23),
        depth_(depth), test_id_(test_id), current_depth_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kWaitTest;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(depth_, test_id_, current_depth_);
  }
  
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(current_depth_);  // Return the final depth reached
  }
};

/**
 * Standard DestroyTask for MOD_NAME
 * All ChiMods should use the same DestroyTask structure from admin
 */
using DestroyTask = chimaera::admin::DestroyTask;

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_TASKS_H_