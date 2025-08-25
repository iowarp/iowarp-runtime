#ifndef ADMIN_TASKS_H_
#define ADMIN_TASKS_H_

#include <chimaera/chimaera.h>

#include "autogen/admin_methods.h"

/**
 * Task struct definitions for Admin ChiMod
 *
 * Critical ChiMod for managing ChiPools and runtime lifecycle.
 * Responsible for pool creation/destruction and runtime shutdown.
 */

namespace chimaera::admin {

/**
 * CreateParams for admin chimod
 * Contains configuration parameters for admin container creation
 */
struct CreateParams {
  // Admin-specific parameters can be added here
  // For now, admin doesn't need special parameters beyond the base ones

  // Required: chimod library name for module manager
  static constexpr const char *chimod_lib_name = "chimaera_admin";

  // Default constructor
  CreateParams() = default;
  
  // Constructor with allocator (even though admin doesn't need it currently)
  explicit CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) {
    // Admin params don't require allocator-based initialization currently
  }

  // Serialization support for cereal
  template <class Archive>
  void serialize(Archive &ar) {
    // No additional fields to serialize for admin
  }
};

/**
 * BaseCreateTask - Templated base class for all ChiMod CreateTasks
 * @tparam CreateParamsT The parameter structure containing chimod-specific
 * configuration
 * @tparam MethodId The method ID for this task type
 * @tparam IS_ADMIN Whether this is an admin operation (sets volatile variable)
 */
template <typename CreateParamsT, chi::u32 MethodId = Method::kCreate, bool IS_ADMIN = false>
struct BaseCreateTask : public chi::Task {
  // Pool operation parameters
  INOUT hipc::string chimod_name_;
  IN hipc::string pool_name_;
  INOUT hipc::string chimod_params_;  // Serialized parameters for the specific ChiMod
  IN chi::u32 domain_flags_;
  INOUT chi::PoolId pool_id_;
  
  // Results for pool operations
  OUT chi::u32 result_code_;
  OUT hipc::string error_message_;
  
  // Volatile admin flag set by template parameter
  volatile bool is_admin_;

  /** SHM default constructor */
  explicit BaseCreateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc), chimod_name_(alloc), pool_name_(alloc), 
        chimod_params_(alloc), domain_flags_(0), pool_id_(chi::PoolId::GetNull()),
        result_code_(0), error_message_(alloc), is_admin_(IS_ADMIN) {}

  /** Emplace constructor */
  explicit BaseCreateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                          const chi::TaskNode &task_node,
                          const chi::PoolId &task_pool_id,
                          const chi::PoolQuery &dom_query,
                          const std::string &chimod_name = "",
                          const std::string &pool_name = "",
                          chi::u32 domain_flags = 0,
                          const chi::PoolId &target_pool_id = chi::PoolId::GetNull())
      : chi::Task(alloc, task_node, task_pool_id, dom_query, 0),
        chimod_name_(alloc, chimod_name), pool_name_(alloc, pool_name), 
        chimod_params_(alloc), domain_flags_(domain_flags),
        pool_id_(target_pool_id), result_code_(0), error_message_(alloc), 
        is_admin_(IS_ADMIN) {
    // Initialize base task
    task_node_ = task_node;
    method_ = MethodId;
    task_flags_.Clear();
    pool_query_ = dom_query;

    // Create and serialize the CreateParams into chimod_params_
    CreateParamsT params(alloc);
    chi::Task::Serialize(alloc, chimod_params_, params);
  }

  /**
   * Get the ChiMod library name for module manager
   */
  static constexpr const char *GetChiModLibName() {
    return CreateParamsT::chimod_lib_name;
  }

  /**
   * Set parameters by serializing them to chimod_params_
   */
  template <typename... Args>
  void SetParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, Args &&...args) {
    CreateParamsT params(alloc, std::forward<Args>(args)...);
    chi::Task::Serialize(alloc, chimod_params_, params);
  }

  /**
   * Get the CreateParams by deserializing from chimod_params_
   */
  CreateParamsT GetParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) const {
    return chi::Task::Deserialize<CreateParamsT>(chimod_params_);
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: chimod_name_, pool_name_, chimod_params_, domain_flags_, pool_id_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(chimod_name_, pool_name_, chimod_params_, domain_flags_, pool_id_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: chimod_name_, chimod_params_, pool_id_, result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(chimod_name_, chimod_params_, pool_id_, result_code_, error_message_);
  }
};


/**
 * CreateTask - Admin container creation task
 * Uses MethodId=kCreate and IS_ADMIN=true
 */
using CreateTask = BaseCreateTask<CreateParams, Method::kCreate, true>;

/**
 * GetOrCreatePoolTask - Template typedef for pool creation by external ChiMods
 * Other ChiMods should inherit this to create their pool creation tasks
 * @tparam CreateParamsT The parameter structure for the specific ChiMod
 */
template<typename CreateParamsT>
using GetOrCreatePoolTask = BaseCreateTask<CreateParamsT, Method::kGetOrCreatePool, false>;

/**
 * DestroyPoolTask - Destroy an existing ChiPool
 */
struct DestroyPoolTask : public chi::Task {
  // Pool destruction parameters
  IN chi::PoolId target_pool_id_;  ///< ID of pool to destroy
  IN chi::u32 destruction_flags_;  ///< Flags controlling destruction behavior

  // Output results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if destruction failed

  /** SHM default constructor */
  explicit DestroyPoolTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        target_pool_id_(0),
        destruction_flags_(0),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit DestroyPoolTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                           const chi::TaskNode &task_node,
                           const chi::PoolId &pool_id,
                           const chi::PoolQuery &dom_query,
                           chi::PoolId target_pool_id,
                           chi::u32 destruction_flags = 0)
      : chi::Task(alloc, task_node, pool_id, dom_query, 10),
        target_pool_id_(target_pool_id),
        destruction_flags_(destruction_flags),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kDestroyPool;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: target_pool_id_, destruction_flags_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(target_pool_id_, destruction_flags_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(result_code_, error_message_);
  }
};

/**
 * StopRuntimeTask - Stop the entire Chimaera runtime
 */
struct StopRuntimeTask : public chi::Task {
  // Runtime shutdown parameters
  IN chi::u32 shutdown_flags_;   ///< Flags controlling shutdown behavior
  IN chi::u32 grace_period_ms_;  ///< Grace period for clean shutdown

  // Output results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if shutdown failed

  /** SHM default constructor */
  explicit StopRuntimeTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        shutdown_flags_(0),
        grace_period_ms_(5000),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit StopRuntimeTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                           const chi::TaskNode &task_node,
                           const chi::PoolId &pool_id,
                           const chi::PoolQuery &dom_query,
                           chi::u32 shutdown_flags = 0,
                           chi::u32 grace_period_ms = 5000)
      : chi::Task(alloc, task_node, pool_id, dom_query, 10),
        shutdown_flags_(shutdown_flags),
        grace_period_ms_(grace_period_ms),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kStopRuntime;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: shutdown_flags_, grace_period_ms_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(shutdown_flags_, grace_period_ms_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(result_code_, error_message_);
  }
};

/**
 * Standard DestroyTask for reuse by all ChiMods
 * All ChiMods should use this same DestroyTask structure
 */
using DestroyTask = DestroyPoolTask;

/**
 * ClientSendTaskInTask - Send task input data to remote node
 * Used for distributed task scheduling when sending tasks to remote nodes
 */
struct ClientSendTaskInTask : public chi::Task {
  // Task data to send
  INOUT chi::TaskInputArchiveOUT task_archive_;
  
  // Network transfer parameters
  IN chi::u32 transfer_flags_;  ///< Flags controlling transfer behavior (CHI_WRITE/CHI_EXPOSE)
  
  // Results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if transfer failed
  
  /** SHM default constructor */
  explicit ClientSendTaskInTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        task_archive_(alloc),
        transfer_flags_(0),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit ClientSendTaskInTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                                const chi::TaskNode &task_node,
                                const chi::PoolId &pool_id,
                                const chi::PoolQuery &dom_query,
                                chi::TaskInputArchiveOUT &&task_archive,
                                chi::u32 transfer_flags = 0)
      : chi::Task(alloc, task_node, pool_id, dom_query, Method::kClientSendTaskIn),
        task_archive_(std::move(task_archive)),
        transfer_flags_(transfer_flags),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kClientSendTaskIn;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: task_archive_, transfer_flags_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(task_archive_, transfer_flags_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: task_archive_, result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(task_archive_, result_code_, error_message_);
  }
};

/**
 * ServerRecvTaskInTask - Receive task input data from remote node
 * Used for distributed task scheduling when receiving tasks from remote nodes
 */
struct ServerRecvTaskInTask : public chi::Task {
  // Received task data
  INOUT chi::TaskInputArchiveIN task_archive_;
  
  // Network transfer parameters
  IN chi::u32 transfer_flags_;  ///< Flags controlling transfer behavior (CHI_WRITE/CHI_EXPOSE)
  
  // Results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if transfer failed
  
  /** SHM default constructor */
  explicit ServerRecvTaskInTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        task_archive_(alloc),
        transfer_flags_(0),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit ServerRecvTaskInTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                                const chi::TaskNode &task_node,
                                const chi::PoolId &pool_id,
                                const chi::PoolQuery &dom_query,
                                chi::TaskInputArchiveIN &&task_archive,
                                chi::u32 transfer_flags = 0)
      : chi::Task(alloc, task_node, pool_id, dom_query, Method::kServerRecvTaskIn),
        task_archive_(std::move(task_archive)),
        transfer_flags_(transfer_flags),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kServerRecvTaskIn;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: task_archive_, transfer_flags_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(task_archive_, transfer_flags_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: task_archive_, result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(task_archive_, result_code_, error_message_);
  }
};

/**
 * ServerSendTaskOutTask - Send task output data to remote node
 * Used for distributed task scheduling when sending completed task results
 */
struct ServerSendTaskOutTask : public chi::Task {
  // Task result data to send
  INOUT chi::TaskOutputArchiveOUT result_archive_;
  
  // Network transfer parameters
  IN chi::u32 transfer_flags_;  ///< Flags controlling transfer behavior (CHI_WRITE/CHI_EXPOSE)
  
  // Results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if transfer failed
  
  /** SHM default constructor */
  explicit ServerSendTaskOutTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        result_archive_(alloc),
        transfer_flags_(0),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit ServerSendTaskOutTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                                 const chi::TaskNode &task_node,
                                 const chi::PoolId &pool_id,
                                 const chi::PoolQuery &dom_query,
                                 chi::TaskOutputArchiveOUT &&result_archive,
                                 chi::u32 transfer_flags = 0)
      : chi::Task(alloc, task_node, pool_id, dom_query, Method::kServerSendTaskOut),
        result_archive_(std::move(result_archive)),
        transfer_flags_(transfer_flags),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kServerSendTaskOut;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: result_archive_, transfer_flags_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(result_archive_, transfer_flags_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: result_archive_, result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(result_archive_, result_code_, error_message_);
  }
};

/**
 * ClientRecvTaskOutTask - Receive task output data from remote node
 * Used for distributed task scheduling when receiving completed task results
 */
struct ClientRecvTaskOutTask : public chi::Task {
  // Received task result data
  INOUT chi::TaskOutputArchiveIN result_archive_;
  
  // Network transfer parameters
  IN chi::u32 transfer_flags_;  ///< Flags controlling transfer behavior (CHI_WRITE/CHI_EXPOSE)
  
  // Results
  OUT chi::u32 result_code_;        ///< Result code (0 = success)
  OUT hipc::string error_message_;  ///< Error description if transfer failed
  
  /** SHM default constructor */
  explicit ClientRecvTaskOutTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc),
        result_archive_(alloc),
        transfer_flags_(0),
        result_code_(0),
        error_message_(alloc) {}

  /** Emplace constructor */
  explicit ClientRecvTaskOutTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                                 const chi::TaskNode &task_node,
                                 const chi::PoolId &pool_id,
                                 const chi::PoolQuery &dom_query,
                                 chi::TaskOutputArchiveIN &&result_archive,
                                 chi::u32 transfer_flags = 0)
      : chi::Task(alloc, task_node, pool_id, dom_query, Method::kClientRecvTaskOut),
        result_archive_(std::move(result_archive)),
        transfer_flags_(transfer_flags),
        result_code_(0),
        error_message_(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kClientRecvTaskOut;
    task_flags_.Clear();
    pool_query_ = dom_query;
  }
  
  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: result_archive_, transfer_flags_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(result_archive_, transfer_flags_);
  }
  
  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: result_archive_, result_code_, error_message_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(result_archive_, result_code_, error_message_);
  }
};

}  // namespace chimaera::admin

#endif  // ADMIN_TASKS_H_