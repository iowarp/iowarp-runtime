#ifndef CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_
#define CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_

#include "chimaera/types.h"

namespace chi {

// Forward declarations
class Worker;
class Lane;

/**
 * Lane reference structure for enqueueing specific lanes to workers
 */
struct TaskQueueLaneRef {
  hipc::Pointer task_queue_ptr;  // Pointer to the TaskQueue
  u32 queue_id;                  // Queue ID within the TaskQueue
  u32 lane_id;                   // Lane ID within the queue
  
  TaskQueueLaneRef() : queue_id(0), lane_id(0) {}
  TaskQueueLaneRef(hipc::Pointer queue_ptr, u32 qid, u32 lid) 
    : task_queue_ptr(queue_ptr), queue_id(qid), lane_id(lid) {}
};

/**
 * Custom header for tracking lane state
 */
struct TaskQueueHeader {
  PoolId pool_id;
  WorkerId assigned_worker_id;
  u32 task_count;        // Number of tasks currently in the lane
  bool is_enqueued;      // Whether this lane is currently enqueued in worker
  
  TaskQueueHeader() : pool_id(0), assigned_worker_id(0), task_count(0), is_enqueued(false) {}
  TaskQueueHeader(PoolId pid, WorkerId wid = 0) : pool_id(pid), assigned_worker_id(wid), task_count(0), is_enqueued(false) {}
};

/**
 * Simple wrapper around hipc::multi_mpsc_queue
 * 
 * This wrapper adds custom enqueue and dequeue functions while maintaining
 * compatibility with existing code that expects the multi_mpsc_queue interface.
 */
class TaskQueue {
public:
  /**
   * Constructor matching hipc allocator calling pattern
   * @param alloc Allocator 
   * @param num_queues Number of queues
   * @param num_lanes_per_queue Number of lanes per queue
   * @param depth_per_lane Depth per lane
   */
  TaskQueue(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
            u32 num_queues, u32 num_lanes_per_queue, u32 depth_per_lane);

  /**
   * Constructor with explicit memory context (alternative)
   * @param mctx Memory context
   * @param alloc Allocator 
   * @param num_queues Number of queues
   * @param num_lanes_per_queue Number of lanes per queue
   * @param depth_per_lane Depth per lane
   */
  TaskQueue(const hipc::MemContext& mctx, 
            const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
            u32 num_queues, u32 num_lanes_per_queue, u32 depth_per_lane);

  /**
   * Destructor
   */
  ~TaskQueue() = default;

  /**
   * Custom enqueue function
   * Automatically handles lane re-enqueuing to workers when lane becomes non-empty
   * @param task_ptr Pointer to task to enqueue
   * @param queue_id Queue ID (default 0)
   * @param lane_id Lane ID 
   * @return true if enqueue successful
   */
  bool Enqueue(hipc::Pointer task_ptr, u32 queue_id = 0, u32 lane_id = 0);

  /**
   * Custom dequeue function
   * @param task_ptr Output pointer to dequeued task
   * @param queue_id Queue ID (default 0)
   * @param lane_id Lane ID
   * @return true if task dequeued, false if empty
   */
  bool Dequeue(hipc::Pointer& task_ptr, u32 queue_id = 0, u32 lane_id = 0);

  /**
   * Forward all other methods to underlying queue for compatibility
   */
  auto& GetLane(u32 queue_id, u32 lane_id) { return queue_.GetLane(queue_id, lane_id); }
  const auto& GetLane(u32 queue_id, u32 lane_id) const { return queue_.GetLane(queue_id, lane_id); }

  /**
   * Check if queue is null (for compatibility)
   */
  bool IsNull() const { return queue_.IsNull(); }

private:
  hipc::multi_mpsc_queue<hipc::Pointer> queue_;  // Underlying queue
  TaskQueueHeader header_;                       // Metadata

  /**
   * Notify worker that a lane should be processed
   * Called when task is added to empty lane
   * @param queue_id Queue identifier
   * @param lane_id Lane identifier
   */
  void NotifyWorkerLaneReady(u32 queue_id, u32 lane_id);
};

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_TASK_QUEUE_H_