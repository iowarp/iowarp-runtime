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
      // Route task to local queue
      std::cout << "Admin: Routing admin Create task to local queue"
                << std::endl;
      // Route to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          chi::TaskQueue::EmplaceTask(lane_ptr, task_ptr.shm_);
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
      // Route task to local queue
      std::cout << "Admin: Routing GetOrCreatePool task to local queue"
                << std::endl;
      // Route to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          chi::TaskQueue::EmplaceTask(lane_ptr, task_ptr.shm_);
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
      // Route task to local queue
      std::cout << "Admin: Routing DestroyPool task to local queue"
                << std::endl;
      // Route to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          chi::TaskQueue::EmplaceTask(lane_ptr, task_ptr.shm_);
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
      // Route task to local queue
      std::cout << "Admin: Routing StopRuntime task to local queue"
                << std::endl;
      // Route to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          chi::TaskQueue::EmplaceTask(lane_ptr, task_ptr.shm_);
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