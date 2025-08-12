/**
 * Work orchestrator implementation
 */

#include "chimaera/work_orchestrator.h"
#include "chimaera/singletons.h"

namespace chi {

//===========================================================================
// Work Orchestrator Implementation
//===========================================================================

// Constructor and destructor removed - handled by HSHM singleton pattern

bool WorkOrchestrator::Init() {
  if (is_initialized_) {
    return true;
  }

  // Initialize scheduling state  
  next_worker_index_for_scheduling_.store(0);
  active_lanes_ = nullptr;

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


  // Initialize worker queues in shared memory
  IpcManager* ipc = CHI_IPC;
  if (ipc && !ipc->InitializeWorkerQueues(static_cast<u32>(all_workers_.size()))) {
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


//===========================================================================
// Lane Scheduling Methods
//===========================================================================

void WorkOrchestrator::ScheduleLanes(const std::vector<LaneId>& lanes) {
  // Get IPC Manager to access external queue
  IpcManager* ipc = CHI_IPC;
  if (!ipc) {
    return;
  }

  TaskQueue* external_queue = ipc->GetTaskQueue();
  if (!external_queue) {
    return;
  }

  // Schedule each lane in the external queue to workers
  // Iterate through all lanes and priorities in the external queue
  for (u32 lane_id = 0; lane_id < 4; ++lane_id) { // 4 lanes as configured
    for (u32 prio_id = 0; prio_id < 2; ++prio_id) { // 2 priorities (low/high latency)
      auto& lane = external_queue->GetLane(lane_id, prio_id);
      if (lane.size() > 0) { // Only schedule non-empty lanes
        // Get next worker using round-robin scheduling
        WorkerId worker_id = GetNextAvailableWorker();
        if (worker_id == 0) {
          continue; // No available workers, skip this lane
        }

        // Update lane's header to set the assigned worker
        auto* allocator = lane.GetAllocator();
        if (allocator) {
          // Cast allocator to our header type since it inherits from BaseAllocator
          auto* header = dynamic_cast<TaskQueueHeader*>(allocator);
          if (header) {
            header->assigned_worker_id = worker_id;
            header->is_enqueued = true;
          }
        }
        
        // Create FullPtr to the lane
        hipc::FullPtr<TaskQueue::TaskLane> lane_ptr(&lane);
        
        // Schedule this lane using the static NotifyWorkerLaneReady
        NotifyWorkerLaneReady(lane_ptr);
      }
    }
  }
}



bool WorkOrchestrator::ServerInitQueues(u32 num_lanes) {
  // Initialize process queues for different priorities
  bool success = true;

  // No longer creating local queues - external queue is managed by IPC Manager
  return success;
}



WorkerId WorkOrchestrator::GetNextAvailableWorker() {
  if (all_workers_.empty()) {
    return 0; // No workers available
  }

  // Round-robin scheduling
  u32 worker_index = next_worker_index_for_scheduling_.fetch_add(1) % all_workers_.size();
  Worker* worker = all_workers_[worker_index];
  
  return worker ? worker->GetId() : 0;
}

/*static*/ void WorkOrchestrator::NotifyWorkerLaneReady(hipc::FullPtr<TaskQueue::TaskLane> lane_ptr) {
  if (lane_ptr.IsNull()) {
    return;
  }
  
  // Get IPC Manager to access worker queues in shared memory
  IpcManager* ipc = CHI_IPC;
  if (!ipc) {
    return;
  }
  
  // TODO: Get worker ID from lane header/mapping
  // For now, use round-robin to determine which worker to assign to
  // In a complete implementation, the lane would have a header indicating its assigned worker
  static std::atomic<u32> next_worker_index{0};
  
  // Get total number of workers from IPC Manager
  u32 num_workers = ipc->GetWorkerCount();
  if (num_workers == 0) {
    return;
  }
  
  // Round-robin worker assignment
  u32 worker_id = next_worker_index.fetch_add(1) % num_workers;
  
  // Get the worker's active queue from shared memory
  auto worker_queue = ipc->GetWorkerQueue(worker_id);
  if (!worker_queue.IsNull()) {
    // Enqueue the lane to the worker's queue
    worker_queue->push(lane_ptr);
  }
}

}  // namespace chi