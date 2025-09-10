#ifndef ADMIN_RUNTIME_H_
#define ADMIN_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include <chimaera/pool_manager.h>
#include "admin_tasks.h"
#include "admin_client.h"
#include <unordered_map>
#include <vector>

namespace chimaera::admin {

// Admin local queue indices
enum AdminQueueIndex {
  kMetadataQueue = 0,          // Queue for metadata operations
  kClientSendTaskInQueue = 1,  // Queue for client task input processing
  kServerRecvTaskInQueue = 2,  // Queue for server task input reception
  kServerSendTaskOutQueue = 3, // Queue for server task output sending
  kClientRecvTaskOutQueue = 4  // Queue for client task output reception
};

// Forward declarations
// Note: CreateTask and GetOrCreatePoolTask are using aliases defined in admin_tasks.h
// We cannot forward declare using aliases, so we rely on the include


/**
 * Runtime implementation for Admin container
 * 
 * Critical ChiMod responsible for managing ChiPools and runtime lifecycle.
 * Must always be found by the runtime or a fatal error occurs.
 */
class Runtime : public chi::Container {
public:
  // CreateParams type used by CHI_TASK_CC macro for lib_name access
  using CreateParams = chimaera::admin::CreateParams;

private:
  // Container-specific state
  chi::u32 create_count_ = 0;
  chi::u32 pools_created_ = 0;
  chi::u32 pools_destroyed_ = 0;
  
  // Runtime state
  bool is_shutdown_requested_ = false;

  // Client for making calls to this ChiMod
  Client client_;

public:
  /**
   * Constructor
   */
  Runtime() = default;

  /**
   * Destructor
   */
  virtual ~Runtime() = default;


  /**
   * Initialize client for this container
   */
  void InitClient(const chi::PoolId& pool_id) override;

  /**
   * Execute a method on a task
   */
  void Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) override;

  /**
   * Monitor a method execution for scheduling/coordination
   */
  void Monitor(chi::MonitorModeId mode, chi::u32 method, 
              hipc::FullPtr<chi::Task> task_ptr,
              chi::RunContext& rctx) override;

  /**
   * Delete/cleanup a task
   */
  void Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

  //===========================================================================
  // Method implementations
  //===========================================================================

  /**
   * Handle Create task - Initialize the Admin container (IS_ADMIN=true)
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);

  /**
   * Handle GetOrCreatePool task - Pool get-or-create operation (IS_ADMIN=false)
   */
  void GetOrCreatePool(hipc::FullPtr<chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>> task, chi::RunContext& rctx);

  /**
   * Monitor Create task (IS_ADMIN=true)
   */
  void MonitorCreate(chi::MonitorModeId mode, 
                    hipc::FullPtr<CreateTask> task_ptr,
                    chi::RunContext& rctx);

  /**
   * Monitor GetOrCreatePool task (IS_ADMIN=false)
   */
  void MonitorGetOrCreatePool(chi::MonitorModeId mode, 
                             hipc::FullPtr<chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>> task_ptr,
                             chi::RunContext& rctx);


  /**
   * Handle Destroy task - Alias for DestroyPool (DestroyTask = DestroyPoolTask)
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx);

  /**
   * Handle DestroyPool task - Destroy an existing ChiPool
   */
  void DestroyPool(hipc::FullPtr<DestroyPoolTask> task, chi::RunContext& rctx);

  /**
   * Monitor Destroy task - Alias for MonitorDestroyPool (DestroyTask = DestroyPoolTask)
   */
  void MonitorDestroy(chi::MonitorModeId mode, 
                     hipc::FullPtr<DestroyTask> task_ptr,
                     chi::RunContext& rctx);

  /**
   * Monitor DestroyPool task
   */
  void MonitorDestroyPool(chi::MonitorModeId mode, 
                         hipc::FullPtr<DestroyPoolTask> task_ptr,
                         chi::RunContext& rctx);

  /**
   * Handle StopRuntime task - Stop the entire runtime
   */
  void StopRuntime(hipc::FullPtr<StopRuntimeTask> task, chi::RunContext& rctx);

  /**
   * Monitor StopRuntime task
   */
  void MonitorStopRuntime(chi::MonitorModeId mode, 
                         hipc::FullPtr<StopRuntimeTask> task_ptr,
                         chi::RunContext& rctx);

  /**
   * Handle Flush task - Flush administrative operations
   */
  void Flush(hipc::FullPtr<FlushTask> task, chi::RunContext& rctx);

  /**
   * Monitor Flush task
   */
  void MonitorFlush(chi::MonitorModeId mode, 
                   hipc::FullPtr<FlushTask> task_ptr,
                   chi::RunContext& rctx);

  //===========================================================================
  // Distributed Task Scheduling Methods
  //===========================================================================

  /**
   * Helper function to add tasks to the node mapping
   * @param task_to_copy Single task to process
   * @param pool_queries Vector of PoolQuery objects for routing
   * @param node_task_map Reference to the map of node_id -> list of tasks
   * @param container Container to use for task copying
   */
  void AddTasksToMap(
      hipc::FullPtr<chi::Task> task_to_copy,
      const std::vector<chi::PoolQuery>& pool_queries,
      std::unordered_map<chi::u32, std::vector<hipc::FullPtr<chi::Task>>>& node_task_map,
      chi::Container* container);

  /**
   * Handle ClientSendTaskIn - Send task input data to remote node
   */
  void ClientSendTaskIn(hipc::FullPtr<ClientSendTaskInTask> task, chi::RunContext& rctx);

  /**
   * Monitor ClientSendTaskIn task
   */
  void MonitorClientSendTaskIn(chi::MonitorModeId mode, 
                              hipc::FullPtr<ClientSendTaskInTask> task_ptr,
                              chi::RunContext& rctx);

  /**
   * Handle ServerRecvTaskIn - Receive task input data from remote node
   */
  void ServerRecvTaskIn(hipc::FullPtr<ServerRecvTaskInTask> task, chi::RunContext& rctx);

  /**
   * Monitor ServerRecvTaskIn task
   */
  void MonitorServerRecvTaskIn(chi::MonitorModeId mode, 
                              hipc::FullPtr<ServerRecvTaskInTask> task_ptr,
                              chi::RunContext& rctx);

  /**
   * Handle ServerSendTaskOut - Send task output data to remote node
   */
  void ServerSendTaskOut(hipc::FullPtr<ServerSendTaskOutTask> task, chi::RunContext& rctx);

  /**
   * Monitor ServerSendTaskOut task
   */
  void MonitorServerSendTaskOut(chi::MonitorModeId mode, 
                               hipc::FullPtr<ServerSendTaskOutTask> task_ptr,
                               chi::RunContext& rctx);

  /**
   * Handle ClientRecvTaskOut - Receive task output data from remote node
   */
  void ClientRecvTaskOut(hipc::FullPtr<ClientRecvTaskOutTask> task, chi::RunContext& rctx);

  /**
   * Monitor ClientRecvTaskOut task
   */
  void MonitorClientRecvTaskOut(chi::MonitorModeId mode, 
                               hipc::FullPtr<ClientRecvTaskOutTask> task_ptr,
                               chi::RunContext& rctx);

  /**
   * Get remaining work count for this admin container
   * Admin container typically has no pending work, returns 0
   */
  chi::u64 GetWorkRemaining() const override;

  //===========================================================================
  // Task Serialization Methods
  //===========================================================================

  /**
   * Serialize task IN parameters for network transfer
   */
  void SaveIn(chi::u32 method, chi::TaskSaveInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task IN parameters from network transfer
   */
  void LoadIn(chi::u32 method, chi::TaskLoadInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Serialize task OUT parameters for network transfer
   */
  void SaveOut(chi::u32 method, chi::TaskSaveOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task OUT parameters from network transfer
   */
  void LoadOut(chi::u32 method, chi::TaskLoadOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Create a new copy of a task (deep copy for distributed execution)
   */
  void NewCopy(chi::u32 method, 
               const hipc::FullPtr<chi::Task> &orig_task,
               hipc::FullPtr<chi::Task> &dup_task, bool deep) override;

private:
  /**
   * Initiate runtime shutdown sequence
   */
  void InitiateShutdown(chi::u32 grace_period_ms);

  /**
   * Create ZeroMQ client for network communication to a specific node
   * @param node_id Target node ID for the connection
   * @return Unique pointer to ZeroMQ client or nullptr if failed
   */
  std::unique_ptr<hshm::lbm::Client> CreateZmqClient(chi::u32 node_id);
};

} // namespace chimaera::admin

#endif // ADMIN_RUNTIME_H_