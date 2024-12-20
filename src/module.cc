//
// Created by llogan on 7/22/24.
//

#include "chimaera/module_registry/module.h"

#include "chimaera/chimaera_types.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#ifdef CHIMAERA_RUNTIME
#include "chimaera/api/chimaera_runtime.h"
#endif

namespace chi {

/** Create lanes for the Module */
void Module::CreateLaneGroup(LaneGroupId group_id, u32 count, u32 flags) {
#ifdef CHIMAERA_RUNTIME
  lane_groups_.emplace_back(std::make_shared<LaneGroup>(flags));
  LaneGroup &lane_group = *lane_groups_[group_id];
  lane_group.reserve(count);
  u32 group_prio;
  if (flags & QUEUE_LOW_LATENCY) {
    group_prio = TaskPrioOpt::kLowLatency;
  } else {
    group_prio = TaskPrioOpt::kHighLatency;
  }
  for (LaneId lane_id = 0; lane_id < count; ++lane_id) {
    ingress::Lane *ig_lane;
    ig_lane = CHI_WORK_ORCHESTRATOR->GetLeastLoadedIngressLane(group_prio);
    Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(ig_lane->worker_id_);
    for (TaskPrio prio = 0; prio < TaskPrioOpt::kNumPrio; ++prio) {
      worker.load_ += 1;
      lane_group.emplace_back(lane_id, prio, group_id, ig_lane->worker_id_);
    }
  }
#endif
}

}  // namespace chi