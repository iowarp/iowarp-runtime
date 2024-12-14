/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "worch_queue_round_robin/worch_queue_round_robin.h"

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera_admin/chimaera_admin.h"

namespace chi::worch_queue_round_robin {

class Server : public Module {
 public:
  u32 count_lowlat_;
  u32 count_highlat_;

 public:
  /** Construct work orchestrator queue scheduler */
  void Create(CreateTask *task, RunContext &rctx) {
    count_lowlat_ = 0;
    count_highlat_ = 0;
    CreateLaneGroup(0, 1, QUEUE_HIGH_LATENCY);
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}

  /** Route a task to a lane */
  Lane *Route(const Task *task) override { return GetLaneByHash(0, 0); }

  /** Destroy work orchestrator queue scheduler */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Check if low latency */
  bool IsLowLatency(Lane &lane) {
    size_t num_tasks = lane.size();
    if (num_tasks == 0) {
      return lane.prio_ == TaskPrio::kLowLatency;
    }
    size_t avg_cpu_load = lane.load_.cpu_load_ / num_tasks;
    size_t avg_io_load = lane.load_.io_load_ / num_tasks;
    return avg_cpu_load < KILOBYTES(50) && avg_io_load < KILOBYTES(8);
  }

  /** Schedule work orchestrator queues */
  void Schedule(ScheduleTask *task, RunContext &rctx) {
    // Iterate over the set of ChiContainers
    // TODO(llogan): Figure out why this may be segfaulting
    task->UnsetStarted();
    return;
    ScopedCoRwReadLock upgrade_lock(CHI_MOD_REGISTRY->upgrade_lock_);
    std::vector<Load> loads = CHI_WORK_ORCHESTRATOR->CalculateLoad();
    for (auto pool_it = CHI_MOD_REGISTRY->pools_.begin();
         pool_it != CHI_MOD_REGISTRY->pools_.end(); ++pool_it) {
      for (auto cont_it = pool_it->second.containers_.begin();
           cont_it != pool_it->second.containers_.end(); ++cont_it) {
        Container *container = cont_it->second;
        for (auto lane_grp_it = container->lane_groups_.begin();
             lane_grp_it != container->lane_groups_.end(); ++lane_grp_it) {
          LaneGroup &lane_grp = *lane_grp_it->second;
          for (auto lane_it = lane_grp.lanes_.begin();
               lane_it != lane_grp.lanes_.end(); ++lane_it) {
            Lane &lane = *lane_it;
            if (&lane == CHI_CUR_LANE) {
              continue;
            }
            // Get the ingress lane to map the chi lane to
            ingress::Lane *ig_lane;
            if (IsLowLatency(lane)) {
              // Migrate to worker with the least load
              ig_lane = CHI_WORK_ORCHESTRATOR->GetThresholdIngressLane(
                  lane.worker_id_, loads, TaskPrio::kLowLatency);
            } else {
              ig_lane = CHI_WORK_ORCHESTRATOR->GetThresholdIngressLane(
                  lane.worker_id_, loads, TaskPrio::kHighLatency);
            }
            // Migrate the lane
            if (ig_lane && ig_lane->worker_id_ != lane.worker_id_) {
              lane.SetPlugged();
              while (lane.size() > 0) {
                task->Yield();
              }
              lane.worker_id_ = ig_lane->worker_id_;
              lane.ingress_id_ = ig_lane->id_;
              //            Worker *worker =
              //                CHI_WORK_ORCHESTRATOR->workers_[lane.worker_id_].get();
              //            worker->RelinquishingQueues();
              lane.UnsetPlugged();
            }
          }
        }
      }
    }
    task->UnsetStarted();
  }
  void MonitorSchedule(MonitorModeId mode, ScheduleTask *task,
                       RunContext &rctx) {}

#include "worch_queue_round_robin/worch_queue_round_robin_lib_exec.h"
};

}  // namespace chi::worch_queue_round_robin

CHI_TASK_CC(chi::worch_queue_round_robin::Server, "worch_queue_round_robin");
