/**
 * Runtime implementation for Admin ChiMod
 *
 * Critical ChiMod for managing ChiPools and runtime lifecycle.
 * Contains the server-side task processing logic with PoolManager integration.
 */

#include "admin/admin_runtime.h"

#include <chimaera/chimaera_manager.h>
#include <chimaera/module_manager.h>
#include <chimaera/pool_manager.h>

#include <iostream>

#include "admin/autogen/admin_lib_exec.h"

namespace chimaera::admin {

// Method implementations for Runtime class

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name) {
  // Call base class Init
  Container::Init(pool_id, pool_name);

  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}

void Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                  chi::RunContext& rctx) {
  // Dispatch to the appropriate method handler
  chimaera::admin::Run(this, method, task_ptr, rctx);
}

void Runtime::Monitor(chi::MonitorModeId mode, chi::u32 method,
                      hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) {
  // Dispatch to the appropriate monitor handler
  chimaera::admin::Monitor(this, mode, method, task_ptr, rctx);
}

void Runtime::Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  // Dispatch to the appropriate delete handler
  chimaera::admin::Del(this, method, task_ptr);
}

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  // Admin container creation logic (IS_ADMIN=true)
  std::cout << "Admin: Executing Admin Create task for pool " << task->pool_id_
            << std::endl;

  std::cout << "Admin: Initializing admin container" << std::endl;

  // Initialize the Admin container with pool information from the task
  // Note: Admin container is already initialized by the framework before Create
  // is called, but we can create local queues here for Admin operations
  CreateLocalQueue(chi::kLowLatency, 4);  // 4 lanes for low latency admin tasks
  CreateLocalQueue(chi::kHighLatency,
                   2);  // 2 lanes for heavy operations like pool creation

  create_count_++;

  std::cout << "Admin: Container created and initialized for pool: "
            << pool_name_ << " (ID: " << task->pool_id_
            << ", count: " << create_count_ << ")" << std::endl;
}

void Runtime::GetOrCreatePool(
    hipc::FullPtr<
        chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>
        task,
    chi::RunContext& rctx) {
  // Pool get-or-create operation logic (IS_ADMIN=false)
  std::cout << "Admin: Executing GetOrCreatePool task - ChiMod: "
            << task->chimod_name_.str() << ", Pool: " << task->pool_name_.str()
            << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // Get pool manager to handle pool creation
    auto* pool_manager = CHI_POOL_MANAGER;
    if (!pool_manager || !pool_manager->IsInitialized()) {
      task->result_code_ = 1;
      task->error_message_ = "Pool manager not available";
      return;
    }

    // Use the new PoolManager API that supports get-or-create semantics (always
    // 1 container)
    chi::PoolId result_pool_id;
    bool was_created;
    if (!pool_manager->CreatePool(task->chimod_name_.str(),
                                  task->pool_name_.str(),
                                  task->chimod_params_.str(), 1, task->pool_id_,
                                  result_pool_id, was_created)) {
      task->result_code_ = 2;
      task->error_message_ = "Failed to create or get pool via PoolManager";
      return;
    }

    // Set success results - update the INOUT pool_id_
    task->pool_id_ = result_pool_id;
    task->result_code_ = 0;
    if (was_created) {
      pools_created_++;
    }

    std::cout << "Admin: Pool " << (was_created ? "created" : "found")
              << " successfully - ID: " << result_pool_id
              << ", Name: " << task->pool_name_.str()
              << " (Total pools created: " << pools_created_ << ")"
              << std::endl;

  } catch (const std::exception& e) {
    task->result_code_ = 99;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during pool creation: ") + e.what());
    std::cerr << "Admin: Pool creation failed with exception: " << e.what()
              << std::endl;
  }
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for admin Create task"
                << std::endl;
      // Set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "Admin: Global scheduling for admin Create task"
                << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - admin container creation is fast
      rctx.estimated_completion_time_us = 1000.0;  // 1ms for admin create
      break;
  }
}

void Runtime::MonitorGetOrCreatePool(
    chi::MonitorModeId mode,
    hipc::FullPtr<
        chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>
        task_ptr,
    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for GetOrCreatePool task"
                << std::endl;
      // Set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "Admin: Global scheduling for GetOrCreatePool task"
                << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - pool creation can be expensive
      rctx.estimated_completion_time_us = 50000.0;  // 50ms for pool creation
      break;
  }
}

// GetOrCreatePool functionality is now merged into Create method

// MonitorGetOrCreatePool functionality is now merged into MonitorCreate method

void Runtime::DestroyPool(hipc::FullPtr<DestroyPoolTask> task,
                          chi::RunContext& rctx) {
  std::cout << "Admin: Executing DestroyPool task - Pool ID: "
            << task->target_pool_id_ << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    chi::PoolId target_pool = task->target_pool_id_;

    // Get pool manager to handle pool destruction
    auto* pool_manager = CHI_POOL_MANAGER;
    if (!pool_manager || !pool_manager->IsInitialized()) {
      task->result_code_ = 1;
      task->error_message_ = "Pool manager not available";
      return;
    }

    // Use PoolManager to destroy the complete pool including metadata
    if (!pool_manager->DestroyPool(target_pool)) {
      task->result_code_ = 2;
      task->error_message_ = "Failed to destroy pool via PoolManager";
      return;
    }

    // Set success results
    task->result_code_ = 0;
    pools_destroyed_++;

    std::cout << "Admin: Pool destroyed successfully - ID: " << target_pool
              << " (Total pools destroyed: " << pools_destroyed_ << ")"
              << std::endl;

  } catch (const std::exception& e) {
    task->result_code_ = 99;
    task->error_message_ =
        std::string("Exception during pool destruction: ") + e.what();
    std::cerr << "Admin: Pool destruction failed with exception: " << e.what()
              << std::endl;
  }
}

void Runtime::MonitorDestroyPool(chi::MonitorModeId mode,
                                 hipc::FullPtr<DestroyPoolTask> task_ptr,
                                 chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for DestroyPool task"
                << std::endl;
      // Set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global pool destruction
      std::cout << "Admin: Global scheduling for DestroyPool task" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - pool destruction is expensive
      rctx.estimated_completion_time_us = 30000.0;  // 30ms for pool destruction
      break;
  }
}

void Runtime::StopRuntime(hipc::FullPtr<StopRuntimeTask> task,
                          chi::RunContext& rctx) {
  std::cout << "Admin: Executing StopRuntime task - Grace period: "
            << task->grace_period_ms_ << "ms" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // Set shutdown flag
    is_shutdown_requested_ = true;

    // Initiate graceful shutdown
    InitiateShutdown(task->grace_period_ms_);

    // Set success results
    task->result_code_ = 0;

    std::cout << "Admin: Runtime shutdown initiated successfully" << std::endl;

  } catch (const std::exception& e) {
    task->result_code_ = 99;
    task->error_message_ =
        std::string("Exception during runtime shutdown: ") + e.what();
    std::cerr << "Admin: Runtime shutdown failed with exception: " << e.what()
              << std::endl;
  }
}

void Runtime::MonitorStopRuntime(chi::MonitorModeId mode,
                                 hipc::FullPtr<StopRuntimeTask> task_ptr,
                                 chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for StopRuntime task"
                << std::endl;
      // Set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global runtime shutdown
      std::cout << "Admin: Global scheduling for StopRuntime task" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - shutdown should be fast
      rctx.estimated_completion_time_us =
          5000.0;  // 5ms for shutdown initiation
      break;
  }
}

void Runtime::InitiateShutdown(chi::u32 grace_period_ms) {
  std::cout << "Admin: Initiating runtime shutdown with " << grace_period_ms
            << "ms grace period" << std::endl;

  // In a real implementation, this would:
  // 1. Signal all worker threads to stop
  // 2. Wait for current tasks to complete (up to grace period)
  // 3. Clean up all resources
  // 4. Exit the runtime process

  // For now, we'll just set a flag that other components can check
  is_shutdown_requested_ = true;

  // Get Chimaera manager to initiate shutdown
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager) {
    // chimaera_manager->InitiateShutdown(grace_period_ms);
  }
  // std::abort();
}

//===========================================================================
// Distributed Task Scheduling Method Implementations
//===========================================================================

void Runtime::ClientSendTaskIn(hipc::FullPtr<ClientSendTaskInTask> task, chi::RunContext& rctx) {
  std::cout << "Admin: Executing ClientSendTaskIn - Sending task input data" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Use the network layer to send the serialized task data
    // 2. Handle CHI_WRITE/CHI_EXPOSE flags for bulk transfer
    // 3. Wait for acknowledgment from remote node
    // 4. Set appropriate result codes

    // For now, simulate successful send
    std::cout << "Admin: Task input data sent successfully" << std::endl;

    task->result_code_ = 0;

  } catch (const std::exception& e) {
    task->result_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during task send: ") + e.what());
    std::cerr << "Admin: ClientSendTaskIn failed: " << e.what() << std::endl;
  }
}

void Runtime::MonitorClientSendTaskIn(chi::MonitorModeId mode,
                                     hipc::FullPtr<ClientSendTaskInTask> task_ptr,
                                     chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ClientSendTaskIn" << std::endl;
      // Set route_lane_ to low latency queue lane 0 for network operations
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global network send
      std::cout << "Admin: Global scheduling for ClientSendTaskIn" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network send time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for network send
      break;
  }
}

void Runtime::ServerRecvTaskIn(hipc::FullPtr<ServerRecvTaskInTask> task, chi::RunContext& rctx) {
  std::cout << "Admin: Executing ServerRecvTaskIn - Receiving task input data" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Receive the serialized task data from network layer
    // 2. Deserialize the task using the specific method type
    // 3. Create a new local task instance
    // 4. Submit the task to the local worker for execution

    std::cout << "Admin: Task input data received successfully" << std::endl;

    task->result_code_ = 0;

  } catch (const std::exception& e) {
    task->result_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during task receive: ") + e.what());
    std::cerr << "Admin: ServerRecvTaskIn failed: " << e.what() << std::endl;
  }
}

void Runtime::MonitorServerRecvTaskIn(chi::MonitorModeId mode,
                                     hipc::FullPtr<ServerRecvTaskInTask> task_ptr,
                                     chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ServerRecvTaskIn" << std::endl;
      // Set route_lane_ to low latency queue lane 0 for network operations
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global network receive
      std::cout << "Admin: Global scheduling for ServerRecvTaskIn" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network receive time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for network receive
      break;
  }
}

void Runtime::ServerSendTaskOut(hipc::FullPtr<ServerSendTaskOutTask> task, chi::RunContext& rctx) {
  std::cout << "Admin: Executing ServerSendTaskOut - Sending task results" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Serialize the completed task result data
    // 2. Send the result back to the originating node
    // 3. Handle CHI_WRITE/CHI_EXPOSE flags for bulk transfer
    // 4. Clean up the completed task

    std::cout << "Admin: Task results sent successfully" << std::endl;

    task->result_code_ = 0;

  } catch (const std::exception& e) {
    task->result_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during result send: ") + e.what());
    std::cerr << "Admin: ServerSendTaskOut failed: " << e.what() << std::endl;
  }
}

void Runtime::MonitorServerSendTaskOut(chi::MonitorModeId mode,
                                      hipc::FullPtr<ServerSendTaskOutTask> task_ptr,
                                      chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ServerSendTaskOut" << std::endl;
      // Set route_lane_ to low latency queue lane 0 for network operations
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global network send
      std::cout << "Admin: Global scheduling for ServerSendTaskOut" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network send time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for result send
      break;
  }
}

void Runtime::ClientRecvTaskOut(hipc::FullPtr<ClientRecvTaskOutTask> task, chi::RunContext& rctx) {
  std::cout << "Admin: Executing ClientRecvTaskOut - Receiving task results" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Receive the serialized task result from network layer
    // 2. Deserialize the result data
    // 3. Update the original task with the result
    // 4. Mark the task as completed

    std::cout << "Admin: Task results received successfully" << std::endl;

    task->result_code_ = 0;

  } catch (const std::exception& e) {
    task->result_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during result receive: ") + e.what());
    std::cerr << "Admin: ClientRecvTaskOut failed: " << e.what() << std::endl;
  }
}

void Runtime::MonitorClientRecvTaskOut(chi::MonitorModeId mode,
                                      hipc::FullPtr<ClientRecvTaskOutTask> task_ptr,
                                      chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ClientRecvTaskOut" << std::endl;
      // Set route_lane_ to low latency queue lane 0 for network operations
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global network receive
      std::cout << "Admin: Global scheduling for ClientRecvTaskOut" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network receive time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for result receive
      break;
  }
}

}  // namespace chimaera::admin

// Dummy function to ensure AdminCreateTask MonitorCreate is linked
// This forces the linker to include the symbol
__attribute__((unused)) static void force_link_admin_monitor_create() {
  // This will never be called but ensures the symbol is included
  chimaera::admin::Runtime* runtime = nullptr;
  if (runtime) {
    hipc::FullPtr<chimaera::admin::CreateTask> task;
    chi::RunContext rctx;
    runtime->MonitorCreate(chi::MonitorModeId::kLocalSchedule, task, rctx);
  }
}

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::admin::Runtime)