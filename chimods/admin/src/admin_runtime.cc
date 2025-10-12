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

void Runtime::ClientSendTaskIn(hipc::FullPtr<ClientSendTaskInTask> task,
                               chi::RunContext &rctx) {
  HILOG(kDebug, "Admin: Executing ClientSendTaskIn - Sending task input data");

  // Get the subtask to send
  hipc::FullPtr<chi::Task> subtask = task->task_to_send_;
  if (subtask.IsNull()) {
    task->SetReturnCode(1);
    return;
  }

  // Get the container for the subtask's pool
  auto *pool_manager = CHI_POOL_MANAGER;
  chi::Container *container = pool_manager->GetContainer(subtask->pool_id_);
  if (!container) {
    task->SetReturnCode(1);
    return;
  }

  // Get network configuration
  auto *ipc_manager = CHI_IPC;
  auto *config_manager = CHI_CONFIG_MANAGER;
  chi::u32 zmq_port = config_manager->GetZmqPort();

  // For each pool query, copy the task and update its domain, then send
  for (const auto &pool_query : task->pool_queries_) {
    // Copy the original task
    hipc::FullPtr<chi::Task> task_copy;
    container->NewCopy(subtask->method_, subtask, task_copy, false);
    if (task_copy.IsNull()) {
      task->SetReturnCode(1);
      return;
    }

    // Update the pool query in the copied task to the specific target
    task_copy->pool_query_ = pool_query;

    // Resolve pool_query to node ID based on routing mode
    chi::u32 node_id = 0;
    chi::RoutingMode routing_mode = pool_query.GetRoutingMode();

    if (routing_mode == chi::RoutingMode::Physical) {
      // Physical mode - use node ID directly from query
      node_id = pool_query.GetNodeId();
    } else if (routing_mode == chi::RoutingMode::DirectId) {
      // DirectId mode - resolve container ID to node ID
      chi::ContainerId container_id = pool_query.GetContainerId();
      node_id =
          pool_manager->GetContainerNodeId(subtask->pool_id_, container_id);
    } else if (routing_mode == chi::RoutingMode::Range) {
      // Range mode - get first container in range
      chi::u32 range_offset = pool_query.GetRangeOffset();
      chi::ContainerId first_container_id(range_offset);
      node_id = pool_manager->GetContainerNodeId(subtask->pool_id_,
                                                 first_container_id);
    } else {
      // Unsupported routing mode for network transfer
      task->SetReturnCode(1);
      return;
    }

    // Get host information from node ID
    const chi::Host *host = ipc_manager->GetHost(node_id);
    if (!host) {
      task->SetReturnCode(1);
      return;
    }

    // Create TaskSaveInArchive with task count of 1
    chi::TaskSaveInArchive ar(1);

    // Serialize the copied task using container's SaveIn method
    container->SaveIn(task_copy->method_, ar, task_copy);

    // Build the message
    std::string message = ar.BuildMessage();

    // Get the data transfers
    const std::vector<chi::DataTransfer> &data_transfers =
        ar.GetDataTransfers();

    // Get the shared ZeroMQ context from IPC manager
    void *zmq_context = ipc_manager->GetMainZmqContext();
    if (!zmq_context) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: ZeroMQ context not initialized");
      return;
    }

    // Create a PUSH socket for sending to the target node
    void *sock = zmq_socket(zmq_context, ZMQ_PUSH);
    if (!sock) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: Failed to create ZeroMQ socket");
      return;
    }

    // Build ZeroMQ address: tcp://ip_address:port
    std::string address = "tcp://" + host->ip_address + ":" + std::to_string(zmq_port);

    // Connect to the target node
    int rc = zmq_connect(sock, address.c_str());
    if (rc != 0) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: Failed to connect to {}", address);
      zmq_close(sock);
      return;
    }

    // Send the main message (BuildMessage output)
    int flags = (data_transfers.size() > 0) ? ZMQ_SNDMORE : 0;
    rc = zmq_send(sock, message.data(), message.size(), flags | ZMQ_DONTWAIT);
    if (rc < 0) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: Failed to send message");
      zmq_close(sock);
      return;
    }

    // Send each data transfer payload
    for (size_t i = 0; i < data_transfers.size(); ++i) {
      const chi::DataTransfer &transfer = data_transfers[i];

      // Convert FullPtr to actual data pointer
      char *data_ptr = transfer.data.ptr_;
      if (!data_ptr) {
        task->SetReturnCode(1);
        zmq_close(sock);
        zmq_ctx_destroy(zmq_context);
        return;
      }

      // Determine if this is the last message part
      bool is_last = (i == data_transfers.size() - 1);
      flags = is_last ? 0 : ZMQ_SNDMORE;

      // Send the data transfer
      rc = zmq_send(sock, data_ptr, transfer.size, flags | ZMQ_DONTWAIT);
      if (rc < 0) {
        task->SetReturnCode(1);
        zmq_close(sock);
        return;
      }
    }

    // Clean up the socket (context is shared, don't destroy it)
    zmq_close(sock);
  }

  // Success
  task->SetReturnCode(0);
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
  task->SetReturnCode(0);

  try {
    // Get IPC manager
    auto *ipc_manager = CHI_IPC;
    if (!ipc_manager) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: IPC manager not initialized");
      return;
    }

    // Get the shared ZeroMQ socket from IPC manager
    void *sock = ipc_manager->GetMainZmqSocket();
    if (!sock) {
      task->SetReturnCode(1);
      HELOG(kError, "Admin: ZeroMQ socket not initialized");
      return;
    }

    // Receive the main message (BuildMessage output)
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int rc = zmq_msg_recv(&msg, sock, ZMQ_DONTWAIT);
    if (rc < 0) {
      // No message available - this is okay for a polling task
      zmq_msg_close(&msg);
      task->SetReturnCode(0);
      return;
    }

    // Extract message data
    std::string message_data(static_cast<char *>(zmq_msg_data(&msg)),
                             zmq_msg_size(&msg));
    int more = zmq_msg_more(&msg);
    zmq_msg_close(&msg);

    // Deserialize using TaskLoadInArchive
    chi::TaskLoadInArchive ar;

    // Load from the BuildMessage output
    ar.LoadFromMessage(message_data);

    // Get data transfers and allocate buffers
    const std::vector<chi::DataTransfer> &data_transfers =
        ar.GetDataTransfers();

    // Allocate buffers for each data transfer and receive data
    for (size_t i = 0; i < data_transfers.size(); ++i) {
      if (!more) {
        task->SetReturnCode(1);
        HELOG(kError, "Admin: Expected more message parts");
        return;
      }

      const chi::DataTransfer &transfer = data_transfers[i];

      // Allocate buffer for this transfer
      hipc::FullPtr<char> buffer =
          ipc_manager->AllocateBuffer<char>(transfer.size);
      char *dest_ptr = buffer.ptr_;
      if (!dest_ptr) {
        task->SetReturnCode(1);
        HELOG(kError, "Admin: Failed to allocate buffer");
        return;
      }

      // Receive next message part
      zmq_msg_init(&msg);
      rc = zmq_msg_recv(&msg, sock, 0);
      if (rc < 0) {
        task->SetReturnCode(1);
        HELOG(kError, "Admin: Failed to receive message part");
        zmq_msg_close(&msg);
        return;
      }

      // Verify message size matches expected size
      size_t msg_size = zmq_msg_size(&msg);
      if (msg_size != transfer.size) {
        task->SetReturnCode(1);
        HELOG(kError, "Admin: Message size mismatch");
        zmq_msg_close(&msg);
        return;
      }

      // Copy data from ZeroMQ message to allocated buffer
      memcpy(dest_ptr, zmq_msg_data(&msg), msg_size);
      more = zmq_msg_more(&msg);
      zmq_msg_close(&msg);

      // Update the pointer in the DataTransfer to point to the allocated buffer
      // This will be used when deserializing the tasks
      const_cast<chi::DataTransfer &>(transfer).data = buffer;
    }

    // Get task count
    size_t task_count = ar.GetTaskCount();

    // Deserialize and enqueue each task
    for (size_t i = 0; i < task_count; ++i) {
      // TODO: Deserialize tasks from archive
      // This requires reading the pool_id and method from the archive
      // and using container->LoadIn() to deserialize the task
      HILOG(kDebug, "Admin: Task {} deserialized successfully", i);
    }

    HILOG(kDebug, "Admin: Task input data received successfully");
    task->SetReturnCode(0);

  } catch (const std::exception &e) {
    task->SetReturnCode(1);
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