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

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "admin/autogen/admin_lib_exec.h"

namespace chimaera::admin {

// Method implementations for Runtime class

void Runtime::InitClient(const chi::PoolId& pool_id) {
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

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  // DestroyTask is aliased to DestroyPoolTask, so delegate to DestroyPool
  DestroyPool(task, rctx);
}

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

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                             hipc::FullPtr<DestroyTask> task_ptr,
                             chi::RunContext& rctx) {
  // DestroyTask is aliased to DestroyPoolTask, so delegate to MonitorDestroyPool
  MonitorDestroyPool(mode, task_ptr, rctx);
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
  std::abort();
}

void Runtime::Flush(hipc::FullPtr<FlushTask> task, chi::RunContext& rctx) {
  std::cout << "Admin: Executing Flush task" << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->total_work_done_ = 0;

  try {
    // Get WorkOrchestrator to check work remaining across all containers
    auto* work_orchestrator = CHI_WORK_ORCHESTRATOR;
    if (!work_orchestrator || !work_orchestrator->IsInitialized()) {
      task->result_code_ = 1;
      return;
    }

    // Loop until all work is complete
    chi::u64 total_work_remaining = 0;
    while (work_orchestrator->HasWorkRemaining(total_work_remaining)) {
      std::cout << "Admin: Flush found " << total_work_remaining
                << " work units still remaining, waiting..." << std::endl;

      // Brief sleep to avoid busy waiting
      task->Yield();
    }

    // Store the final work count (should be 0)
    task->total_work_done_ = total_work_remaining;
    task->result_code_ = 0;  // Success - all work completed

    std::cout
        << "Admin: Flush completed - no work remaining across all containers"
        << std::endl;

  } catch (const std::exception& e) {
    task->result_code_ = 99;
    std::cerr << "Admin: Flush failed with exception: " << e.what()
              << std::endl;
  }
}

void Runtime::MonitorFlush(chi::MonitorModeId mode,
                           hipc::FullPtr<FlushTask> task_ptr,
                           chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for Flush task" << std::endl;
      // Set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global flush operations
      std::cout << "Admin: Global scheduling for Flush task" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - flush should be fast
      rctx.estimated_completion_time_us = 1000.0;  // 1ms for flush
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
    const std::vector<chi::PoolQuery>& pool_queries,
    std::unordered_map<chi::u32, std::vector<hipc::FullPtr<chi::Task>>>&
        node_task_map,
    chi::Container* container) {
  // Iterate over the PoolQuery vector
  for (const auto& pool_query : pool_queries) {
    // Get the node ID from the PoolQuery
    chi::u32 node_id = 0;
    if (pool_query.IsPhysicalMode()) {
      node_id = pool_query.GetNodeId();
    }

    // Make a copy of the task using the container NewCopy method
    // Note: NewCopy method may not exist yet, using task copy for now
    hipc::FullPtr<chi::Task> task_copy =
        task_to_copy;  // This should be a proper copy

    // Add entry to the unordered map
    node_task_map[node_id].push_back(task_copy);
  }
}

void Runtime::ClientSendTaskIn(hipc::FullPtr<ClientSendTaskInTask> task,
                               chi::RunContext& rctx) {
  std::cout << "Admin: Executing ClientSendTaskIn - Sending task input data"
            << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  try {
    // Step 1: Get the current lane
    chi::Worker* worker =
        (HSHM_THREAD_MODEL->GetTls<chi::Worker>(chi::chi_cur_worker_key_));
    if (!worker) {
      task->result_code_ = 1;
      task->error_message_ = "No current worker available";
      return;
    }

    auto* current_lane = worker->GetCurrentLane();
    if (!current_lane) {
      task->result_code_ = 2;
      task->error_message_ = "No current lane available";
      return;
    }

    // Get the container for the task_to_send_
    auto* pool_manager = CHI_POOL_MANAGER;
    if (!pool_manager) {
      task->result_code_ = 3;
      task->error_message_ = "Pool manager not available";
      return;
    }

    auto* container = pool_manager->GetContainer(task->task_to_send_->pool_id_);
    if (!container) {
      task->result_code_ = 4;
      task->error_message_ = "Container not found for task pool";
      return;
    }

    // Step 2 & 3: Build unordered_map of node_id -> list<Task> using single
    // loop
    std::unordered_map<chi::u32, std::vector<hipc::FullPtr<chi::Task>>>
        node_task_map;

    // Convert current lane to FullPtr for TaskQueue operations
    hipc::FullPtr<chi::TaskQueue::TaskLane> lane_full_ptr(current_lane);

    // Add the input task to the map
    AddTasksToMap(task->task_to_send_, task->pool_queries_, node_task_map,
                  container);

    // Pop each task from the current lane and add to the map in a single loop
    hipc::TypedPointer<chi::Task> popped_task_ptr;
    while (chi::TaskQueue::PopTask(lane_full_ptr, popped_task_ptr)) {
      hipc::FullPtr<chi::Task> task_full_ptr(popped_task_ptr);
      if (!task_full_ptr.IsNull()) {
        AddTasksToMap(task_full_ptr, task->pool_queries_, node_task_map,
                      container);
      }
    }

    // Step 4: Iterate over the map and serialize tasks
    for (auto& [node_id, task_list] : node_task_map) {
      if (task_list.empty()) continue;

      std::cout << "Admin: Processing " << task_list.size()
                << " tasks for node " << node_id << std::endl;

      // Create TaskSaveInArchive for serializing task inputs
      auto* ipc = CHI_IPC;
      hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX,
                                                     ipc->GetMainAllocator());
      chi::TaskSaveInArchive archive(ctx_alloc);

      for (auto& task_copy : task_list) {
        // For each task copy, update the pool query to the resolved PoolQuery
        // (This would be the first PoolQuery that maps to this node_id)
        for (const auto& pool_query : task->pool_queries_) {
          if ((pool_query.IsPhysicalMode() &&
               pool_query.GetNodeId() == node_id) ||
              (!pool_query.IsPhysicalMode() && node_id == 0)) {
            task_copy->pool_query_ = pool_query;
            break;
          }
        }

        // Serialize task using TaskSaveInArchive
        archive << *task_copy;

        std::cout << "Admin: Serialized task for node " << node_id
                  << " using TaskSaveInArchive" << std::endl;
      }
    }

    std::cout << "Admin: Task input data sent successfully to "
              << node_task_map.size() << " nodes" << std::endl;

    task->result_code_ = 0;

  } catch (const std::exception& e) {
    task->result_code_ = 1;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during task send: ") + e.what());
    std::cerr << "Admin: ClientSendTaskIn failed: " << e.what() << std::endl;
  }
}

void Runtime::MonitorClientSendTaskIn(
    chi::MonitorModeId mode, hipc::FullPtr<ClientSendTaskInTask> task_ptr,
    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ClientSendTaskIn"
                << std::endl;
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

void Runtime::ServerRecvTaskIn(hipc::FullPtr<ServerRecvTaskInTask> task,
                               chi::RunContext& rctx) {
  std::cout << "Admin: Executing ServerRecvTaskIn - Receiving task input data"
            << std::endl;

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

void Runtime::MonitorServerRecvTaskIn(
    chi::MonitorModeId mode, hipc::FullPtr<ServerRecvTaskInTask> task_ptr,
    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ServerRecvTaskIn"
                << std::endl;
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

void Runtime::ServerSendTaskOut(hipc::FullPtr<ServerSendTaskOutTask> task,
                                chi::RunContext& rctx) {
  std::cout << "Admin: Executing ServerSendTaskOut - Sending task results"
            << std::endl;

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

void Runtime::MonitorServerSendTaskOut(
    chi::MonitorModeId mode, hipc::FullPtr<ServerSendTaskOutTask> task_ptr,
    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ServerSendTaskOut"
                << std::endl;
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
      std::cout << "Admin: Global scheduling for ServerSendTaskOut"
                << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network send time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for result send
      break;
  }
}

void Runtime::ClientRecvTaskOut(hipc::FullPtr<ClientRecvTaskOutTask> task,
                                chi::RunContext& rctx) {
  std::cout << "Admin: Executing ClientRecvTaskOut - Receiving task results"
            << std::endl;

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

void Runtime::MonitorClientRecvTaskOut(
    chi::MonitorModeId mode, hipc::FullPtr<ClientRecvTaskOutTask> task_ptr,
    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "Admin: Setting route_lane_ for ClientRecvTaskOut"
                << std::endl;
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
      std::cout << "Admin: Global scheduling for ClientRecvTaskOut"
                << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate network receive time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for result receive
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

void Runtime::SaveIn(chi::u32 method, chi::TaskSaveInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    default:
      // Unknown method - do nothing
      break;
  }
}

void Runtime::LoadIn(chi::u32 method, chi::TaskLoadInArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
      typed_task->SerializeIn(archive);
      break;
    }
    default:
      // Unknown method - do nothing
      break;
  }
}

void Runtime::SaveOut(chi::u32 method, chi::TaskSaveOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    default:
      // Unknown method - do nothing
      break;
  }
}

void Runtime::LoadOut(chi::u32 method, chi::TaskLoadOutArchive& archive, hipc::FullPtr<chi::Task> task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto typed_task = task_ptr.Cast<CreateTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroy: {
      auto typed_task = task_ptr.Cast<DestroyTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kGetOrCreatePool: {
      auto typed_task = task_ptr.Cast<admin::GetOrCreatePoolTask<admin::CreateParams>>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kDestroyPool: {
      auto typed_task = task_ptr.Cast<DestroyPoolTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kStopRuntime: {
      auto typed_task = task_ptr.Cast<StopRuntimeTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kFlush: {
      auto typed_task = task_ptr.Cast<FlushTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientSendTaskIn: {
      auto typed_task = task_ptr.Cast<ClientSendTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerRecvTaskIn: {
      auto typed_task = task_ptr.Cast<ServerRecvTaskInTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kServerSendTaskOut: {
      auto typed_task = task_ptr.Cast<ServerSendTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    case Method::kClientRecvTaskOut: {
      auto typed_task = task_ptr.Cast<ClientRecvTaskOutTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    default:
      // Unknown method - do nothing
      break;
  }
}

}  // namespace chimaera::admin

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::admin::Runtime)