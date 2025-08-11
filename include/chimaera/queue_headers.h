#ifndef CHIMAERA_INCLUDE_CHIMAERA_QUEUE_HEADERS_H_
#define CHIMAERA_INCLUDE_CHIMAERA_QUEUE_HEADERS_H_

#include "chimaera/types.h"
#include <atomic>
#include <cstdint>
#include <ctime>

namespace chi {

/**
 * Header for lane-specific metadata in multi-MPSC queues
 * Stores information about worker assignments and lane statistics
 */
struct LaneHeader {
  // Worker assignment
  std::atomic<WorkerId> assigned_worker_id{0};
  std::atomic<bool> is_active{false};
  
  // Lane statistics
  std::atomic<uint64_t> total_enqueued{0};
  std::atomic<uint64_t> total_dequeued{0};
  std::atomic<uint64_t> current_size{0};
  
  // Metadata
  LaneId lane_id;
  QueuePriority priority;
  uint64_t creation_timestamp;
  
  LaneHeader() : lane_id(0), priority(kLowLatency), creation_timestamp(std::time(nullptr)) {}
  
  LaneHeader(LaneId lid, QueuePriority prio = kLowLatency) 
    : lane_id(lid), priority(prio), creation_timestamp(std::time(nullptr)) {}
  
  // Worker assignment methods
  void SetAssignedWorker(WorkerId worker_id) {
    assigned_worker_id.store(worker_id);
  }
  
  WorkerId GetAssignedWorker() const {
    return assigned_worker_id.load();
  }
  
  bool HasAssignedWorker() const {
    return assigned_worker_id.load() != 0;
  }
  
  void ClearAssignedWorker() {
    assigned_worker_id.store(0);
  }
  
  // Activity state
  void SetActive(bool active) {
    is_active.store(active);
  }
  
  bool IsActive() const {
    return is_active.load();
  }
  
  // Statistics tracking
  void OnEnqueue() {
    total_enqueued.fetch_add(1);
    current_size.fetch_add(1);
  }
  
  void OnDequeue() {
    total_dequeued.fetch_add(1);
    current_size.fetch_sub(1);
  }
  
  uint64_t GetPendingCount() const {
    return current_size.load();
  }
  
  double GetThroughput() const {
    uint64_t total = total_enqueued.load();
    if (total == 0) return 0.0;
    uint64_t now = std::time(nullptr);
    uint64_t elapsed = now - creation_timestamp;
    return elapsed > 0 ? static_cast<double>(total) / elapsed : 0.0;
  }
};

/**
 * Header for container-level queue metadata
 * Stores information about the container and its queue configuration
 */
struct ContainerQueueHeader {
  // Container identification
  PoolId pool_id;
  u32 queue_id;
  std::atomic<uint32_t> num_active_lanes{0};
  
  // Queue configuration
  uint32_t total_lanes;
  uint32_t depth_per_lane;
  QueuePriority base_priority;
  
  // Container metadata
  uint64_t creation_timestamp;
  char container_name[64];
  
  // Global statistics
  std::atomic<uint64_t> total_tasks_processed{0};
  std::atomic<uint64_t> total_enqueue_operations{0};
  std::atomic<uint64_t> total_dequeue_operations{0};
  
  ContainerQueueHeader() 
    : pool_id(0), queue_id(0), total_lanes(1), depth_per_lane(1024), 
      base_priority(kLowLatency), creation_timestamp(std::time(nullptr)) {
    container_name[0] = '\0';
  }
  
  ContainerQueueHeader(PoolId pid, u32 qid, uint32_t lanes, uint32_t depth, 
                       QueuePriority priority = kLowLatency, const char* name = nullptr)
    : pool_id(pid), queue_id(qid), total_lanes(lanes), depth_per_lane(depth),
      base_priority(priority), creation_timestamp(std::time(nullptr)) {
    if (name) {
      strncpy(container_name, name, sizeof(container_name) - 1);
      container_name[sizeof(container_name) - 1] = '\0';
    } else {
      snprintf(container_name, sizeof(container_name), "container_%u_%u", pid, qid);
    }
  }
  
  // Lane management
  void IncrementActiveLanes() {
    num_active_lanes.fetch_add(1);
  }
  
  void DecrementActiveLanes() {
    uint32_t current = num_active_lanes.load();
    if (current > 0) {
      num_active_lanes.fetch_sub(1);
    }
  }
  
  uint32_t GetActiveLaneCount() const {
    return num_active_lanes.load();
  }
  
  // Statistics tracking
  void OnTaskProcessed() {
    total_tasks_processed.fetch_add(1);
  }
  
  void OnEnqueueOperation() {
    total_enqueue_operations.fetch_add(1);
  }
  
  void OnDequeueOperation() {
    total_dequeue_operations.fetch_add(1);
  }
  
  double GetProcessingRate() const {
    uint64_t total = total_tasks_processed.load();
    if (total == 0) return 0.0;
    uint64_t now = std::time(nullptr);
    uint64_t elapsed = now - creation_timestamp;
    return elapsed > 0 ? static_cast<double>(total) / elapsed : 0.0;
  }
};

/**
 * Header for IPC manager process queues
 * Stores information about the runtime's global task processing
 */
struct ProcessQueueHeader {
  // Process identification
  uint32_t process_id;
  uint32_t num_worker_threads;
  
  // Queue configuration
  uint32_t num_lanes;
  uint32_t num_priorities;
  uint32_t depth_per_queue;
  
  // Runtime metadata
  uint64_t creation_timestamp;
  std::atomic<uint64_t> last_activity_timestamp;
  char process_name[64];
  
  // Global statistics across all lanes and priorities
  std::atomic<uint64_t> total_tasks_submitted{0};
  std::atomic<uint64_t> total_tasks_completed{0};
  std::atomic<uint64_t> total_worker_assignments{0};
  
  // Per-priority statistics (support up to 8 priority levels)
  std::atomic<uint64_t> priority_counts[8] = {0};
  
  ProcessQueueHeader() 
    : process_id(0), num_worker_threads(1), num_lanes(1), num_priorities(1), 
      depth_per_queue(1024), creation_timestamp(std::time(nullptr)),
      last_activity_timestamp(std::time(nullptr)) {
    process_name[0] = '\0';
  }
  
  ProcessQueueHeader(uint32_t pid, uint32_t workers, uint32_t lanes, 
                     uint32_t priorities, uint32_t depth, const char* name = nullptr)
    : process_id(pid), num_worker_threads(workers), num_lanes(lanes),
      num_priorities(priorities), depth_per_queue(depth),
      creation_timestamp(std::time(nullptr)), last_activity_timestamp(std::time(nullptr)) {
    if (name) {
      strncpy(process_name, name, sizeof(process_name) - 1);
      process_name[sizeof(process_name) - 1] = '\0';
    } else {
      snprintf(process_name, sizeof(process_name), "process_%u", pid);
    }
  }
  
  // Activity tracking
  void UpdateLastActivity() {
    last_activity_timestamp.store(std::time(nullptr));
  }
  
  uint64_t GetTimeSinceLastActivity() const {
    uint64_t now = std::time(nullptr);
    return now - last_activity_timestamp.load();
  }
  
  // Task tracking
  void OnTaskSubmitted(QueuePriority priority = kLowLatency) {
    total_tasks_submitted.fetch_add(1);
    int priority_idx = static_cast<int>(priority);
    if (priority_idx >= 0 && priority_idx < 8) {
      priority_counts[priority_idx].fetch_add(1);
    }
    UpdateLastActivity();
  }
  
  void OnTaskCompleted() {
    total_tasks_completed.fetch_add(1);
    UpdateLastActivity();
  }
  
  void OnWorkerAssignment() {
    total_worker_assignments.fetch_add(1);
  }
  
  // Statistics
  uint64_t GetPendingTaskCount() const {
    return total_tasks_submitted.load() - total_tasks_completed.load();
  }
  
  double GetTaskCompletionRate() const {
    uint64_t total = total_tasks_completed.load();
    if (total == 0) return 0.0;
    uint64_t elapsed = std::time(nullptr) - creation_timestamp;
    return elapsed > 0 ? static_cast<double>(total) / elapsed : 0.0;
  }
  
  uint64_t GetPriorityCount(QueuePriority priority) const {
    int idx = static_cast<int>(priority);
    if (idx >= 0 && idx < 8) {
      return priority_counts[idx].load();
    }
    return 0;
  }
};

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_QUEUE_HEADERS_H_