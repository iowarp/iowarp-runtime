/**
 * TaskQueue implementation - simple wrapper around hipc::multi_mpsc_queue
 */

#include "chimaera/task_queue.h"

#include "chimaera/singletons.h"
#include "chimaera/work_orchestrator.h"

namespace chi {

TaskQueue::TaskQueue(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
                     u32 num_lanes, u32 num_prios, u32 depth_per_lane)
    : queue_(alloc, num_lanes, num_prios, depth_per_lane) {
  // Headers are now managed automatically by the multi_mpsc_queue per lane
}

void TaskQueue::NotifyWorkerLaneReady(u32 lane_id, u32 prio_id) {
  // Get the specific lane that has work available
  auto& lane = queue_.GetLane(lane_id, prio_id);
  hipc::FullPtr<TaskQueue::TaskLane> lane_ptr(&lane);

  // Call static function to notify worker via IPC Manager
  WorkOrchestrator::NotifyWorkerLaneReady(lane_ptr);
}

/*static*/ bool TaskQueue::EmplaceTask(hipc::FullPtr<TaskLane>& lane_ptr, hipc::TypedPointer<Task> task_ptr) {
  if (lane_ptr.IsNull() || task_ptr.IsNull()) {
    return false;
  }
  
  // Check if lane was empty before enqueue for notification
  bool was_empty = (lane_ptr->size() == 0);
  
  // Push to the lane
  lane_ptr->push(task_ptr);
  
  // Notify worker if lane was empty - WorkOrchestrator will use the worker ID from the lane header
  if (was_empty) {
    WorkOrchestrator::NotifyWorkerLaneReady(lane_ptr);
  }
  
  return true;
}

}  // namespace chi