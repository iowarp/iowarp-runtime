#ifndef CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_

#include "chimaera/chimod_spec.h"
#include "chimaera/types.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"
#include "chimaera/work_orchestrator.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>

/**
 * Container Base Class with Default Implementations
 * 
 * Provides default implementations of ChiContainer methods for simpler modules.
 * Modules can inherit from this class instead of ChiContainer to get basic
 * queue and lane management functionality out of the box.
 */

namespace chi {

/**
 * Container - Base class with default implementations
 * 
 * Provides a simpler base class than ChiContainer with default implementations
 * of queue and lane management. Modules can inherit from this class to get
 * basic functionality without having to implement all pure virtual methods.
 */
class Container : public ChiContainer {
 protected:
  // Local queue management using TaskQueue
  std::unordered_map<QueueId, hipc::FullPtr<TaskQueue>> local_queues_;
  DomainQuery dom_query_;
  std::string name_;
  
  // Default allocator for creating lanes
  CHI_MAIN_ALLOC_T* main_allocator_ = nullptr;
  
 public:
  Container() = default;
  virtual ~Container() {
    // Note: Lane mappings are managed by WorkOrchestrator lifecycle
    // No explicit cleanup needed since lanes are mapped, not registered
  }
  
  /**
   * Initialize container with pool information and domain query
   * This version includes DomainQuery for more complete initialization
   */
  virtual void Init(const PoolId& pool_id, const DomainQuery& dom_query) {
    pool_id_ = pool_id;
    dom_query_ = dom_query;
    pool_name_ = "pool_" + std::to_string(pool_id);
    
    // Get main allocator for creating lanes
    auto mem_manager = HSHM_MEMORY_MANAGER;
    main_allocator_ = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(hipc::AllocatorId(1, 0));
  }
  
  /**
   * Initialize container with pool information (ChiContainer interface)
   */
  void Init(const PoolId& pool_id, const std::string& pool_name) override {
    pool_id_ = pool_id;
    pool_name_ = pool_name;
    dom_query_ = DomainQuery();  // Default domain query
    
    // Get main allocator for creating lanes
    auto mem_manager = HSHM_MEMORY_MANAGER;
    main_allocator_ = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(hipc::AllocatorId(1, 0));
    
    // For testing: Create local queues during initialization to verify WorkOrchestrator integration
    std::cout << "Container: Initializing container for pool " << pool_id << " (" << pool_name << ")" << std::endl;
    CreateLocalQueue(kLowLatency, 4);   // 4 lanes for low latency tasks
    CreateLocalQueue(kHighLatency, 2);  // 2 lanes for heavy operations
    std::cout << "Container: Created local queues for pool " << pool_id << std::endl;
  }
  
  /**
   * Create a local queue with specified priority
   * Simplified version using QueuePriority enum with configurable lanes
   */
  virtual void CreateLocalQueue(QueuePriority priority, u32 num_lanes = 1) {
    CreateLocalQueue(static_cast<QueueId>(priority), num_lanes, 0);
  }
  
  
  /**
   * Create a local queue with specified lanes (ChiContainer interface)
   */
  void CreateLocalQueue(QueueId queue_id, u32 num_lanes, u32 flags) override {
    (void)flags; // Suppress unused parameter warning - flags not used in TaskQueue yet
    
    if (local_queues_.find(queue_id) != local_queues_.end()) {
      return; // Queue already exists
    }
    
    // Create TaskQueue with configurable lanes (assume only one priority)
    if (main_allocator_) {
      hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);
      
      // Create TaskQueue (headers are managed internally by TaskQueue)
      auto task_queue = main_allocator_->template NewObj<TaskQueue>(
        HSHM_MCTX, ctx_alloc, num_lanes, 1, 1024);
      
      if (!task_queue.IsNull()) {
        local_queues_[queue_id] = task_queue;
        
        // Schedule all lanes in the queue using round-robin scheduler
        auto* work_orchestrator = CHI_WORK_ORCHESTRATOR;
        if (work_orchestrator && work_orchestrator->IsInitialized()) {
          work_orchestrator->RoundRobinTaskQueueScheduler(task_queue.ptr_);
          std::cout << "Container: Scheduled lanes for queue " << queue_id 
                    << " with WorkOrchestrator for pool " << pool_id_ << std::endl;
        } else {
          std::cerr << "Container: WorkOrchestrator not available for lane scheduling" << std::endl;
        }
      }
    }
  }
  
  /**
   * Get TaskQueue by queue ID
   */
  TaskQueue* GetTaskQueue(QueueId queue_id) {
    auto it = local_queues_.find(queue_id);
    if (it != local_queues_.end() && !it->second.IsNull()) {
      return it->second.ptr_;
    }
    return nullptr;
  }
  
  /**
   * Get specific lane by ID (ChiContainer interface)
   */
  TaskQueue::TaskLane* GetLane(QueueId queue_id, LaneId lane_id) override {
    auto* task_queue = GetTaskQueue(queue_id);
    if (task_queue && lane_id < task_queue->GetNumLanes()) {
      return &task_queue->GetLane(lane_id, 0);  // priority 0 since only one priority
    }
    return nullptr;
  }
  
  /**
   * Get lane by hash for load balancing (ChiContainer interface)
   */
  TaskQueue::TaskLane* GetLaneByHash(QueueId queue_id, u32 hash) override {
    auto* task_queue = GetTaskQueue(queue_id);
    if (task_queue) {
      u32 num_lanes = task_queue->GetNumLanes();
      if (num_lanes > 0) {
        LaneId lane_id = hash % num_lanes;
        return &task_queue->GetLane(lane_id, 0);  // priority 0 since only one priority
      }
    }
    return nullptr;
  }
  
  /**
   * Get FullPtr to specific lane by ID for task emplacement
   */
  hipc::FullPtr<TaskQueue::TaskLane> GetLaneFullPtr(QueueId queue_id, LaneId lane_id) {
    auto* lane = GetLane(queue_id, lane_id);
    if (lane) {
      return hipc::FullPtr<TaskQueue::TaskLane>(lane);
    }
    return hipc::FullPtr<TaskQueue::TaskLane>();
  }
  
  /**
   * Get FullPtr to lane by hash for task emplacement
   */
  hipc::FullPtr<TaskQueue::TaskLane> GetLaneByHashFullPtr(QueueId queue_id, u32 hash) {
    auto* lane = GetLaneByHash(queue_id, hash);
    if (lane) {
      return hipc::FullPtr<TaskQueue::TaskLane>(lane);
    }
    return hipc::FullPtr<TaskQueue::TaskLane>();
  }
  
  /**
   * Default Run implementation - should be overridden by derived classes
   */
  void Run(u32 method, hipc::FullPtr<Task> task_ptr, RunContext& rctx) override {
    // Default: no-op, derived classes should override this
    (void)method;
    (void)task_ptr;
    (void)rctx;
  }
  
  /**
   * Default Monitor implementation - should be overridden by derived classes
   */
  void Monitor(MonitorModeId mode, u32 method, 
               hipc::FullPtr<Task> task_ptr,
               RunContext& rctx) override {
    // Default: no monitoring action
    (void)mode; (void)method; (void)task_ptr; (void)rctx; // Suppress unused warnings
  }
  
  /**
   * Default Del implementation - should be overridden by derived classes
   */
  void Del(u32 method, hipc::FullPtr<Task> task_ptr) override {
    // Default: no cleanup needed
    (void)method; (void)task_ptr; // Suppress unused warnings
  }
  
 protected:
  /**
   * Helper to get queue priority from queue ID
   */
  QueuePriority GetQueuePriority(QueueId queue_id) const {
    return static_cast<QueuePriority>(queue_id);
  }
  
  /**
   * Helper to check if a queue exists
   */
  bool HasQueue(QueueId queue_id) const {
    return local_queues_.find(queue_id) != local_queues_.end();
  }
  
  /**
   * Helper to get number of queues
   */
  size_t GetQueueCount() const {
    return local_queues_.size();
  }
};

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_