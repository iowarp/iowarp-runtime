#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_

#include <atomic>
#include <memory>
#include <vector>

#include "chimaera/task_queue.h"
#include "chimaera/types.h"
#include "chimaera/worker.h"

namespace chi {

// Forward declarations
class Worker;


/**
 * Work Orchestrator singleton for managing worker threads and lane scheduling
 *
 * Spawns configurable worker threads of different types using HSHM thread
 * model, maps queue lanes to workers using round-robin scheduling, and
 * coordinates task distribution. Uses hipc::multi_mpsc_queue for both container
 * queues and process queues.
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
   * Initialize process queues with multiple lanes (for runtime)
   * @param num_lanes Number of lanes per priority level
   * @return true if initialization successful, false otherwise
   */
  bool ServerInitQueues(u32 num_lanes);

  /**
   * Get next available worker using round-robin scheduling
   * @return Worker ID of next available worker
   */
  WorkerId GetNextAvailableWorker();

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
   * Static function to notify that a lane has work available and should be
   * enqueued to its assigned worker Uses IPC Manager to enqueue the lane to the
   * appropriate worker's shared memory queue Works the same way on clients and
   * in the runtime
   * @param lane_ptr FullPtr to the TaskLane that has work available
   */
  static void NotifyWorkerLaneReady(
      hipc::FullPtr<TaskQueue::TaskLane> lane_ptr);

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

  // Active lanes pointer to IPC Manager worker queues
  void* active_lanes_;

  // Round-robin scheduling state
  std::atomic<u32> next_worker_index_for_scheduling_;

  // HSHM threads (will be filled during initialization)
  std::vector<hshm::thread::Thread> worker_threads_;
  hshm::thread::ThreadGroup thread_group_;
};

}  // namespace chi

// Macro for accessing the Work Orchestrator singleton using HSHM singleton
#define CHI_WORK_ORCHESTRATOR hshm::Singleton<WorkOrchestrator>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_