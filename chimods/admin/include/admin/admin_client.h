#ifndef ADMIN_CLIENT_H_
#define ADMIN_CLIENT_H_

#include <chimaera/chimaera.h>
#include "admin_tasks.h"

/**
 * Client API for Admin ChiMod
 * 
 * Critical ChiMod for managing ChiPools and runtime lifecycle.
 * Provides methods for external programs to create/destroy pools and stop runtime.
 */

namespace chimaera::admin {

class Client : public chi::ChiContainerClient {
 public:
  /**
   * Default constructor
   */
  Client() = default;

  /**
   * Constructor with pool ID
   */
  explicit Client(const chi::PoolId& pool_id) {
    Init(pool_id);
  }

  /**
   * Create the Admin container (synchronous)
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& dom_query) {
    auto task = AsyncCreate(mctx, dom_query);
    task->Wait();
    
    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Create the Admin container (asynchronous)
   */
  hipc::FullPtr<CreateTask> AsyncCreate(const hipc::MemContext& mctx, 
                                       const chi::PoolQuery& dom_query) {
    auto* ipc_manager = CHI_IPC;
    
    // Allocate CreateTask for admin container creation
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::TaskNode(0), 
        pool_id_,
        dom_query);
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    
    return task;
  }


  /**
   * Destroy an existing ChiPool (synchronous)
   */
  void DestroyPool(const hipc::MemContext& mctx,
                   const chi::PoolQuery& dom_query,
                   chi::PoolId target_pool_id,
                   chi::u32 destruction_flags = 0) {
    auto task = AsyncDestroyPool(mctx, dom_query, target_pool_id, destruction_flags);
    task->Wait();
    
    // Check for errors
    if (task->result_code_ != 0) {
      std::string error = task->error_message_.str();
      auto* ipc_manager = CHI_IPC;
      ipc_manager->DelTask(task);
      throw std::runtime_error("Pool destruction failed: " + error);
    }
    
    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Destroy an existing ChiPool (asynchronous)
   */
  hipc::FullPtr<DestroyPoolTask> AsyncDestroyPool(const hipc::MemContext& mctx,
                                                  const chi::PoolQuery& dom_query,
                                                  chi::PoolId target_pool_id,
                                                  chi::u32 destruction_flags = 0) {
    auto* ipc_manager = CHI_IPC;
    
    // Allocate DestroyPoolTask
    auto task = ipc_manager->NewTask<DestroyPoolTask>(
        chi::TaskNode(0),
        pool_id_,
        dom_query,
        target_pool_id,
        destruction_flags);
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    
    return task;
  }


  /**
   * Stop the entire Chimaera runtime (asynchronous)
   */
  hipc::FullPtr<StopRuntimeTask> AsyncStopRuntime(const hipc::MemContext& mctx,
                                                  const chi::PoolQuery& dom_query,
                                                  chi::u32 shutdown_flags = 0,
                                                  chi::u32 grace_period_ms = 5000) {
    auto* ipc_manager = CHI_IPC;
    
    // Allocate StopRuntimeTask
    auto task = ipc_manager->NewTask<StopRuntimeTask>(
        chi::TaskNode(0),
        pool_id_,
        dom_query,
        shutdown_flags,
        grace_period_ms);
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    
    return task;
  }
};

} // namespace chimaera::admin

#endif // ADMIN_CLIENT_H_