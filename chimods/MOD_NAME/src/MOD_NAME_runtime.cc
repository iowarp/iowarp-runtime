/**
 * Runtime implementation for MOD_NAME
 *
 * Contains the server-side task processing logic.
 */

#include "../include/MOD_NAME/MOD_NAME_runtime.h"

#include <iostream>

#include "../include/MOD_NAME/autogen/MOD_NAME_lib_exec.h"

namespace chimaera::MOD_NAME {

// Method implementations for Runtime class

void Runtime::InitClient(const chi::PoolId& pool_id) {
  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}

void Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
                  chi::RunContext& rctx) {
  // Dispatch to the appropriate method handler
  chimaera::MOD_NAME::Run(this, method, task_ptr, rctx);
}

void Runtime::Monitor(chi::MonitorModeId mode, chi::u32 method,
                      hipc::FullPtr<chi::Task> task_ptr,
                      chi::RunContext& rctx) {
  // Dispatch to the appropriate monitor handler
  chimaera::MOD_NAME::Monitor(this, mode, method, task_ptr, rctx);
}

void Runtime::Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  // Dispatch to the appropriate delete handler
  chimaera::MOD_NAME::Del(this, method, task_ptr);
}

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  std::cout << "MOD_NAME: Executing Create task for pool " << task->pool_id_
            << std::endl;

  // Initialize the container with pool information and domain query
  chi::Container::Init(task->pool_id_, task->pool_query_);

  // Create local queues for different priorities
  CreateLocalQueue(chi::kLowLatency, 4);   // 4 lanes for low latency tasks
  CreateLocalQueue(chi::kHighLatency, 2);  // 2 lanes for high latency tasks

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
      // Use base class lane management - set route_lane_ to low latency queue
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
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
      // Use base class lane management - set route_lane_ to low latency queue
      // lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
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
      // Use base class lane management - set route_lane_ to low latency queue lane 0
      {
        auto lane_ptr = GetLaneFullPtr(chi::kLowLatency, 0);
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
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
    case Method::kCustom: {
      auto typed_task = task_ptr.Cast<CustomTask>();
      typed_task->SerializeOut(archive);
      break;
    }
    default:
      // Unknown method - do nothing
      break;
  }
}

}  // namespace chimaera::MOD_NAME

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::MOD_NAME::Runtime)