#ifndef ADMIN_RUNTIME_H_
#define ADMIN_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include <chimaera/pool_manager.h>
#include "admin_tasks.h"
#include "admin_client.h"

namespace chimaera::admin {

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
   * Initialize container with pool information
   */
  void Init(const chi::PoolId& pool_id, const std::string& pool_name) override;

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
   * Handle DestroyPool task - Destroy an existing ChiPool
   */
  void DestroyPool(hipc::FullPtr<DestroyPoolTask> task, chi::RunContext& rctx);

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

private:
  /**
   * Initiate runtime shutdown sequence
   */
  void InitiateShutdown(chi::u32 grace_period_ms);
};

} // namespace chimaera::admin

#endif // ADMIN_RUNTIME_H_