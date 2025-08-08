#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORK_ORCHESTRATOR_H_

#include <vector>
#include <memory>
#include "chimaera/types.h"
#include "chimaera/worker.h"

namespace chi {

/**
 * Work Orchestrator singleton for managing worker threads
 * 
 * Spawns configurable worker threads of different types using HSHM thread model,
 * maps queue lanes to workers, and coordinates task distribution.
 * Uses HSHM global cross pointer variable singleton pattern.
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