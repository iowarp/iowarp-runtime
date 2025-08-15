#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_

#include "chimaera/types.h"

namespace chi {

// Forward declaration for WorkOrchestrator static method
class WorkOrchestrator;

// Forward declarations
class Worker;
class Lane;
class TaskQueue;
class WorkOrchestrator;

/**
 * Custom header for tracking lane state (stored per-lane)
 */
struct TaskQueueHeader {
  PoolId pool_id;
  WorkerId assigned_worker_id;
  u32 task_count;        // Number of tasks currently in the queue
  bool is_enqueued;      // Whether this queue is currently enqueued in worker
  
  TaskQueueHeader() : pool_id(0), assigned_worker_id(0), task_count(0), is_enqueued(false) {}
  TaskQueueHeader(PoolId pid, WorkerId wid = 0) 
    : pool_id(pid), assigned_worker_id(wid), task_count(0), is_enqueued(false) {}
};

/**
 * Simple wrapper around hipc::multi_mpsc_queue
 * 
 * This wrapper adds custom enqueue and dequeue functions while maintaining
 * compatibility with existing code that expects the multi_mpsc_queue interface.
 */
class TaskQueue {
public:
  // Type alias for individual lanes with per-lane headers
  using TaskLane = chi::ipc::multi_mpsc_queue<hipc::TypedPointer<Task>, TaskQueueHeader>::queue_t;
  /**
   * Constructor using CtxAllocator (preferred pattern)
   * @param alloc Context allocator containing memory context
   * @param num_lanes Number of lanes
   * @param num_prios Number of priorities
   * @param depth_per_lane Depth per lane
   */
  TaskQueue(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
            u32 num_lanes, u32 num_prios, u32 depth_per_lane);

  /**
   * Destructor
   */
  ~TaskQueue() = default;


  /**
   * Forward all other methods to underlying queue for compatibility
   */
  TaskLane& GetLane(u32 lane_id, u32 prio_id) { 
    return queue_.GetLane(lane_id, prio_id); 
  }
  const TaskLane& GetLane(u32 lane_id, u32 prio_id) const { 
    return queue_.GetLane(lane_id, prio_id); 
  }

  /**
   * Check if queue is null (for compatibility)
   */
  bool IsNull() const { return queue_.IsNull(); }

  /**
   * Get number of lanes in the queue
   */
  u32 GetNumLanes() const { return queue_.GetNumLanes(); }

  /**
   * Get number of priorities in the queue
   */
  u32 GetNumPriorities() const { return queue_.GetNumPriorities(); }



  /**
   * Static method to emplace a task into a specific lane
   * @param lane_ptr FullPtr to the lane (as returned by GetLane)
   * @param task_ptr TypedPointer to the task to emplace
   * @return true if emplace successful
   */
  static bool EmplaceTask(hipc::FullPtr<TaskLane>& lane_ptr, hipc::TypedPointer<Task> task_ptr);

  /**
   * Static method to pop a task from a specific lane
   * @param lane_ptr FullPtr to the lane (as returned by GetLane)
   * @param task_ptr Output TypedPointer to the popped task
   * @return true if pop successful
   */
  static bool PopTask(hipc::FullPtr<TaskLane>& lane_ptr, hipc::TypedPointer<Task>& task_ptr) {
    if (lane_ptr.IsNull()) {
      return false;
    }
    
    auto token = lane_ptr->pop(task_ptr);
    return !token.IsNull();
  }

private:
  chi::ipc::multi_mpsc_queue<hipc::TypedPointer<Task>, TaskQueueHeader> queue_; // Underlying queue

  /**
   * Notify worker that a lane should be processed
   * Called when task is added to empty lane
   * @param lane_id Lane identifier
   * @param prio_id Priority identifier
   */
  void NotifyWorkerLaneReady(u32 lane_id, u32 prio_id);
};

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_