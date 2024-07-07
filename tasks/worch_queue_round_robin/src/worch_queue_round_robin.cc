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

#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/api/chimaera_runtime.h"
#include "worch_queue_round_robin/worch_queue_round_robin.h"

namespace chi::worch_queue_round_robin {

class Server : public TaskLib {
 public:
  u32 count_lowlat_;
  u32 count_highlat_;

 public:
  /** Construct work orchestrator queue scheduler */
  void Create(CreateTask *task, RunContext &rctx) {
    count_lowlat_ = 0;
    count_highlat_ = 0;
    CreateLaneGroup(0, 1);
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Route a task to a lane */
  Lane* Route(const Task *task) override {
    return GetLaneByHash(0, 0);
  }

  /** Destroy work orchestrator queue scheduler */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(u32 mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Schedule work orchestrator queues */
  void Schedule(ScheduleTask *task, RunContext &rctx) {
    // Iterate over the set of ChiContainers
    ScopedRwReadLock lock(CHI_TASK_REGISTRY->lock_, 0);
    for (auto pool_it = CHI_TASK_REGISTRY->pools_.begin();
         pool_it != CHI_TASK_REGISTRY->pools_.end(); ++pool_it) {
      for (auto cont_it = pool_it->second.containers_.begin();
           cont_it != pool_it->second.containers_.end(); ++cont_it) {
        Container *container = cont_it->second;
        for (auto lane_grp_it = container->lane_groups_.begin();
             lane_grp_it != container->lane_groups_.end(); ++lane_grp_it) {
          LaneGroup &lane_grp = lane_grp_it->second;
          for (auto lane_it = lane_grp.lanes_.begin();
               lane_it != lane_grp.lanes_.end(); ++lane_it) {
            Lane &lane = *lane_it;
            // Check the worker the container maps to
            Worker *worker =
                CHI_WORK_ORCHESTRATOR->workers_[lane.worker_id_].get();
            // Check if this ChiLane is low latency.
            if (worker->IsLowLatency()) {
            }
            // If not, the worker should perform vertical migration.
          }
        }
        // Check the worker the container maps to
        // Does this match the worker it is on?
        // If not, the worker should perform vertical migration.
      }
    }
  }
  void MonitorSchedule(u32 mode, ScheduleTask *task, RunContext &rctx) {
  }

#include "worch_queue_round_robin/worch_queue_round_robin_lib_exec.h"
};

}  // namespace chi

CHI_TASK_CC(chi::worch_queue_round_robin::Server, "worch_queue_round_robin");
