#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_

#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include "chimaera/types.h"
#include "chimaera/worker.h"

namespace chi {

// Forward declarations
class Worker;

/**
 * LaneQueue represents a single processing lane within a multi-MPSC queue
 * Each lane can be independently scheduled and assigned to workers
 */
class LaneQueue {
public:
  LaneQueue(LaneId lane_id, hipc::multi_mpsc_queue<hipc::Pointer>* parent_queue, u32 lane_index);
  ~LaneQueue() = default;

  // Task operations
  void Enqueue(hipc::Pointer task_ptr);
  bool Dequeue(hipc::Pointer& task_ptr);
  bool IsEmpty() const;
  size_t Size() const;

  // Worker assignment
  void SetAssignedWorker(WorkerId worker_id);
  WorkerId GetAssignedWorker() const;
  bool HasAssignedWorker() const;
  void ClearAssignedWorker();

  // Lane properties
  LaneId GetLaneId() const { return lane_id_; }
  u32 GetLaneIndex() const { return lane_index_; }
  bool IsActive() const { return is_active_.load(); }
  void SetActive(bool active) { is_active_.store(active); }

private:
  LaneId lane_id_;
  u32 lane_index_;
  hipc::multi_mpsc_queue<hipc::Pointer>* parent_queue_;
  std::atomic<WorkerId> assigned_worker_;
  std::atomic<bool> is_active_;
};

/**
 * Work Orchestrator singleton for managing worker threads and lane scheduling
 * 
 * Spawns configurable worker threads of different types using HSHM thread model,
 * maps queue lanes to workers using round-robin scheduling, and coordinates task distribution.
 * Uses hipc::multi_mpsc_queue for both container queues and process queues.
 */
class WorkOrchestrator {
 public:

  /**
   * Initialize work orchestrator
   * @return true if initialization successful, false otherwise
   */
  bool Init();

  /**
   * Finalize and cleanup orchestrator resources
   */
  void Finalize();

  /**
   * Start all worker threads
   * @return true if all workers started successfully, false otherwise
   */
  bool StartWorkers();

  /**
   * Stop all worker threads
   */
  void StopWorkers();

  /**
   * Get worker by ID
   * @param worker_id Worker identifier
   * @return Pointer to worker or nullptr if not found
   */
  Worker* GetWorker(u32 worker_id) const;

  /**
   * Get workers by thread type
   * @param thread_type Type of worker threads
   * @return Vector of worker pointers of specified type
   */
  std::vector<Worker*> GetWorkersByType(ThreadType thread_type) const;

  /**
   * Get total number of workers
   * @return Total count of all workers
   */
  size_t GetWorkerCount() const;

  /**
   * Get worker count by type
   * @param thread_type Type of worker threads
   * @return Count of workers of specified type
   */
  u32 GetWorkerCountByType(ThreadType thread_type) const;

  /**
   * Schedule lanes for worker assignment
   * Exposes lanes for scheduling and implements round-robin assignment
   * @param lanes Vector of lane IDs to schedule
   */
  void ScheduleLanes(const std::vector<LaneId>& lanes);

  /**
   * Schedule a single lane for worker assignment
   * @param lane_id Lane identifier to schedule
   * @return true if scheduling successful, false otherwise
   */
  bool ScheduleLane(LaneId lane_id);

  /**
   * Create local queue with multiple independent lanes (for containers)
   * @param priority Queue priority level
   * @param num_lanes Number of independent lanes to create
   * @return Vector of lane IDs created
   */
  std::vector<LaneId> CreateLocalQueue(QueuePriority priority, u32 num_lanes);

  /**
   * Initialize process queues with multiple lanes (for runtime)
   * @param num_lanes Number of lanes per priority level
   * @return true if initialization successful, false otherwise
   */
  bool ServerInitQueues(u32 num_lanes);

  /**
   * Get lane by ID
   * @param lane_id Lane identifier
   * @return Pointer to lane or nullptr if not found
   */
  LaneQueue* GetLane(LaneId lane_id);

  /**
   * Get all active lanes assigned to a specific worker
   * @param worker_id Worker identifier
   * @return Vector of lane IDs assigned to the worker
   */
  std::vector<LaneId> GetActiveLanesForWorker(WorkerId worker_id);

  /**
   * Get next available worker using round-robin scheduling
   * @return Worker ID of next available worker
   */
  WorkerId GetNextAvailableWorker();

  /**
   * Map queue lane to worker
   * @param lane_id Queue lane identifier
   * @param worker_id Worker identifier
   * @return true if mapping successful, false otherwise
   */
  bool MapLaneToWorker(u32 lane_id, u32 worker_id);

  /**
   * Check if orchestrator is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

  /**
   * Check if workers are running
   * @return true if workers are active, false otherwise
   */
  bool AreWorkersRunning() const;

  /**
   * Notify that a lane has work available and should be enqueued to its assigned worker
   * @param queue_id Queue identifier
   * @param lane_id Lane identifier
   */
  void NotifyWorkerLaneReady(u32 queue_id, u32 lane_id);

  /**
   * Notify that a lane has work available and should be enqueued to its assigned worker
   * @param queue_id Queue identifier
   * @param lane_id Lane identifier  
   * @param lane_ref_ptr hipc::Pointer to TaskQueueLaneRef in shared memory
   */
  void NotifyWorkerLaneReady(u32 queue_id, u32 lane_id, hipc::Pointer lane_ref_ptr);

 private:
  /**
   * Spawn worker threads using HSHM thread model
   * @return true if spawning successful, false otherwise
   */
  bool SpawnWorkerThreads();

  /**
   * Create workers of specified type
   * @param thread_type Type of workers to create
   * @param count Number of workers to create
   * @return true if creation successful, false otherwise
   */
  bool CreateWorkers(ThreadType thread_type, u32 count);

  /**
   * Initialize queue lane mappings
   * @return true if mapping successful, false otherwise
   */
  bool InitializeQueueMappings();

  bool is_initialized_ = false;
  bool workers_running_ = false;

  // Worker containers organized by type
  std::vector<std::unique_ptr<Worker>> low_latency_workers_;
  std::vector<std::unique_ptr<Worker>> high_latency_workers_;
  std::vector<std::unique_ptr<Worker>> reinforcement_workers_;
  std::vector<std::unique_ptr<Worker>> process_reaper_workers_;

  // All workers for easy access
  std::vector<Worker*> all_workers_;

  // Lane management
  std::unordered_map<LaneId, std::unique_ptr<LaneQueue>> lanes_;
  std::atomic<LaneId> next_lane_id_;
  std::vector<LaneId> active_lanes_;
  mutable std::shared_mutex lanes_mutex_;

  // Multi-MPSC queues for different priorities  
  std::unordered_map<QueuePriority, std::unique_ptr<hipc::multi_mpsc_queue<hipc::Pointer>>> priority_queues_;

  // Round-robin scheduling state
  std::atomic<u32> next_worker_index_for_scheduling_;

  // Queue lane to worker mappings
  std::vector<u32> lane_to_worker_map_;

  // HSHM threads (will be filled during initialization)
  std::vector<hshm::thread::Thread> worker_threads_;
  hshm::thread::ThreadGroup thread_group_;
};

}  // namespace chi

// Macro for accessing the Work Orchestrator singleton using HSHM singleton
#define CHI_WORK_ORCHESTRATOR hshm::Singleton<WorkOrchestrator>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_