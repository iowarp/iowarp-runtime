/**
 * Work orchestrator implementation
 */

#include "chimaera/work_orchestrator.h"
#include "chimaera/singletons.h"

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool WorkOrchestrator::Init() {
  if (is_initialized_) {
    return true;
  }

  // Initialize HSHM thread group first
  auto thread_model = HSHM_THREAD_MODEL;
  thread_group_ = thread_model->CreateThreadGroup({});
  
  ConfigManager* config = CHI_CONFIG;

  // Create workers based on configuration
  if (!CreateWorkers(kLowLatencyWorker, config->GetWorkerThreadCount(kLowLatencyWorker))) {
    return false;
  }

  if (!CreateWorkers(kHighLatencyWorker, config->GetWorkerThreadCount(kHighLatencyWorker))) {
    return false;
  }

  if (!CreateWorkers(kReinforcementWorker, config->GetWorkerThreadCount(kReinforcementWorker))) {
    return false;
  }

  if (!CreateWorkers(kProcessReaper, config->GetWorkerThreadCount(kProcessReaper))) {
    return false;
  }

  // Initialize queue mappings
  if (!InitializeQueueMappings()) {
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

  // Clear lane mappings
  lane_to_worker_map_.clear();

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

  // Stop all workers
  for (auto* worker : all_workers_) {
    if (worker) {
      worker->Stop();
    }
  }

  // Wait for worker threads to finish using HSHM thread model
  auto thread_model = HSHM_THREAD_MODEL;
  for (auto& thread : worker_threads_) {
    thread_model->Join(thread);
  }

  workers_running_ = false;
}

Worker* WorkOrchestrator::GetWorker(u32 worker_id) const {
  if (!is_initialized_ || worker_id >= all_workers_.size()) {
    return nullptr;
  }

  return all_workers_[worker_id];
}

std::vector<Worker*> WorkOrchestrator::GetWorkersByType(ThreadType thread_type) const {
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
  ConfigManager* config = CHI_CONFIG;
  return config->GetWorkerThreadCount(thread_type);
}

bool WorkOrchestrator::MapLaneToWorker(u32 lane_id, u32 worker_id) {
  if (!is_initialized_ || worker_id >= all_workers_.size()) {
    return false;
  }

  if (lane_id >= lane_to_worker_map_.size()) {
    lane_to_worker_map_.resize(lane_id + 1, 0);
  }

  lane_to_worker_map_[lane_id] = worker_id;
  return true;
}

bool WorkOrchestrator::IsInitialized() const {
  return is_initialized_;
}

bool WorkOrchestrator::AreWorkersRunning() const {
  return workers_running_;
}

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
          thread_group_,
          [worker](int tid) {
            worker->Run();
          },
          static_cast<int>(i)
        );
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

bool WorkOrchestrator::InitializeQueueMappings() {
  // Stub implementation - would initialize lane to worker mappings
  // For now, just ensure the map is properly sized
  if (!all_workers_.empty()) {
    lane_to_worker_map_.resize(all_workers_.size(), 0);
  }

  return true;
}

}  // namespace chi