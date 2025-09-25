/**
 * Runtime implementation for Simple Mod ChiMod
 *
 * Minimal ChiMod for testing external development patterns.
 * Contains the server-side task processing logic with basic functionality.
 */

#include "chimaera/simple_mod/simple_mod_runtime.h"

#include <chimaera/chimaera_manager.h>
#include <chimaera/module_manager.h>
#include <chimaera/pool_manager.h>
#include <chimaera/task_archives.h>

#include <iostream>

namespace external_test::simple_mod {

// Method implementations for Runtime class

void Runtime::InitClient(const chi::PoolId& pool_id) {
  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}

// Virtual method implementations now in autogen/simple_mod_lib_exec.cc

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  // Simple mod container creation logic
  std::cout << "SimpleMod: Initializing simple_mod container" << std::endl;

  // Initialize the Simple Mod container with pool information from the task
  // Create a single local queue for simple mod operations (high-latency priority, 1 lane)
  CreateLocalQueue(kMetadataQueue, 1, chi::kHighLatency);  // Metadata operations

  create_count_++;

  // Set success result
  task->return_code_ = 0;
  task->error_message_ = "";

  std::cout << "SimpleMod: Container created and initialized for pool: "
            << pool_name_ << " (ID: " << task->pool_id_
            << ", count: " << create_count_ << ")" << std::endl;
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "SimpleMod: Setting route_lane_ for simple_mod Create task"
                << std::endl;
      // Set route_lane_ to metadata queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "SimpleMod: Global scheduling for simple_mod Create task"
                << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - simple mod creation is fast
      rctx.estimated_completion_time_us = 500.0;  // 0.5ms for simple mod create
      break;
  }
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  std::cout << "SimpleMod: Executing Destroy task - Pool ID: "
            << task->target_pool_id_ << std::endl;

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  try {
    // Simple destruction logic - just log that we're destroyed
    std::cout << "SimpleMod: Container destroyed successfully - ID: "
              << task->target_pool_id_ << std::endl;

    // Set success result
    task->return_code_ = 0;

  } catch (const std::exception& e) {
    task->return_code_ = 99;
    auto alloc = task->GetCtxAllocator();
    task->error_message_ = hipc::string(
        alloc, std::string("Exception during simple_mod destruction: ") + e.what());
    std::cerr << "SimpleMod: Destruction failed with exception: " << e.what()
              << std::endl;
  }
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                             hipc::FullPtr<DestroyTask> task_ptr,
                             chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to metadata queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - destruction is fast
      rctx.estimated_completion_time_us = 100.0;  // 0.1ms for destruction
      break;
  }
}

void Runtime::Flush(hipc::FullPtr<FlushTask> task, chi::RunContext& rctx) {
  std::cout << "SimpleMod: Executing Flush task" << std::endl;

  // Simple flush implementation - just report no work remaining
  task->return_code_ = 0;
  task->total_work_done_ = GetWorkRemaining();

  std::cout << "SimpleMod: Flush completed - work done: " 
            << task->total_work_done_ << std::endl;
}

void Runtime::MonitorFlush(chi::MonitorModeId mode,
                           hipc::FullPtr<FlushTask> task_ptr,
                           chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to metadata queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time - flush is fast
      rctx.estimated_completion_time_us = 50.0;  // 0.05ms for flush
      break;
  }
}

chi::u64 Runtime::GetWorkRemaining() const {
  // Simple mod typically has no pending work, returns 0
  return 0;
}

// Note: Task serialization methods (SaveIn, LoadIn, SaveOut, LoadOut, NewCopy) 
// are automatically generated in autogen/simple_mod_lib_exec.cc and should not 
// be manually implemented here.

}  // namespace external_test::simple_mod