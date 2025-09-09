/**
 * Runtime implementation for MOD_NAME
 *
 * Contains the server-side task processing logic.
 */

#include "../include/MOD_NAME/MOD_NAME_runtime.h"

#include <iostream>
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
  std::cout << "MOD_NAME: Executing Create task for pool " << task->pool_id_
            << std::endl;

  // Initialize the container with pool information and domain query
  chi::Container::Init(task->pool_id_, task->pool_query_);

  // Create local queues with explicit queue IDs and priorities
  CreateLocalQueue(0, 4, chi::kLowLatency);   // Queue 0: 4 lanes for low latency tasks
  CreateLocalQueue(1, 2, chi::kHighLatency);  // Queue 1: 2 lanes for high latency tasks

  create_count_++;

  std::cout << "MOD_NAME: Container created and initialized for pool: "
            << pool_name_ << " (ID: " << task->pool_id_
            << ", count: " << create_count_ << ")" << std::endl;
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "MOD_NAME: Setting route_lane_ for Create task" << std::endl;
      // Use base class lane management - set route_lane_ to queue 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "MOD_NAME: Global scheduling for Create task" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      std::cout << "MOD_NAME: Estimating load for Create task" << std::endl;
      break;
  }
}

void Runtime::Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& rctx) {
  std::cout << "MOD_NAME: Executing Custom task with data: "
            << task->data_.c_str() << std::endl;

  custom_count_++;

  // Process custom task here
  // In a real implementation, this would perform the custom operation

  std::cout << "MOD_NAME: Custom completed (count: " << custom_count_ << ")"
            << std::endl;
}

void Runtime::MonitorCustom(chi::MonitorModeId mode,
                            hipc::FullPtr<CustomTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "MOD_NAME: Setting route_lane_ for Custom task" << std::endl;
      // Use base class lane management - set route_lane_ to queue 0
      // lane 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "MOD_NAME: Global scheduling for Custom task" << std::endl;
      break;

    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      std::cout << "MOD_NAME: Estimating load for Custom task" << std::endl;
      break;
  }
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  std::cout << "MOD_NAME: Executing Destroy task - Pool ID: "
            << task->target_pool_id_ << std::endl;

  // Initialize output values
  task->result_code_ = 0;
  task->error_message_ = "";

  // In a real implementation, this would clean up MOD_NAME-specific resources
  // For now, just mark as successful
  std::cout << "MOD_NAME: Container destroyed successfully" << std::endl;
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                            hipc::FullPtr<DestroyTask> task_ptr,
                            chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Set route_lane_ to indicate where task should be routed
      std::cout << "MOD_NAME: Setting route_lane_ for Destroy task" << std::endl;
      // Use base class lane management - set route_lane_ to queue 0 lane 0
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;

    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global destruction
      std::cout << "MOD_NAME: Global scheduling for Destroy task" << std::endl;
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
  std::cout << "MOD_NAME: Executing CoMutexTest task " << task->test_id_
            << " (hold: " << task->hold_duration_ms_ << "ms)" << std::endl;

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

  task->result_ = 1;  // Success
  std::cout << "MOD_NAME: CoMutexTest " << task->test_id_ << " completed" << std::endl;
}

void Runtime::MonitorCoMutexTest(chi::MonitorModeId mode,
                                hipc::FullPtr<CoMutexTestTask> task_ptr,
                                chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;
    default:
      break;
  }
}

void Runtime::CoRwLockTest(hipc::FullPtr<CoRwLockTestTask> task, chi::RunContext& rctx) {
  std::cout << "MOD_NAME: Executing CoRwLockTest task " << task->test_id_
            << " (" << (task->is_writer_ ? "writer" : "reader")
            << ", hold: " << task->hold_duration_ms_ << "ms)" << std::endl;

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

  task->result_ = 1;  // Success
  std::cout << "MOD_NAME: CoRwLockTest " << task->test_id_ << " completed" << std::endl;
}

void Runtime::MonitorCoRwLockTest(chi::MonitorModeId mode,
                                 hipc::FullPtr<CoRwLockTestTask> task_ptr,
                                 chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      {
        auto lane_ptr = GetLaneFullPtr(0, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
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