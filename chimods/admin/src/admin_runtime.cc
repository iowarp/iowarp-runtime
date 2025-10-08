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

// ZeroMQ and lightbeam networking includes (through HSHM)
// Note: lightbeam types are available through hermes_shm.h already included in
// types.h

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

std::unique_ptr<hshm::lbm::Client> Runtime::CreateZmqClient(chi::u32 node_id) {
  try {
    // Get configuration for ZeroMQ connections
    chi::ConfigManager *config = CHI_CONFIG_MANAGER;
    if (!config) {
      HELOG(kError, "Admin: Config manager not available for ZMQ client");
      return nullptr;
    }

    // For now, use localhost with standard port + node_id offset
    // In a real implementation, this would use a node registry/discovery
    // service
    std::string target_addr = "127.0.0.1";
    std::string protocol = "tcp";
    chi::u32 base_port = config->GetZmqPort();
    chi::u32 target_port = base_port + node_id;

    HILOG(kDebug, "Admin: Creating ZMQ client connection to node {} at {}:{}",
          node_id, target_addr, target_port);

    // Retry connection creation with exponential backoff
    const int max_retries = 3;
    const int base_delay_ms = 100;

    for (int retry = 0; retry < max_retries; ++retry) {
      try {
        // Create ZeroMQ client using HSHM lightbeam
        auto zmq_client = hshm::lbm::TransportFactory::GetClient(
            target_addr, hshm::lbm::Transport::kZeroMq, protocol, target_port);

        if (zmq_client) {
          HILOG(kDebug,
                "Admin: Successfully created ZMQ client for node {} on attempt "
                "{}",
                node_id, (retry + 1));
          return zmq_client;
        }

        if (retry < max_retries - 1) {
          int delay_ms = base_delay_ms * (1 << retry); // Exponential backoff
          HILOG(kDebug,
                "Admin: ZMQ client creation attempt {} failed, retrying in "
                "{}ms...",
                (retry + 1), delay_ms);
          std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
      } catch (const std::exception &retry_ex) {
        HELOG(kError,
              "Admin: ZMQ client creation attempt {} threw exception: {}",
              (retry + 1), retry_ex.what());
        if (retry == max_retries - 1) {
          throw; // Re-throw on final attempt
        }
      }
    }

    HELOG(kError,
          "Admin: Failed to create ZMQ client for node {} after {} attempts",
          node_id, max_retries);
    return nullptr;

  } catch (const std::exception &e) {
    HELOG(kError, "Admin: Exception creating ZMQ client for node {}: {}",
          node_id, e.what());
    return nullptr;
  }
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

/**
 * Helper function to add tasks to the node mapping
 * @param task_to_copy Single task to process
 * @param pool_queries Vector of PoolQuery objects for routing
 * @param node_task_map Reference to the map of node_id -> list of tasks
 * @param container Container to use for task copying
 */
void Runtime::AddTasksToMap(
    hipc::FullPtr<chi::Task> task_to_copy,
    const std::vector<chi::PoolQuery> &pool_queries,
    std::unordered_map<chi::u32, std::vector<hipc::FullPtr<chi::Task>>>
        &node_task_map,
    chi::Container *container) {
  // Iterate over the PoolQuery vector
  for (const auto &pool_query : pool_queries) {
    // Get the node ID from the PoolQuery
    chi::u32 node_id = 0;
    if (pool_query.IsPhysicalMode()) {
      node_id = pool_query.GetNodeId();
    }

    // Make a proper copy of the task using container's pool manager approach
    hipc::FullPtr<chi::Task> task_copy;

    if (!task_to_copy.IsNull()) {
      try {
        // Get the container from the pool manager using the task's pool_id
        auto *pool_manager = CHI_POOL_MANAGER;
        if (!pool_manager || !pool_manager->IsInitialized()) {
          HELOG(kError, "Admin: Pool manager not available for task copy");
          continue; // Skip this pool_query iteration if pool manager not
                    // available
        }

        auto *source_container =
            pool_manager->GetContainer(task_to_copy->pool_id_);
        if (!source_container) {
          HELOG(kError, "Admin: Container not found for pool_id {}",
                task_to_copy->pool_id_);
          continue; // Skip this pool_query iteration if container not found
        }

        // Use the container's NewCopy method to properly copy the task
        source_container->NewCopy(task_to_copy->method_, task_to_copy,
                                  task_copy, true);

        if (task_copy.IsNull()) {
          HELOG(kError, "Admin: NewCopy failed to create task copy");
          continue; // Skip this pool_query iteration if NewCopy failed
        }

      } catch (const std::exception &e) {
        HELOG(kError, "Admin: Exception during task copy creation: {}",
              e.what());
        continue; // Skip this pool_query iteration if copy creation failed
      }
    } else {
      HELOG(kError, "Admin: Cannot copy null task");
      continue; // Skip this pool_query iteration if source task is null
    }

    // Add entry to the unordered map
    node_task_map[node_id].push_back(task_copy);
  }
}

void Runtime::ClientSendTaskIn(hipc::FullPtr<ClientSendTaskInTask> task,
                               chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing ClientSendTaskIn - Sending task input data");
}

void Runtime::MonitorClientSendTaskIn(
    chi::MonitorModeId mode, hipc::FullPtr<ClientSendTaskInTask> task_ptr,
    chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global network send
    HILOG(kDebug, "Admin: Global scheduling for ClientSendTaskIn");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate network send time
    rctx.estimated_completion_time_us = 10000.0; // 10ms for network send
    break;
  }
}

void Runtime::ServerRecvTaskIn(hipc::FullPtr<ServerRecvTaskInTask> task,
                               chi::RunContext &rctx) {
  HILOG(kDebug,
        "Admin: Executing ServerRecvTaskIn - Receiving task input data");

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Receive the serialized task data from network layer
    // 2. Deserialize the task using the specific method type
    // 3. Create a new local task instance
    // 4. Submit the task to the local worker for execution

    HILOG(kDebug, "Admin: Task input data received successfully");

    task->return_code_ = 0;

  } catch (const std::exception &e) {
    task->return_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during task receive: ") + e.what());
    HELOG(kError, "Admin: ServerRecvTaskIn failed: {}", e.what());
  }
}

void Runtime::MonitorServerRecvTaskIn(
    chi::MonitorModeId mode, hipc::FullPtr<ServerRecvTaskInTask> task_ptr,
    chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global network receive
    HILOG(kDebug, "Admin: Global scheduling for ServerRecvTaskIn");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate network receive time
    rctx.estimated_completion_time_us = 10000.0; // 10ms for network receive
    break;
  }
}

void Runtime::ServerSendTaskOut(hipc::FullPtr<ServerSendTaskOutTask> task,
                                chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing ServerSendTaskOut - Sending task results");

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";
}

void Runtime::MonitorServerSendTaskOut(
    chi::MonitorModeId mode, hipc::FullPtr<ServerSendTaskOutTask> task_ptr,
    chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global network send
    HILOG(kDebug, "Admin: Global scheduling for ServerSendTaskOut");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate network send time
    rctx.estimated_completion_time_us = 10000.0; // 10ms for result send
    break;
  }
}

void Runtime::ClientRecvTaskOut(hipc::FullPtr<ClientRecvTaskOutTask> task,
                                chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing ClientRecvTaskOut - Receiving task results");

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    // In a real implementation, this would:
    // 1. Receive the serialized task result from network layer
    // 2. Deserialize the result data
    // 3. Update the original task with the result
    // 4. Mark the task as completed

    HILOG(kDebug, "Admin: Task results received successfully");

    task->return_code_ = 0;

  } catch (const std::exception &e) {
    task->return_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during result receive: ") + e.what());
    HELOG(kError, "Admin: ClientRecvTaskOut failed: {}", e.what());
  }
}

void Runtime::MonitorClientRecvTaskOut(
    chi::MonitorModeId mode, hipc::FullPtr<ClientRecvTaskOutTask> task_ptr,
    chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    // Task executes directly on current worker without re-routing
    break;

  case chi::MonitorModeId::kGlobalSchedule:
    // Coordinate global network receive
    HILOG(kDebug, "Admin: Global scheduling for ClientRecvTaskOut");
    break;

  case chi::MonitorModeId::kEstLoad:
    // Estimate network receive time
    rctx.estimated_completion_time_us = 10000.0; // 10ms for result receive
    break;
  }
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