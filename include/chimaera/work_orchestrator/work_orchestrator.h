#ifndef CHI_WORK_ORCHESTRATOR_H_
#define CHI_WORK_ORCHESTRATOR_H_

#include <hermes_shm/util/singleton.h>
#include <memory>
#include <vector>
#include "../config/config_manager.h"
#include "worker.h"

namespace chi {

#ifdef CHIMAERA_RUNTIME

class WorkOrchestrator {
private:
  std::vector<std::unique_ptr<Worker>> workers_;
  bool is_initialized_;

public:
  WorkOrchestrator() : is_initialized_(false) {}

  void Initialize();
  void Shutdown();
  bool IsInitialized() const;
  const std::vector<std::unique_ptr<Worker>>& GetWorkers() const;

private:
  void CreateWorkers(const WorkOrchestratorConfig& config);
  void MapQueues();
  void StartWorkers();
};

#endif  // CHIMAERA_RUNTIME

}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::WorkOrchestrator, chiWorkOrchestrator);
#define CHI_WORK_ORCHESTRATOR \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::WorkOrchestrator, chiWorkOrchestrator)
#define CHI_WORK_ORCHESTRATOR_T chi::WorkOrchestrator*

#endif  // CHI_WORK_ORCHESTRATOR_H_