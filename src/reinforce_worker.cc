//
// Created by llogan on 7/30/24.
//

#include "chimaera/work_orchestrator/reinforce_worker.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera/work_orchestrator/corwlock.h"

namespace chi {

void ReinforceWorker::Run() {
  while (!CHI_WORK_ORCHESTRATOR->kill_requested_) {
    Reinforce();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ReinforceWorker::Reinforce() {
  // TODO(llogan): figure out why this may be segaulting
  return;
  ScopedCoRwTryReadLock upgrade_lock(CHI_MOD_REGISTRY->upgrade_lock_);
  if (!upgrade_lock.locked_) {
    return;
  }
  CHI_WORK_ORCHESTRATOR->ImportModule("chimaera_monitor");
  std::vector<Load> loads = CHI_WORK_ORCHESTRATOR->CalculateLoad();
  RunContext rctx;
  for (auto pool_it = CHI_MOD_REGISTRY->pools_.begin();
       pool_it != CHI_MOD_REGISTRY->pools_.end(); ++pool_it) {
    for (auto cont_it = pool_it->second.containers_.begin();
         cont_it != pool_it->second.containers_.end(); ++cont_it) {
      Container *container = cont_it->second;
      for (u32 method_i = 0; method_i < 100; ++method_i) {
        container->Monitor(MonitorMode::kReinforceLoad, method_i,
                           nullptr, rctx);
      }
    }
  }
}

}  // namespace chi