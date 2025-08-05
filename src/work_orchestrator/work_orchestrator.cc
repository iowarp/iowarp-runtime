#include "chimaera/work_orchestrator/work_orchestrator.h"

#ifdef CHIMAERA_RUNTIME

#include "chimaera/ipc/ipc_manager.h"

namespace chi {

void WorkOrchestrator::Initialize() {
  if (is_initialized_) {
    return;
  }

  auto config = CHI_CONFIG_MANAGER->GetWorkOrchestratorConfig();

  CreateWorkers(config);
  MapQueues();
  StartWorkers();

  is_initialized_ = true;
  HILOG(kInfo, "Work Orchestrator initialized with {} workers",
        workers_.size());
}

void WorkOrchestrator::CreateWorkers(const WorkOrchestratorConfig &config) {
  int worker_id = 0;

  for (int cpu : config.cpus_) {
    WorkerType type = (worker_id % 2 == 0) ? WorkerType::kLowLatency
                                           : WorkerType::kHighLatency;
    workers_.emplace_back(std::make_unique<Worker>(type, worker_id, cpu));
    worker_id++;
  }

  workers_.emplace_back(std::make_unique<Worker>(
      WorkerType::kReinforcement, worker_id, config.reinforce_cpu_));
}

void WorkOrchestrator::MapQueues() {
  auto ipc_manager = CHI_IPC_MANAGER;
  auto process_queue = ipc_manager->GetProcessQueue();
  u32 num_lanes = process_queue->GetNumLanes();

  for (size_t i = 0; i < workers_.size() - 1; ++i) {
    u32 lanes_per_worker = num_lanes / (workers_.size() - 1);
    u32 start_lane = i * lanes_per_worker;
    u32 end_lane =
        (i == workers_.size() - 2) ? num_lanes : start_lane + lanes_per_worker;

    for (u32 lane = start_lane; lane < end_lane; ++lane) {
      workers_[i]->AddLane(lane);
    }
  }
}

void WorkOrchestrator::StartWorkers() {
  for (auto &worker : workers_) {
    worker->Start();
  }
}

void WorkOrchestrator::Shutdown() {
  for (auto &worker : workers_) {
    worker->Stop();
  }
  workers_.clear();
  is_initialized_ = false;
  HILOG(kInfo, "Work Orchestrator shut down");
}

bool WorkOrchestrator::IsInitialized() const { 
  return is_initialized_; 
}

const std::vector<std::unique_ptr<Worker>> &WorkOrchestrator::GetWorkers() const {
  return workers_;
}

}  // namespace chi

#endif  // CHIMAERA_RUNTIME

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::WorkOrchestrator, chiWorkOrchestrator);