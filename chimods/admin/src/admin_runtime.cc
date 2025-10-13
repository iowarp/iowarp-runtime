/**
 * Runtime implementation for Admin ChiMod
 *
 * Critical ChiMod for managing ChiPools and runtime lifecycle.
 * Contains the server-side task processing logic with PoolManager integration.
 */

#include "chimaera/admin/admin_runtime.h"

#include <chimaera/chimaera_manager.h>
#include <chimaera/module_manager.h>
#include <chimaera/pool_manager.h>
#include <chimaera/task_archives.h>

#include <chrono>
#include <memory>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <zmq.h>

namespace chimaera::admin {

// Method implementations for Runtime class

void Runtime::Init(const chi::PoolId &pool_id, const std::string &pool_name) {
  // Call base class initialization
  chi::Container::Init(pool_id, pool_name);

  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}

// Virtual method implementations now in autogen/admin_lib_exec.cc

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext &rctx) {
  // Admin container creation logic (IS_ADMIN=true)
  HILOG(kDebug, "Admin: Initializing admin container");

  // Initialize the Admin container with pool information from the task
  // Note: Admin container is already initialized by the framework before Create
  // is called

  create_count_++;

  HILOG(kDebug,
        "Admin: Container created and initialized for pool: {} (ID: {}, count: "
        "{})",
        pool_name_, task->new_pool_id_, create_count_);
}

void Runtime::GetOrCreatePool(
    hipc::FullPtr<
        chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>
        task,
    chi::RunContext &rctx) {
  // Pool get-or-create operation logic (IS_ADMIN=false)
  HILOG(kDebug, "Admin: Executing GetOrCreatePool task - ChiMod: {}, Pool: {}",
        task->chimod_name_.str(), task->pool_name_.str());

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    // Get pool manager to handle pool creation
    auto *pool_manager = CHI_POOL_MANAGER;
    if (!pool_manager || !pool_manager->IsInitialized()) {
      task->return_code_ = 1;
      task->error_message_ = "Pool manager not available";
      return;
    }

    // Use the simplified PoolManager API that extracts all parameters from the
    // task
    if (!pool_manager->CreatePool(task.Cast<chi::Task>(), &rctx)) {
      task->return_code_ = 2;
      task->error_message_ = "Failed to create or get pool via PoolManager";
      return;
    }

    // Set success results (task->new_pool_id_ is already updated by CreatePool)
    task->return_code_ = 0;
    pools_created_++;

    HILOG(kDebug,
          "Admin: Pool operation completed successfully - ID: {}, Name: {} "
          "(Total pools created: {})",
          task->new_pool_id_, task->pool_name_.str(), pools_created_);

  } catch (const std::exception &e) {
    task->return_code_ = 99;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during pool creation: ") + e.what());
    HELOG(kError, "Admin: Pool creation failed with exception: {}", e.what());
  }
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task_ptr,
                            chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global distribution
    HILOG(kDebug, "Admin: Global scheduling for admin Create task");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate task execution time - admin container creation is fast
    rctx.estimated_completion_time_us = 1000.0; // 1ms for admin create
    break;
  }
}

void Runtime::MonitorGetOrCreatePool(
    chi::MonitorModeId mode,
    hipc::FullPtr<
        chimaera::admin::GetOrCreatePoolTask<chimaera::admin::CreateParams>>
        task_ptr,
    chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global distribution
    HILOG(kDebug, "Admin: Global scheduling for GetOrCreatePool task");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate task execution time - pool creation can be expensive
    rctx.estimated_completion_time_us = 50000.0; // 50ms for pool creation
    break;
  }
}

// GetOrCreatePool functionality is now merged into Create method

// MonitorGetOrCreatePool functionality is now merged into MonitorCreate method

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext &rctx) {
  // DestroyTask is aliased to DestroyPoolTask, so delegate to DestroyPool
  DestroyPool(task, rctx);
}

void Runtime::DestroyPool(hipc::FullPtr<DestroyPoolTask> task,
                          chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing DestroyPool task - Pool ID: {}",
        task->target_pool_id_);

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    chi::PoolId target_pool = task->target_pool_id_;

    // Get pool manager to handle pool destruction
    auto *pool_manager = CHI_POOL_MANAGER;
    if (!pool_manager || !pool_manager->IsInitialized()) {
      task->return_code_ = 1;
      task->error_message_ = "Pool manager not available";
      return;
    }

    // Use PoolManager to destroy the complete pool including metadata
    if (!pool_manager->DestroyPool(target_pool)) {
      task->return_code_ = 2;
      task->error_message_ = "Failed to destroy pool via PoolManager";
      return;
    }

    // Set success results
    task->return_code_ = 0;
    pools_destroyed_++;

    HILOG(kDebug,
          "Admin: Pool destroyed successfully - ID: {} (Total pools destroyed: "
          "{})",
          target_pool, pools_destroyed_);

  } catch (const std::exception &e) {
    task->return_code_ = 99;
    task->error_message_ =
        std::string("Exception during pool destruction: ") + e.what();
    HELOG(kError, "Admin: Pool destruction failed with exception: {}",
          e.what());
  }
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                             hipc::FullPtr<DestroyTask> task_ptr,
                             chi::RunContext &rctx) {
  // DestroyTask is aliased to DestroyPoolTask, so delegate to
  // MonitorDestroyPool
  MonitorDestroyPool(mode, task_ptr, rctx);
}

void Runtime::MonitorDestroyPool(chi::MonitorModeId mode,
                                 hipc::FullPtr<DestroyPoolTask> task_ptr,
                                 chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global pool destruction
    HILOG(kDebug, "Admin: Global scheduling for DestroyPool task");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate task execution time - pool destruction is expensive
    rctx.estimated_completion_time_us = 30000.0; // 30ms for pool destruction
    break;
  }
}

void Runtime::StopRuntime(hipc::FullPtr<StopRuntimeTask> task,
                          chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing StopRuntime task - Grace period: {}ms",
        task->grace_period_ms_);

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    // Set shutdown flag
    is_shutdown_requested_ = true;

    // Initiate graceful shutdown
    InitiateShutdown(task->grace_period_ms_);

    // Set success results
    task->return_code_ = 0;

    HILOG(kDebug, "Admin: Runtime shutdown initiated successfully");

  } catch (const std::exception &e) {
    task->return_code_ = 99;
    task->error_message_ =
        std::string("Exception during runtime shutdown: ") + e.what();
    HELOG(kError, "Admin: Runtime shutdown failed with exception: {}",
          e.what());
  }
}

void Runtime::MonitorStopRuntime(chi::MonitorModeId mode,
                                 hipc::FullPtr<StopRuntimeTask> task_ptr,
                                 chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global runtime shutdown
    HILOG(kDebug, "Admin: Global scheduling for StopRuntime task");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate task execution time - shutdown should be fast
    rctx.estimated_completion_time_us = 5000.0; // 5ms for shutdown initiation
    break;
  }
}

void Runtime::InitiateShutdown(chi::u32 grace_period_ms) {
  HILOG(kDebug, "Admin: Initiating runtime shutdown with {}ms grace period",
        grace_period_ms);

  // In a real implementation, this would:
  // 1. Signal all worker threads to stop
  // 2. Wait for current tasks to complete (up to grace period)
  // 3. Clean up all resources
  // 4. Exit the runtime process

  // For now, we'll just set a flag that other components can check
  is_shutdown_requested_ = true;

  // Get Chimaera manager to initiate shutdown
  auto *chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager) {
    // chimaera_manager->InitiateShutdown(grace_period_ms);
  }
  std::abort();
}

void Runtime::Flush(hipc::FullPtr<FlushTask> task, chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing Flush task");

  // Initialize output values
  task->return_code_ = 0;
  task->total_work_done_ = 0;

  try {
    // Get WorkOrchestrator to check work remaining across all containers
    auto *work_orchestrator = CHI_WORK_ORCHESTRATOR;
    if (!work_orchestrator || !work_orchestrator->IsInitialized()) {
      task->return_code_ = 1;
      return;
    }

    // Loop until all work is complete
    chi::u64 total_work_remaining = 0;
    while (work_orchestrator->HasWorkRemaining(total_work_remaining)) {
      HILOG(kDebug,
            "Admin: Flush found {} work units still remaining, waiting...",
            total_work_remaining);

      // Brief sleep to avoid busy waiting
      task->Yield();
    }

    // Store the final work count (should be 0)
    task->total_work_done_ = total_work_remaining;
    task->return_code_ = 0; // Success - all work completed

    HILOG(kDebug,
          "Admin: Flush completed - no work remaining across all containers");

  } catch (const std::exception &e) {
    task->return_code_ = 99;
    HELOG(kError, "Admin: Flush failed with exception: {}", e.what());
  }
}

void Runtime::MonitorFlush(chi::MonitorModeId mode,
                           hipc::FullPtr<FlushTask> task_ptr,
                           chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global flush operations
    HILOG(kDebug, "Admin: Global scheduling for Flush task");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate task execution time - flush should be fast
    rctx.estimated_completion_time_us = 1000.0; // 1ms for flush
    break;
  }
}

//===========================================================================
// Distributed Task Scheduling Method Implementations
//===========================================================================

void Runtime::Send(hipc::FullPtr<SendTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "Admin: Executing Send - TODO: implement networking");
  
  // TODO: Implement full network send functionality
  // This requires networking APIs that don't exist yet
  
  auto* main_allocator = HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
  
  hipc::FullPtr<chi::Task> subtask = task->subtask_;
  if (subtask.IsNull()) {
    task->error_message_ = hipc::string(main_allocator, "Subtask is null");
    task->SetReturnCode(1);
    return;
  }
  
  // Stub: just mark success for now
  task->SetReturnCode(0);
  (void)rctx;  // Suppress unused warning
}

void Runtime::MonitorSend(chi::MonitorModeId mode, hipc::FullPtr<SendTask> task_ptr,
                          chi::RunContext& rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    break;
  case chi::MonitorModeId::kGlobalSchedule:
    break;
  case chi::MonitorModeId::kEstLoad:
    rctx.estimated_completion_time_us = 10000.0;  // 10ms estimate
    break;
  }
  (void)task_ptr;  // Suppress unused warning
}

void Runtime::Recv(hipc::FullPtr<RecvTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "Admin: Executing Recv - TODO: implement networking");
  
  // TODO: Implement full network receive functionality
  // This requires networking APIs that don't exist yet
  
  // Stub: just mark success for now
  task->SetReturnCode(0);
  (void)rctx;  // Suppress unused warning
}

void Runtime::MonitorRecv(chi::MonitorModeId mode, hipc::FullPtr<RecvTask> task_ptr,
                          chi::RunContext& rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    break;
  case chi::MonitorModeId::kGlobalSchedule:
    break;
  case chi::MonitorModeId::kEstLoad:
    rctx.estimated_completion_time_us = 10000.0;  // 10ms estimate
    break;
  }
  (void)task_ptr;  // Suppress unused warning
}

chi::u64 Runtime::GetWorkRemaining() const {
  // Admin container typically has no pending work
  // In a real implementation, this could track pending administrative
  // operations
  return 0;
}

//===========================================================================
// Task Serialization Method Implementations
//===========================================================================

// Task Serialization Method Implementations now in autogen/admin_lib_exec.cc

} // namespace chimaera::admin

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::admin::Runtime)