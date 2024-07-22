//
// Created by llogan on 7/22/24.
//

#include "chimaera/module_registry/module.h"
#ifdef CHIMAERA_RUNTIME
#include "chimaera/api/chimaera_runtime.h"
#endif

namespace chi {

/** Create lanes for the Module */
void Module::CreateLaneGroup(const LaneGroupId &id, u32 count, u32 flags) {
#ifdef CHIMAERA_RUNTIME
  lane_groups_.emplace(id, std::make_shared<LaneGroup>());
  LaneGroup &lane_group = *lane_groups_[id];
  lane_group.lanes_.reserve(count);
  for (u32 i = 0; i < count; ++i) {
    ingress::Lane *ig_lane;
    u32 lane_prio;
    if (flags & QUEUE_LOW_LATENCY) {
      // Find least-burdened dedicated worker
      lane_prio = TaskPrio::kLowLatency;
    } else {
      lane_prio = TaskPrio::kHighLatency;
    }
    ig_lane = CHI_WORK_ORCHESTRATOR->GetLeastLoadedIngressLane(
        lane_prio);
    Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(ig_lane->worker_id_);
    worker.load_ += 1;
    lane_group.lanes_.emplace_back(QueueId{id, lane_counter_++},
                                   ig_lane->id_,
                                   ig_lane->worker_id_);
  }
#endif
}

}  // namespace chi