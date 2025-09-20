/**
 * Runtime implementation for MOD_NAME
 *
 * Contains the server-side task processing logic.
 */

#include "../include/chimaera/MOD_NAME/MOD_NAME_runtime.h"

#include <chrono>


namespace chimaera::MOD_NAME {

// Method implementations for Runtime class

void Runtime::InitClient(const chi::PoolId& pool_id) {
  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}

// Virtual method implementations now in autogen/MOD_NAME_lib_exec.cc

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing Create task for pool {}", task->pool_id_);

  // Initialize the container with pool information and domain query
  chi::Container::Init(task->pool_id_, task->pool_query_);

  // Create local queues with explicit queue IDs and priorities
  CreateLocalQueue(0, 4, chi::kLowLatency);   // Queue 0: 4 lanes for low latency tasks
  CreateLocalQueue(1, 2, chi::kHighLatency);  // Queue 1: 2 lanes for high latency tasks

  create_count_++;

  HILOG(kDebug, "MOD_NAME: Container created and initialized for pool: {} (ID: {}, count: {})", pool_name_, task->pool_id_, create_count_);
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      HILOG(kDebug, "MOD_NAME: Setting route_lane_ for Create task");
      // Use base class lane management - set route_lane_ to queue 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      HILOG(kDebug, "MOD_NAME: Global scheduling for Create task");
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      HILOG(kDebug, "MOD_NAME: Estimating load for Create task");
      break;
  }
}

void Runtime::Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing Custom task with data: {}", task->data_.c_str());

  custom_count_++;

  // Process custom task here
  // In a real implementation, this would perform the custom operation

  HILOG(kDebug, "MOD_NAME: Custom completed (count: {})", custom_count_);
}

void Runtime::MonitorCustom(chi::MonitorModeId mode,
                            hipc::FullPtr<CustomTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      HILOG(kDebug, "MOD_NAME: Setting route_lane_ for Custom task");
      // Use base class lane management - set route_lane_ to queue 0
      // lane 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      HILOG(kDebug, "MOD_NAME: Global scheduling for Custom task");
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      HILOG(kDebug, "MOD_NAME: Estimating load for Custom task");
      break;
  }
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing Destroy task - Pool ID: {}", task->target_pool_id_);

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  // In a real implementation, this would clean up MOD_NAME-specific resources
  // For now, just mark as successful
  HILOG(kDebug, "MOD_NAME: Container destroyed successfully");
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                            hipc::FullPtr<DestroyTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      HILOG(kDebug, "MOD_NAME: Setting route_lane_ for Destroy task");
      // Use base class lane management - set route_lane_ to queue 0 lane 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global destruction
      HILOG(kDebug, "MOD_NAME: Global scheduling for Destroy task");
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      rctx.estimated_completion_time_us = 10000.0;  // 10ms for destruction
      break;
  }
}

chi::u64 Runtime::GetWorkRemaining() const {
  // Template container implementation returns 0 (no work tracking)
  return 0;
}

//===========================================================================
// Task Serialization Method Implementations now in autogen/MOD_NAME_lib_exec.cc
//===========================================================================

void Runtime::CoMutexTest(hipc::FullPtr<CoMutexTestTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing CoMutexTest task {} (hold: {}ms)", task->test_id_, task->hold_duration_ms_);

  // Use actual CoMutex synchronization primitive
  chi::ScopedCoMutex lock(test_comutex_);

  // Hold the mutex for the specified duration
  if (task->hold_duration_ms_ > 0) {
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
      auto now = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      if (duration >= task->hold_duration_ms_) {
        break;
      }
    }
  }

  task->return_code_ = 0;  // Success (0 means success in most conventions)
  HILOG(kDebug, "MOD_NAME: CoMutexTest {} completed", task->test_id_);
}

void Runtime::MonitorCoMutexTest(chi::MonitorModeId mode,
                                hipc::FullPtr<CoMutexTestTask> task_ptr,
                                chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;
    default:
      break;
  }
}

void Runtime::CoRwLockTest(hipc::FullPtr<CoRwLockTestTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing CoRwLockTest task {} ({}, hold: {}ms)", task->test_id_, (task->is_writer_ ? "writer" : "reader"), task->hold_duration_ms_);

  // Use actual CoRwLock synchronization primitive with appropriate lock type
  if (task->is_writer_) {
    chi::ScopedCoRwWriteLock lock(test_corwlock_);
    
    // Hold the write lock for the specified duration
    if (task->hold_duration_ms_ > 0) {
      auto start = std::chrono::high_resolution_clock::now();
      while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (duration >= task->hold_duration_ms_) {
          break;
        }
      }
    }
  } else {
    chi::ScopedCoRwReadLock lock(test_corwlock_);
    
    // Hold the read lock for the specified duration
    if (task->hold_duration_ms_ > 0) {
      auto start = std::chrono::high_resolution_clock::now();
      while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (duration >= task->hold_duration_ms_) {
          break;
        }
      }
    }
  }

  task->return_code_ = 0;  // Success (0 means success in most conventions)
  HILOG(kDebug, "MOD_NAME: CoRwLockTest {} completed", task->test_id_);
}

void Runtime::MonitorCoRwLockTest(chi::MonitorModeId mode,
                                 hipc::FullPtr<CoRwLockTestTask> task_ptr,
                                 chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;
    default:
      break;
  }
}

void Runtime::FireAndForgetTest(hipc::FullPtr<FireAndForgetTestTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing FireAndForgetTest task {} (processing: {}ms, message: '{}')", task->test_id_, task->processing_time_ms_, task->log_message_.c_str());

  // Simulate processing time
  if (task->processing_time_ms_ > 0) {
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
      auto now = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      if (duration >= task->processing_time_ms_) {
        break;
      }
    }
  }

  HILOG(kDebug, "MOD_NAME: FireAndForgetTest {} completed and will be auto-deleted", task->test_id_);
}

void Runtime::MonitorFireAndForgetTest(chi::MonitorModeId mode,
                                      hipc::FullPtr<FireAndForgetTestTask> task_ptr,
                                      chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;
    default:
      break;
  }
}

void Runtime::WaitTest(hipc::FullPtr<WaitTestTask> task, chi::RunContext& rctx) {
  HILOG(kDebug, "MOD_NAME: Executing WaitTest task {} (depth: {}, current_depth: {})", task->test_id_, task->depth_, task->current_depth_);

  // Increment current depth
  task->current_depth_++;

  // If we haven't reached the target depth, create a subtask and wait for it
  if (task->current_depth_ < task->depth_) {
    HILOG(kDebug, "MOD_NAME: WaitTest {} creating recursive subtask at depth {}", task->test_id_, task->current_depth_);
    
    // Use the client API for recursive calls - this tests the Wait() functionality properly
    // Create a subtask with remaining depth
    hipc::MemContext mctx;
    chi::u32 remaining_depth = task->depth_ - task->current_depth_;
    chi::u32 subtask_final_depth = client_.WaitTest(mctx, task->pool_query_, 
                                                   remaining_depth, task->test_id_);
    
    // The subtask returns the final depth it reached, so we set our depth to that
    task->current_depth_ = task->depth_;
    
    HILOG(kDebug, "MOD_NAME: WaitTest {} subtask completed via client API, final depth: {}", task->test_id_, task->current_depth_);
  }

  HILOG(kDebug, "MOD_NAME: WaitTest {} completed at depth {}", task->test_id_, task->current_depth_);
}

void Runtime::MonitorWaitTest(chi::MonitorModeId mode,
                             hipc::FullPtr<WaitTestTask> task_ptr,
                             chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = lane_ptr.ptr_;
        }
      }
      break;
    case chi::MonitorModeId::kEstLoad:
      // Estimate completion time based on depth
      rctx.estimated_completion_time_us = task_ptr->depth_ * 1000.0;  // 1ms per depth level
      break;
    default:
      break;
  }
}

// Static member definitions
chi::CoMutex Runtime::test_comutex_;
chi::CoRwLock Runtime::test_corwlock_;

}  // namespace chimaera::MOD_NAME

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::MOD_NAME::Runtime)