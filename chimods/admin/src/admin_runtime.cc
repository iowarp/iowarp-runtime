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
#include <chimaera/worker.h>

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

  // Spawn periodic Recv task with 15 microsecond period
  // Worker will automatically reschedule periodic tasks
  hipc::MemContext mctx;
  client_.AsyncRecv(mctx, chi::PoolQuery::Local(), 0, 0.015);

  HILOG(kDebug,
        "Admin: Container created and initialized for pool: {} (ID: {}, count: "
        "{})",
        pool_name_, task->new_pool_id_, create_count_);
  HILOG(kDebug, "Admin: Spawned periodic Recv task with 15us period");
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
    rctx.est_load = 1000.0; // 1ms for admin create
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
    rctx.est_load = 50000.0; // 50ms for pool creation
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
    rctx.est_load = 30000.0; // 30ms for pool destruction
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
    rctx.est_load = 5000.0; // 5ms for shutdown initiation
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
      task->Yield(25);
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
    rctx.est_load = 1000.0; // 1ms for flush
    break;
  }
}

//===========================================================================
// Distributed Task Scheduling Method Implementations
//===========================================================================

/**
 * Helper function: Send task inputs to remote node
 * @param task SendTask containing subtask and pool queries
 * @param rctx RunContext for managing subtasks
 */
void Runtime::SendIn(hipc::FullPtr<SendTask> task, chi::RunContext &rctx) {
  auto *ipc_manager = CHI_IPC;
  auto *pool_manager = CHI_POOL_MANAGER;

  // Validate subtask
  hipc::FullPtr<chi::Task> subtask = task->subtask_;
  if (subtask.IsNull()) {
    task->SetReturnCode(1);
    return;
  }

  // Get the container associated with the subtask
  chi::Container *container = pool_manager->GetContainer(subtask->pool_id_);
  if (!container) {
    task->SetReturnCode(2);
    return;
  }

  HILOG(kDebug, "Admin: SendIn - adding to send_map");

  // Add the origin task to send_map for later lookup
  send_map_[subtask->task_id_] = subtask;

  // Send to each target in pool_queries
  for (size_t i = 0; i < task->pool_queries_.size(); ++i) {
    const chi::PoolQuery &query = task->pool_queries_[i];

    // Determine target node_id based on query type
    chi::u64 target_node_id = 0;

    if (query.IsLocalMode()) {
      // Local mode - target is the local node (for TASK_FORCE_NET testing)
      target_node_id = ipc_manager->GetNodeId();
    } else if (query.IsPhysicalMode()) {
      target_node_id = query.GetNodeId();
    } else if (query.IsDirectIdMode()) {
      chi::ContainerId container_id = query.GetContainerId();
      target_node_id =
          pool_manager->GetContainerNodeId(subtask->pool_id_, container_id);
    } else if (query.IsRangeMode()) {
      chi::u32 offset = query.GetRangeOffset();
      chi::ContainerId container_id(offset);
      target_node_id =
          pool_manager->GetContainerNodeId(subtask->pool_id_, container_id);
    } else {
      HELOG(kError, "Admin: Unsupported query type for SendIn");
      continue;
    }

    // Get host information for target node
    const chi::Host *target_host = ipc_manager->GetHost(target_node_id);
    if (!target_host) {
      HELOG(kError, "Admin: Host not found for node_id {}", target_node_id);
      continue;
    }

    HILOG(kDebug, "Admin: Sending task inputs to node {} ({})", target_node_id,
          target_host->ip_address);

    // Create Lightbeam client using configured port
    auto *config_manager = CHI_CONFIG_MANAGER;
    int port = static_cast<int>(config_manager->GetZmqPort());
    auto lbm_client = hshm::lbm::TransportFactory::GetClient(
        target_host->ip_address, hshm::lbm::Transport::kZeroMq, "tcp", port);

    // Create SaveTaskArchive with SerializeIn mode and lbm_client
    chi::SaveTaskArchive archive(true, lbm_client.get());

    // Create task copy
    hipc::FullPtr<chi::Task> task_copy;
    container->NewCopy(subtask->method_, subtask, task_copy, true);

    // Update the copy's pool query to current query
    task_copy->pool_query_ = query;

    // Set return node ID in the pool query
    chi::u64 this_node_id = ipc_manager->GetNodeId();
    task_copy->pool_query_.SetReturnNode(this_node_id);
    HILOG(kDebug, "Admin: Task copy return node set to {}", this_node_id);

    // Get or allocate the subtask's RunContext to store replicas
    chi::RunContext *subtask_rctx = subtask->run_ctx_;
    if (!subtask_rctx) {
      // Allocate RunContext for subtask if it doesn't have one
      subtask_rctx = new chi::RunContext();
      subtask_rctx->task = subtask;
      subtask->run_ctx_ = subtask_rctx;
      HILOG(kDebug, "Admin: Allocated RunContext for subtask");
    }

    // Add copy to subtasks vector in subtask's RunContext
    subtask_rctx->subtasks_.push_back(task_copy);

    // Update replica_id of task_id to be index in subtasks
    chi::TaskId copy_id = task_copy->task_id_;
    copy_id.replica_id_ = subtask_rctx->subtasks_.size() - 1;
    task_copy->task_id_ = copy_id;

    // Serialize the task using container->SaveTask (Expose will be called
    // automatically for bulks)
    container->SaveTask(task_copy->method_, archive, task_copy);

    // Send using Lightbeam
    int rc = lbm_client->Send(archive);
    if (rc != 0) {
      HELOG(kError, "Admin: Lightbeam Send failed with error code {}", rc);
      continue;
    }

    HILOG(kDebug, "Admin: Task inputs sent via Lightbeam (bulks: {})",
          archive.send.size());
  }

  task->SetReturnCode(0);
}

/**
 * Helper function: Send task outputs back to origin node
 * @param task SendTask containing subtask
 */
void Runtime::SendOut(hipc::FullPtr<SendTask> task) {
  auto *ipc_manager = CHI_IPC;
  auto *pool_manager = CHI_POOL_MANAGER;

  // Validate subtask
  hipc::FullPtr<chi::Task> subtask = task->subtask_;
  if (subtask.IsNull()) {
    task->SetReturnCode(1);
    return;
  }

  // Get the container associated with the subtask
  chi::Container *container = pool_manager->GetContainer(subtask->pool_id_);
  if (!container) {
    task->SetReturnCode(2);
    return;
  }

  HILOG(kDebug, "Admin: SendOut - removing from recv_map");

  // Remove task from recv_map as we're completing it
  auto it = recv_map_.find(subtask->task_id_);
  if (it == recv_map_.end()) {
    task->SetReturnCode(3); // Error: Task not found in recv_map
    return;
  }
  recv_map_.erase(it);

  // Send to each target in pool_queries
  for (size_t i = 0; i < task->pool_queries_.size(); ++i) {
    const chi::PoolQuery &query = task->pool_queries_[i];

    // Determine target node_id
    chi::u64 target_node_id = 0;

    if (query.IsPhysicalMode()) {
      target_node_id = query.GetNodeId();
    } else {
      HELOG(kError, "Admin: SendOut only supports Physical query mode");
      continue;
    }

    // Get host information
    const chi::Host *target_host = ipc_manager->GetHost(target_node_id);
    if (!target_host) {
      HELOG(kError, "Admin: Host not found for node_id {}", target_node_id);
      continue;
    }

    HILOG(kDebug, "Admin: Sending task outputs to node {} ({})", target_node_id,
          target_host->ip_address);

    // Create Lightbeam client using configured port
    auto *config_manager = CHI_CONFIG_MANAGER;
    int port = static_cast<int>(config_manager->GetZmqPort());
    auto lbm_client = hshm::lbm::TransportFactory::GetClient(
        target_host->ip_address, hshm::lbm::Transport::kZeroMq, "tcp", port);

    // Create SaveTaskArchive with SerializeOut mode and lbm_client
    // The client will automatically call Expose internally during serialization
    chi::SaveTaskArchive archive(false, lbm_client.get());

    // Serialize the task outputs using container->SaveTask (Expose called
    // automatically)
    container->SaveTask(subtask->method_, archive, subtask);

    int rc = lbm_client->Send(archive);
    if (rc != 0) {
      HELOG(kError, "Admin: Lightbeam Send failed with error code {}", rc);
      continue;
    }

    HILOG(kDebug, "Admin: Task outputs sent via Lightbeam (bulks: {})",
          archive.send.size());
  }

  // Delete the task after sending outputs
  ipc_manager->DelTask(subtask);
  HILOG(kDebug, "Admin: Task deleted after SendOut");

  task->SetReturnCode(0);
}

/**
 * Main Send function - dispatches to SendIn or SendOut
 */
void Runtime::Send(hipc::FullPtr<SendTask> task, chi::RunContext &rctx) {
  if (task->srl_mode_) {
    SendIn(task, rctx);
  } else {
    SendOut(task);
  }
}

void Runtime::MonitorSend(chi::MonitorModeId mode,
                          hipc::FullPtr<SendTask> task_ptr,
                          chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    break;
  case chi::MonitorModeId::kGlobalSchedule:
    break;
  case chi::MonitorModeId::kEstLoad:
    rctx.est_load = 10000.0; // 10ms estimate
    break;
  }
  (void)task_ptr; // Suppress unused warning
}

/**
 * Helper function: Receive task inputs from remote node
 * @param task RecvTask containing control information
 * @param archive Already-parsed LoadTaskArchive containing task info
 * @param lbm_server Lightbeam server for receiving bulk data
 */
void Runtime::RecvIn(hipc::FullPtr<RecvTask> task,
                     chi::LoadTaskArchive &archive,
                     hshm::lbm::Server *lbm_server) {
  auto *ipc_manager = CHI_IPC;
  auto *pool_manager = CHI_POOL_MANAGER;

  const auto &task_infos = archive.GetTaskInfos();
  HILOG(kDebug, "Admin: RecvIn - {} tasks", task_infos.size());

  // Allocate buffers for bulk data and expose them for receiving
  // archive.send contains sender's bulk descriptors (populated by RecvMetadata)
  for (const auto &send_bulk : archive.send) {
    hipc::FullPtr<char> buffer = ipc_manager->AllocateBuffer(send_bulk.size);
    archive.recv.push_back(
        lbm_server->Expose(buffer, send_bulk.size, send_bulk.flags.bits_));
  }

  // Receive all bulk data using Lightbeam
  int rc = lbm_server->RecvBulks(archive);
  if (rc != 0) {
    HELOG(kError, "Admin: Lightbeam RecvBulks failed with error code {}", rc);
    task->SetReturnCode(4);
    return;
  }

  HILOG(kDebug, "Admin: Received {} bulk transfers via Lightbeam",
        archive.recv.size());

  for (size_t task_idx = 0; task_idx < task_infos.size(); ++task_idx) {
    const auto &task_info = task_infos[task_idx];

    // Get container associated with PoolId
    chi::Container *container = pool_manager->GetContainer(task_info.pool_id_);
    if (!container) {
      HELOG(kError, "Admin: Container not found for pool_id {}",
            task_info.pool_id_);
      continue;
    }

    // Allocate task pointer (LoadTask will allocate it using NewTask)
    hipc::FullPtr<chi::Task> task_ptr = hipc::FullPtr<chi::Task>::GetNull();

    // Call LoadTask to allocate and deserialize the task
    container->LoadTask(task_info.method_id_, archive, task_ptr);

    if (task_ptr.IsNull()) {
      HELOG(kError, "Admin: Failed to load task");
      continue;
    }

    // Mark task as remote, set as data owner, unset periodic and TASK_FORCE_NET
    task_ptr->SetFlags(TASK_REMOTE | TASK_DATA_OWNER);
    task_ptr->ClearFlags(TASK_PERIODIC | TASK_FORCE_NET);
    HILOG(
        kDebug,
        "Admin: Task marked as remote and data owner, TASK_FORCE_NET cleared");

    // Add task to recv_map for later lookup
    recv_map_[task_ptr->task_id_] = task_ptr;

    // Enqueue task for execution
    ipc_manager->Enqueue(task_ptr);

    HILOG(kDebug, "Admin: Task enqueued for execution (task_id={}, pool_id={})",
          task_ptr->task_id_, task_ptr->pool_id_);
  }

  task->SetReturnCode(0);
}

/**
 * Helper function: Receive task outputs from remote node
 * @param task RecvTask containing control information
 * @param archive Already-parsed LoadTaskArchive containing task info
 * @param lbm_server Lightbeam server for receiving bulk data
 */
void Runtime::RecvOut(hipc::FullPtr<RecvTask> task,
                      chi::LoadTaskArchive &archive,
                      hshm::lbm::Server *lbm_server) {
  auto *pool_manager = CHI_POOL_MANAGER;

  const auto &task_infos = archive.GetTaskInfos();
  HILOG(kDebug, "Admin: RecvOut - {} tasks", task_infos.size());

  // Set lbm_server in archive for bulk transfer exposure in output mode
  archive.SetLbmServer(lbm_server);

  // First pass: Deserialize to expose buffers
  // LoadTask will call ar.bulk() which will expose the pointers and populate
  // archive.recv
  for (size_t task_idx = 0; task_idx < task_infos.size(); ++task_idx) {
    const auto &task_info = task_infos[task_idx];

    // Locate origin task from send_map
    auto send_it = send_map_.find(task_info.task_id_);
    if (send_it == send_map_.end()) {
      HELOG(kError, "Admin: Origin task not found in send_map");
      task->SetReturnCode(5);
      return;
    }

    hipc::FullPtr<chi::Task> origin_task = send_it->second;
    chi::RunContext *origin_rctx = origin_task->run_ctx_;
    if (!origin_rctx) {
      HELOG(kError, "Admin: Origin task has no RunContext");
      task->SetReturnCode(6);
      return;
    }

    // Locate replica in origin's run_ctx using replica_id
    chi::u32 replica_id = task_info.task_id_.replica_id_;
    if (replica_id >= origin_rctx->subtasks_.size()) {
      HELOG(kError, "Admin: Invalid replica_id {} (subtasks size: {})",
            replica_id, origin_rctx->subtasks_.size());
      task->SetReturnCode(7);
      return;
    }

    hipc::FullPtr<chi::Task> replica = origin_rctx->subtasks_[replica_id];

    // Get the container associated with the origin task
    chi::Container *container =
        pool_manager->GetContainer(origin_task->pool_id_);
    if (!container) {
      HELOG(kError, "Admin: Container not found for pool_id {}",
            origin_task->pool_id_);
      task->SetReturnCode(8);
      return;
    }

    // Call LoadTask to deserialize - this will expose buffers via ar.bulk()
    // and populate archive.recv
    container->LoadTask(replica->method_, archive, replica);
  }

  // Receive all bulk data using Lightbeam
  int rc = lbm_server->RecvBulks(archive);
  if (rc != 0) {
    HELOG(kError, "Admin: Lightbeam RecvBulks failed with error code {}", rc);
    task->SetReturnCode(4);
    return;
  }

  HILOG(kDebug, "Admin: Received {} bulk transfers via Lightbeam",
        archive.recv.size());

  // Second pass: Aggregate results
  for (size_t task_idx = 0; task_idx < task_infos.size(); ++task_idx) {
    const auto &task_info = task_infos[task_idx];

    // Locate origin task from send_map
    auto send_it = send_map_.find(task_info.task_id_);
    if (send_it == send_map_.end()) {
      HELOG(kError, "Admin: Origin task not found in send_map");
      continue;
    }

    hipc::FullPtr<chi::Task> origin_task = send_it->second;
    chi::RunContext *origin_rctx = origin_task->run_ctx_;
    if (!origin_rctx) {
      HELOG(kError, "Admin: Origin task has no RunContext");
      continue;
    }

    // Locate replica in origin's run_ctx using replica_id
    chi::u32 replica_id = task_info.task_id_.replica_id_;
    if (replica_id >= origin_rctx->subtasks_.size()) {
      HELOG(kError, "Admin: Invalid replica_id {} (subtasks size: {})",
            replica_id, origin_rctx->subtasks_.size());
      continue;
    }

    hipc::FullPtr<chi::Task> replica = origin_rctx->subtasks_[replica_id];

    // Get the container associated with the origin task
    chi::Container *container =
        pool_manager->GetContainer(origin_task->pool_id_);
    if (!container) {
      HELOG(kError, "Admin: Container not found for pool_id {}",
            origin_task->pool_id_);
      continue;
    }

    // Aggregate replica results into origin task
    container->Aggregate(origin_task->method_, origin_task, replica);

    // Increment completed replicas counter in origin's rctx
    chi::u32 completed = origin_rctx->completed_replicas_.fetch_add(1) + 1;

    HILOG(kDebug, "Admin: Replica {} completed ({}/{})", replica_id, completed,
          origin_rctx->subtasks_.size());

    // If all replicas completed
    if (completed == origin_rctx->subtasks_.size()) {
      // Get pool manager to access container
      auto *pool_manager = CHI_POOL_MANAGER;
      chi::Container *container =
          pool_manager->GetContainer(origin_task->pool_id_);

      // Unmark TASK_DATA_OWNER before deleting replicas to avoid freeing the
      // same data pointers twice Delete all subtask replicas using
      // container->Del() to avoid memory leak
      if (container) {
        for (const auto &subtask_ptr : origin_rctx->subtasks_) {
          subtask_ptr->ClearFlags(TASK_DATA_OWNER);
          container->Del(subtask_ptr->method_, subtask_ptr);
        }
      }

      // Clear subtasks vector after deleting tasks
      origin_rctx->subtasks_.clear();

      // Remove origin from send_map
      send_map_.erase(send_it);

      // Handle task completion based on whether it's periodic
      if (origin_task->IsPeriodic()) {
        // Periodic task - add back to blocked queue for next iteration
        auto *worker =
            HSHM_THREAD_MODEL->GetTls<chi::Worker>(chi::chi_cur_worker_key_);
        worker->AddToBlockedQueue(origin_rctx);
        HILOG(kDebug, "Admin: Periodic origin task added to blocked queue");
      } else {
        // Non-periodic task - free RunContext and mark as complete
        delete origin_rctx;
        origin_task->run_ctx_ = nullptr;
        origin_task->is_complete_.store(1);
        HILOG(kDebug, "Admin: Non-periodic origin task marked complete");
      }
    }
  }

  task->SetReturnCode(0);
}

/**
 * Main Recv function - receives metadata and dispatches based on mode
 */
void Runtime::Recv(hipc::FullPtr<RecvTask> task, chi::RunContext &rctx) {
  // Get the main server from CHI_IPC (already bound during initialization)
  auto *ipc_manager = CHI_IPC;
  hshm::lbm::Server *lbm_server = ipc_manager->GetMainServer();
  if (!lbm_server) {
    HELOG(kError, "Admin: Main server not available");
    task->SetReturnCode(1);
    return;
  }

  // Receive metadata first to determine mode (non-blocking)
  chi::LoadTaskArchive archive;
  int rc = lbm_server->RecvMetadata(archive);
  if (rc == EAGAIN) {
    // No message available - this is normal for polling
    task->SetReturnCode(0);
    return;
  }
  if (rc != 0) {
    // Error receiving metadata
    HELOG(kError, "Admin: Lightbeam RecvMetadata failed with error code {}",
          rc);
    task->SetReturnCode(2);
    return;
  }

  HILOG(kDebug, "Admin: Received metadata (mode: {})",
        archive.GetSerializeMode() ? "SerializeIn" : "SerializeOut");

  // Dispatch based on serialization mode
  if (archive.GetSerializeMode()) {
    RecvIn(task, archive, lbm_server);
  } else {
    RecvOut(task, archive, lbm_server);
  }

  (void)rctx;
}

void Runtime::MonitorRecv(chi::MonitorModeId mode,
                          hipc::FullPtr<RecvTask> task_ptr,
                          chi::RunContext &rctx) {
  switch (mode) {
  case chi::MonitorModeId::kLocalSchedule:
    break;
  case chi::MonitorModeId::kGlobalSchedule:
    break;
  case chi::MonitorModeId::kEstLoad:
    rctx.est_load = 10000.0; // 10ms estimate
    break;
  }
  (void)task_ptr; // Suppress unused warning
}

chi::u64 Runtime::GetWorkRemaining() const {
  return send_map_.size() + recv_map_.size();
}

//===========================================================================
// Task Serialization Method Implementations
//===========================================================================

// Task Serialization Method Implementations now in autogen/admin_lib_exec.cc

} // namespace chimaera::admin

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::admin::Runtime)