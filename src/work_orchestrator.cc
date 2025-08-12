/**
 * Work orchestrator implementation
 */

#include "chimaera/work_orchestrator.h"
#include "chimaera/singletons.h"

namespace chi {

//===========================================================================
// LaneQueue Implementation
//===========================================================================

LaneQueue::LaneQueue(LaneId lane_id, hipc::multi_mpsc_queue<hipc::Pointer>* parent_queue, u32 lane_index)
    : lane_id_(lane_id), lane_index_(lane_index), parent_queue_(parent_queue),
      assigned_worker_(0), is_active_(false) {
}

void LaneQueue::Enqueue(hipc::Pointer task_ptr) {
  if (parent_queue_ && !task_ptr.IsNull()) {
    // Enqueue to the specific lane within the multi-MPSC queue
    parent_queue_->GetLane(0, lane_index_).push(task_ptr);
  }
}

bool LaneQueue::Dequeue(hipc::Pointer& task_ptr) {
  if (parent_queue_) {
    // Try to pop from the specific lane
    auto token = parent_queue_->GetLane(0, lane_index_).pop(task_ptr);
    return !token.IsNull();  // Check if the token is valid (successful pop)
  }
  return false;
}

bool LaneQueue::IsEmpty() const {
  if (parent_queue_) {
    return parent_queue_->GetLane(0, lane_index_).size() == 0;
  }
  return true;
}

size_t LaneQueue::Size() const {
  if (parent_queue_) {
    return parent_queue_->GetLane(0, lane_index_).size();
  }
  return 0;
}

void LaneQueue::SetAssignedWorker(WorkerId worker_id) {
  assigned_worker_.store(worker_id);
}

WorkerId LaneQueue::GetAssignedWorker() const {
  return assigned_worker_.load();
}

bool LaneQueue::HasAssignedWorker() const {
  return assigned_worker_.load() != 0;
}

void LaneQueue::ClearAssignedWorker() {
  assigned_worker_.store(0);
}

//===========================================================================
// Work Orchestrator Implementation
//===========================================================================

// Constructor and destructor removed - handled by HSHM singleton pattern

bool WorkOrchestrator::Init() {
  if (is_initialized_) {
    return true;
  }

  // Initialize lane management
  next_lane_id_.store(1); // Start from 1, 0 is reserved for invalid
  next_worker_index_for_scheduling_.store(0);

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
  // Initialize lane to worker mappings
  // This will be updated as lanes are created and scheduled
  if (!all_workers_.empty()) {
    lane_to_worker_map_.resize(all_workers_.size(), 0);
  }

  return true;
}

//===========================================================================
// Lane Scheduling Methods
//===========================================================================

void WorkOrchestrator::ScheduleLanes(const std::vector<LaneId>& lanes) {
  for (LaneId lane_id : lanes) {
    ScheduleLane(lane_id);
  }
}

bool WorkOrchestrator::ScheduleLane(LaneId lane_id) {
  std::unique_lock<std::shared_mutex> lock(lanes_mutex_);
  
  auto it = lanes_.find(lane_id);
  if (it == lanes_.end()) {
    return false; // Lane not found
  }

  LaneQueue* lane = it->second.get();
  if (lane->HasAssignedWorker()) {
    return true; // Already assigned
  }

  // Get next worker using round-robin scheduling
  WorkerId worker_id = GetNextAvailableWorker();
  if (worker_id == 0) {
    return false; // No available workers
  }

  // Assign lane to worker
  lane->SetAssignedWorker(worker_id);
  lane->SetActive(true);

  // Add to active lanes list
  active_lanes_.push_back(lane_id);

  return true;
}

std::vector<LaneId> WorkOrchestrator::CreateLocalQueue(QueuePriority priority, u32 num_lanes) {
  std::vector<LaneId> created_lanes;

  // Get or create multi-MPSC queue for this priority
  auto& queue_ptr = priority_queues_[priority];
  if (!queue_ptr) {
    // Create new multi-MPSC queue with the requested number of lanes
    auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
    hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, alloc);
    // Can't use make_unique with HSHM objects, need to use allocator
    auto queue_full_ptr = alloc->template NewObj<hipc::multi_mpsc_queue<hipc::Pointer>>(
        HSHM_MCTX, ctx_alloc, num_lanes, 1, 1024);
    queue_ptr.reset(queue_full_ptr.ptr_);
  }

  std::unique_lock<std::shared_mutex> lock(lanes_mutex_);

  // Create individual lanes for this queue
  for (u32 i = 0; i < num_lanes; ++i) {
    LaneId lane_id = next_lane_id_.fetch_add(1);
    
    // Create lane object
    auto lane = std::make_unique<LaneQueue>(lane_id, queue_ptr.get(), i);
    
    // Store the lane
    lanes_[lane_id] = std::move(lane);
    created_lanes.push_back(lane_id);
  }

  return created_lanes;
}

bool WorkOrchestrator::ServerInitQueues(u32 num_lanes) {
  // Initialize process queues for different priorities
  bool success = true;

  // Create queues for each priority level
  auto low_latency_lanes = CreateLocalQueue(kLowLatency, num_lanes);
  auto high_latency_lanes = CreateLocalQueue(kHighLatency, num_lanes);

  // Schedule the lanes for processing
  ScheduleLanes(low_latency_lanes);
  ScheduleLanes(high_latency_lanes);

  return success && !low_latency_lanes.empty() && !high_latency_lanes.empty();
}

LaneQueue* WorkOrchestrator::GetLane(LaneId lane_id) {
  std::shared_lock<std::shared_mutex> lock(lanes_mutex_);
  
  auto it = lanes_.find(lane_id);
  if (it != lanes_.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::vector<LaneId> WorkOrchestrator::GetActiveLanesForWorker(WorkerId worker_id) {
  std::vector<LaneId> worker_lanes;
  std::shared_lock<std::shared_mutex> lock(lanes_mutex_);

  for (const auto& [lane_id, lane_ptr] : lanes_) {
    if (lane_ptr->GetAssignedWorker() == worker_id && lane_ptr->IsActive()) {
      worker_lanes.push_back(lane_id);
    }
  }

  return worker_lanes;
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

void WorkOrchestrator::NotifyWorkerLaneReady(u32 queue_id, u32 lane_id) {
  // Delegate to the overloaded method with null pointer
  NotifyWorkerLaneReady(queue_id, lane_id, hipc::Pointer::GetNull());
}

void WorkOrchestrator::NotifyWorkerLaneReady(u32 queue_id, u32 lane_id, hipc::Pointer lane_ref_ptr) {
  // This method will be called from TaskQueue - it needs to route to appropriate workers
  // For now, we'll route to the first available worker (round-robin)
  // In a complete implementation, we'd maintain lane-to-worker mappings
  
  if (all_workers_.empty()) {
    return;
  }
  
  // Get next worker using round-robin scheduling
  WorkerId worker_id = GetNextAvailableWorker();
  if (worker_id > 0 && worker_id <= all_workers_.size()) {
    Worker* worker = all_workers_[worker_id - 1]; // worker_id is 1-based, vector is 0-based
    if (worker) {
      // Pass the lane reference pointer to the worker
      worker->EnqueueLane(lane_ref_ptr);
    }
  }
}

}  // namespace chi