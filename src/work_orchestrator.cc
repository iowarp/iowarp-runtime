/**
 * Work orchestrator implementation
 */

#include "chimaera/work_orchestrator.h"

#include <chrono>
#include <iostream>

#include "chimaera/singletons.h"

// Global pointer variable definition for Work Orchestrator singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::WorkOrchestrator, g_work_orchestrator);

namespace chi {

//===========================================================================
// Work Orchestrator Implementation
//===========================================================================

// Constructor and destructor removed - handled by HSHM singleton pattern

bool WorkOrchestrator::Init() {
  if (is_initialized_) {
    return true;
  }

  // Initialize HSHM TLS key for workers
  HSHM_THREAD_MODEL->CreateTls<class Worker>(chi_cur_worker_key_, nullptr);

  // Initialize scheduling state
  next_worker_index_for_scheduling_.store(0);
  active_lanes_ = nullptr;

  // Initialize HSHM thread group first
  auto thread_model = HSHM_THREAD_MODEL;
  thread_group_ = thread_model->CreateThreadGroup({});

  ConfigManager* config = CHI_CONFIG_MANAGER;
  if (!config) {
    return false;  // Configuration manager not initialized
  }

  // Initialize worker queues in shared memory first
  IpcManager* ipc = CHI_IPC;
  if (!ipc) {
    return false;  // IPC manager not initialized
  }

  // Calculate total worker count from configuration
  u32 total_workers = config->GetWorkerThreadCount(kLowLatencyWorker) +
                      config->GetWorkerThreadCount(kHighLatencyWorker) +
                      config->GetWorkerThreadCount(kReinforcementWorker) +
                      config->GetWorkerThreadCount(kProcessReaper);

  if (!ipc->InitializeWorkerQueues(total_workers)) {
    return false;
  }

  // Create workers based on configuration
  if (!CreateWorkers(kLowLatencyWorker,
                     config->GetWorkerThreadCount(kLowLatencyWorker))) {
    return false;
  }

  if (!CreateWorkers(kHighLatencyWorker,
                     config->GetWorkerThreadCount(kHighLatencyWorker))) {
    return false;
  }

  if (!CreateWorkers(kReinforcementWorker,
                     config->GetWorkerThreadCount(kReinforcementWorker))) {
    return false;
  }

  if (!CreateWorkers(kProcessReaper,
                     config->GetWorkerThreadCount(kProcessReaper))) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

void WorkOrchestrator::Finalize() {
  if (!is_initialized_) {
    return;
  }

  // Stop workers if running
  if (workers_running_) {
    StopWorkers();
  }

  // Cleanup worker threads using HSHM thread model
  auto thread_model = HSHM_THREAD_MODEL;
  for (auto& thread : worker_threads_) {
    thread_model->Join(thread);
  }
  worker_threads_.clear();

  // Clear worker containers
  all_workers_.clear();
  low_latency_workers_.clear();
  high_latency_workers_.clear();
  reinforcement_workers_.clear();
  process_reaper_workers_.clear();

  is_initialized_ = false;
}

bool WorkOrchestrator::StartWorkers() {
  if (!is_initialized_ || workers_running_) {
    return false;
  }

  // Spawn worker threads using HSHM thread model
  if (!SpawnWorkerThreads()) {
    return false;
  }

  workers_running_ = true;
  return true;
}

void WorkOrchestrator::StopWorkers() {
  if (!workers_running_) {
    return;
  }

  std::cout << "Stopping " << all_workers_.size() << " worker threads..."
            << std::endl;

  // Stop all workers
  for (auto* worker : all_workers_) {
    if (worker) {
      worker->Stop();
    }
  }

  // Wait for worker threads to finish using HSHM thread model with timeout
  auto thread_model = HSHM_THREAD_MODEL;
  auto start_time = std::chrono::steady_clock::now();
  const auto timeout_duration = std::chrono::seconds(5);  // 5 second timeout

  size_t joined_count = 0;
  for (auto& thread : worker_threads_) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > timeout_duration) {
      std::cerr << "Warning: Worker thread join timeout reached. Some threads "
                   "may not have stopped gracefully."
                << std::endl;
      break;
    }

    thread_model->Join(thread);
    joined_count++;
  }

  std::cout << "Joined " << joined_count << " of " << worker_threads_.size()
            << " worker threads" << std::endl;
  workers_running_ = false;
}

Worker* WorkOrchestrator::GetWorker(u32 worker_id) const {
  if (!is_initialized_ || worker_id >= all_workers_.size()) {
    return nullptr;
  }

  return all_workers_[worker_id];
}

std::vector<Worker*> WorkOrchestrator::GetWorkersByType(
    ThreadType thread_type) const {
  std::vector<Worker*> workers;
  if (!is_initialized_) {
    return workers;
  }

  for (auto* worker : all_workers_) {
    if (worker && worker->GetThreadType() == thread_type) {
      workers.push_back(worker);
    }
  }

  return workers;
}

size_t WorkOrchestrator::GetWorkerCount() const {
  return is_initialized_ ? all_workers_.size() : 0;
}

u32 WorkOrchestrator::GetWorkerCountByType(ThreadType thread_type) const {
  ConfigManager* config = CHI_CONFIG_MANAGER;
  return config->GetWorkerThreadCount(thread_type);
}

bool WorkOrchestrator::IsInitialized() const { return is_initialized_; }

bool WorkOrchestrator::AreWorkersRunning() const { return workers_running_; }

bool WorkOrchestrator::SpawnWorkerThreads() {
  // Use HSHM thread model to spawn worker threads
  auto thread_model = HSHM_THREAD_MODEL;
  worker_threads_.reserve(all_workers_.size());

  try {
    for (size_t i = 0; i < all_workers_.size(); ++i) {
      auto* worker = all_workers_[i];
      if (worker) {
        // Spawn thread using HSHM thread model
        hshm::thread::Thread thread = thread_model->Spawn(
            thread_group_, [worker](int tid) { worker->Run(); },
            static_cast<int>(i));
        worker_threads_.emplace_back(std::move(thread));
      }
    }
    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

bool WorkOrchestrator::CreateWorkers(ThreadType thread_type, u32 count) {
  for (u32 i = 0; i < count; ++i) {
    u32 worker_id = static_cast<u32>(all_workers_.size());
    auto worker = std::make_unique<Worker>(worker_id, thread_type);

    if (!worker->Init()) {
      return false;
    }

    Worker* worker_ptr = worker.get();
    all_workers_.push_back(worker_ptr);

    // Add to type-specific container
    switch (thread_type) {
      case kLowLatencyWorker:
        low_latency_workers_.push_back(std::move(worker));
        break;
      case kHighLatencyWorker:
        high_latency_workers_.push_back(std::move(worker));
        break;
      case kReinforcementWorker:
        reinforcement_workers_.push_back(std::move(worker));
        break;
      case kProcessReaper:
        process_reaper_workers_.push_back(std::move(worker));
        break;
    }
  }

  return true;
}

//===========================================================================
// Lane Scheduling Methods
//===========================================================================

bool WorkOrchestrator::ServerInitQueues(u32 num_lanes) {
  // Initialize process queues for different priorities
  bool success = true;

  // No longer creating local queues - external queue is managed by IPC Manager
  return success;
}

WorkerId WorkOrchestrator::GetNextAvailableWorker() {
  if (all_workers_.empty()) {
    return 0;  // No workers available
  }

  // Round-robin scheduling
  u32 worker_index =
      next_worker_index_for_scheduling_.fetch_add(1) % all_workers_.size();
  Worker* worker = all_workers_[worker_index];

  return worker ? worker->GetId() : 0;
}

void WorkOrchestrator::MapLaneToWorker(TaskQueue::TaskLane* lane,
                                       WorkerId worker_id) {
  if (!lane) {
    return;
  }

  // TODO: Implement lane-to-worker mapping if needed
  // For now, just log the mapping
  std::cout << "WorkOrchestrator: Mapped lane to worker " << worker_id
            << std::endl;
}

void WorkOrchestrator::RoundRobinTaskQueueScheduler(TaskQueue* task_queue) {
  if (!is_initialized_ || !task_queue) {
    return;
  }

  // Get the number of lanes and priorities from the TaskQueue
  u32 num_lanes = task_queue->GetNumLanes();
  u32 num_priorities = task_queue->GetNumPriorities();

  std::cout << "WorkOrchestrator: Scheduling TaskQueue with " << num_lanes
            << " lanes and " << num_priorities << " priorities" << std::endl;

  // Iterate through all lanes and assign workers using round-robin
  for (u32 lane_id = 0; lane_id < num_lanes; ++lane_id) {
    for (u32 prio_id = 0; prio_id < num_priorities; ++prio_id) {
      // Get the specific lane from the TaskQueue
      auto& lane = task_queue->GetLane(lane_id, prio_id);

      // Get next worker using round-robin scheduling
      WorkerId worker_id = GetNextAvailableWorker();
      if (worker_id == 0) {
        std::cerr
            << "WorkOrchestrator: No workers available for lane scheduling"
            << std::endl;
        continue;
      }

      // Map this lane to the selected worker
      MapLaneToWorker(&lane, worker_id);
    }
  }
}

/*static*/ void WorkOrchestrator::NotifyWorkerLaneReady(
    hipc::FullPtr<TaskQueue::TaskLane> lane_ptr) {
  if (lane_ptr.IsNull()) {
    return;
  }

  // Get IPC Manager to access worker queues in shared memory
  IpcManager* ipc = CHI_IPC;
  if (!ipc) {
    return;
  }

  // Get worker ID from lane header
  // Access the lane's header directly - TaskQueueHeader is the header type
  auto& header = lane_ptr->GetHeader();
  u32 worker_id = header.assigned_worker_id;

  // Get the worker's active queue from shared memory
  auto worker_queue = ipc->GetWorkerQueue(worker_id);
  if (!worker_queue.IsNull()) {
    // Enqueue the lane to the worker's queue
    worker_queue->push(lane_ptr);
  }
}

}  // namespace chi