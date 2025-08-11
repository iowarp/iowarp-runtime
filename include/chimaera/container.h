#ifndef CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CONTAINER_H_

#include "chimaera/chimod_spec.h"
#include "chimaera/types.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"
#include <unordered_map>
#include <vector>
#include <memory>

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
  std::unordered_map<QueueId, TaskQueueHeader> queue_headers_;
  DomainQuery dom_query_;
  std::string name_;
  
  // Default allocator for creating lanes
  CHI_MAIN_ALLOC_T* main_allocator_ = nullptr;
  
 public:
  Container() = default;
  virtual ~Container() = default;
  
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
  }
  
  /**
   * Create a local queue with specified priority
   * Simplified version using QueuePriority enum with configurable lanes and priorities
   */
  virtual void CreateLocalQueue(QueuePriority priority, u32 num_lanes = 1, u32 num_priorities = 1) {
    CreateLocalQueue(static_cast<QueueId>(priority), num_lanes, num_priorities, 0);
  }
  
  /**
   * Create a local queue with specified lanes (ChiContainer interface compatibility)
   */
  void CreateLocalQueue(QueueId queue_id, u32 num_lanes, u32 flags) override {
    CreateLocalQueue(queue_id, num_lanes, 1, flags); // Default to 1 priority
  }
  
  /**
   * Create a local queue with specified lanes and priorities
   */
  void CreateLocalQueue(QueueId queue_id, u32 num_lanes, u32 num_priorities, u32 flags) {
    (void)flags; // Suppress unused parameter warning - flags not used in TaskQueue yet
    
    if (local_queues_.find(queue_id) != local_queues_.end()) {
      return; // Queue already exists
    }
    
    // Create TaskQueue with configurable lanes and priorities
    if (main_allocator_) {
      hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);
      
      // Create TaskQueue
      auto task_queue = main_allocator_->template NewObj<TaskQueue>(
        HSHM_MCTX, ctx_alloc, num_lanes, num_priorities, 1024);
      
      // Store header separately
      TaskQueueHeader header(pool_id_, 0); // pool_id, no worker assigned initially
      queue_headers_[queue_id] = header;
      
      if (!task_queue.IsNull()) {
        local_queues_[queue_id] = task_queue;
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
   * Note: Returns nullptr since TaskQueue doesn't expose Lane objects directly
   * Use GetTaskQueue() and TaskQueue methods instead
   */
  Lane* GetLane(QueueId queue_id, LaneId lane_id) override {
    // TaskQueue doesn't expose individual Lane objects
    // Containers should use GetTaskQueue() and TaskQueue methods directly
    (void)queue_id; // Suppress unused parameter warning
    (void)lane_id;  // Suppress unused parameter warning
    return nullptr;
  }
  
  /**
   * Get lane by hash for load balancing (ChiContainer interface)
   * Note: Returns nullptr since TaskQueue doesn't expose Lane objects directly
   * Use GetTaskQueue() and TaskQueue methods instead
   */
  Lane* GetLaneByHash(QueueId queue_id, u32 hash) override {
    // TaskQueue doesn't expose individual Lane objects
    // Containers should use GetTaskQueue() and TaskQueue methods directly
    (void)queue_id; // Suppress unused parameter warning
    (void)hash;     // Suppress unused parameter warning
    return nullptr;
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