/**
 * Runtime implementation for MOD_NAME
 * 
 * Contains the server-side task processing logic.
 */

#include "../include/MOD_NAME/MOD_NAME_runtime.h"
#include "../include/MOD_NAME/autogen/MOD_NAME_lib_exec.h"
#include <iostream>

namespace chimaera::MOD_NAME {

// Method implementations for Runtime class

void Runtime::Init(const chi::PoolId& pool_id, const std::string& pool_name) {
  chi::ChiContainer::Init(pool_id, pool_name);
  std::cout << "MOD_NAME Runtime initialized for pool: " << pool_name 
            << " (ID: " << pool_id << ")" << std::endl;
}

void Runtime::CreateLocalQueue(chi::QueueId queue_id, chi::u32 num_lanes, chi::u32 flags) {
  // Simplified implementation - would create actual lanes in full implementation
  std::cout << "Creating local queue " << queue_id << " with " << num_lanes 
            << " lanes" << std::endl;
}

chi::Lane* Runtime::GetLane(chi::QueueId queue_id, chi::LaneId lane_id) {
  // Return default lane for now
  return default_lane_.get();
}

chi::Lane* Runtime::GetLaneByHash(chi::QueueId queue_id, chi::u32 hash) {
  // Simple implementation - just return default lane
  return default_lane_.get();
}

void Runtime::Run(chi::u32 method, chi::Task* task, chi::RunContext& rctx) {
  // Convert Task* to FullPtr for dispatch to autogen functions
  hipc::FullPtr<chi::Task> task_ptr;
  task_ptr.ptr_ = task;
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
  std::cout << "MOD_NAME: Executing Create task for pool " 
            << task->pool_id_ << std::endl;
  
  create_count_++;
  
  // Perform initialization logic here
  // In a real implementation, this would set up container resources
  
  std::cout << "MOD_NAME: Create completed (count: " << create_count_ << ")" 
            << std::endl;
}

void Runtime::MonitorCreate(chi::MonitorModeId mode, 
                           hipc::FullPtr<CreateTask> task_ptr,
                           chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // Route task to local queue
      std::cout << "MOD_NAME: Routing Create task to local queue" << std::endl;
      if (default_lane_) {
        default_lane_->Enqueue(task_ptr.shm_);
      }
      break;
      
    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "MOD_NAME: Global scheduling for Create task" << std::endl;
      break;
      
    case chi::MonitorModeId::kCleanup:
      // Clean up completed task
      std::cout << "MOD_NAME: Cleaning up Create task" << std::endl;
      break;
  }
}

void Runtime::DelCreate(hipc::FullPtr<CreateTask> task_ptr) {
  // Clean up task-specific resources
  std::cout << "MOD_NAME: Deleting Create task" << std::endl;
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
      // Route task to local queue
      std::cout << "MOD_NAME: Routing Custom task to local queue" << std::endl;
      if (default_lane_) {
        default_lane_->Enqueue(task_ptr.shm_);
      }
      break;
      
    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      std::cout << "MOD_NAME: Global scheduling for Custom task" << std::endl;
      break;
      
    case chi::MonitorModeId::kCleanup:
      // Clean up completed task
      std::cout << "MOD_NAME: Cleaning up Custom task" << std::endl;
      break;
  }
}

void Runtime::DelCustom(hipc::FullPtr<CustomTask> task_ptr) {
  // Clean up task-specific resources
  std::cout << "MOD_NAME: Deleting Custom task" << std::endl;
}

} // namespace chimaera::MOD_NAME

// Define ChiMod entry points
CHI_CHIMOD_CC(chimaera::MOD_NAME::Runtime, "MOD_NAME")