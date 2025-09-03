#ifndef MOD_NAME_CLIENT_H_
#define MOD_NAME_CLIENT_H_

#include <chimaera/chimaera.h>
#include "MOD_NAME_tasks.h"

/**
 * Client API for MOD_NAME
 * 
 * Provides methods for external programs to submit tasks to the runtime.
 */

namespace chimaera::MOD_NAME {

class Client : public chi::ContainerClient {
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
   * Create the container (synchronous)
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& dom_query) {
    auto task = AsyncCreate(mctx, dom_query);
    task->Wait();
    
    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
  }

  /**
   * Create the container (asynchronous)
   */
  hipc::FullPtr<CreateTask> AsyncCreate(const hipc::MemContext& mctx, 
                                       const chi::PoolQuery& dom_query) {
    auto* ipc_manager = CHI_IPC;
    
    // CreateTask is a GetOrCreatePoolTask, which must be handled by admin pool
    // So we send it to admin pool (chi::kAdminPoolId), not to our target pool_id_
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskNode(),
        chi::kAdminPoolId,  // Send to admin pool for GetOrCreatePool processing
        dom_query,
        "chimaera_MOD_NAME_runtime",  // chimod name  
        "mod_name_pool_" + std::to_string(pool_id_.ToU64()),  // pool name
        0,   // domain flags
        pool_id_  // target pool ID to create
    );
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    
    return task;
  }

  /**
   * Execute custom operation (synchronous)
   */
  chi::u32 Custom(const hipc::MemContext& mctx,
             const chi::PoolQuery& dom_query,
             const std::string& input_data,
             chi::u32 operation_id,
             std::string& output_data) {
    auto task = AsyncCustom(mctx, dom_query, input_data, operation_id);
    task->Wait();
    
    // Get results
    output_data = task->data_.str();
    chi::u32 result_code = task->result_code_;
    
    // Clean up task
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
    
    return result_code;
  }

  /**
   * Execute custom operation (asynchronous)
   */
  hipc::FullPtr<CustomTask> AsyncCustom(const hipc::MemContext& mctx,
                                       const chi::PoolQuery& dom_query,
                                       const std::string& input_data,
                                       chi::u32 operation_id) {
    auto* ipc_manager = CHI_IPC;
    
    // Allocate CustomTask
    auto task = ipc_manager->NewTask<CustomTask>(
        chi::CreateTaskNode(),
        pool_id_,
        dom_query,
        input_data,
        operation_id);
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    
    return task;
  }
};

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_CLIENT_H_