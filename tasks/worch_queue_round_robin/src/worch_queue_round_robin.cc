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

namespace chm::worch_queue_round_robin {

class Server : public TaskLib {
 public:
  u32 count_lowlat_;
  u32 count_highlat_;

 public:
  /** Construct work orchestrator queue scheduler */
  void Create(CreateTask *task, RunContext &rctx) {
    count_lowlat_ = 0;
    count_highlat_ = 0;
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Destroy work orchestrator queue scheduler */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** Schedule work orchestrator queues */
  void Schedule(ScheduleTask *task, RunContext &rctx) {
    // Check if any new queues need to be scheduled
    for (MultiQueue &queue : *HRUN_QM_RUNTIME->queue_map_) {
      if (queue.id_.IsNull() || !queue.flags_.Any(QUEUE_READY)) {
        continue;
      }
      for (LaneGroup &lane_group : *queue.groups_) {
        u32 num_lanes = lane_group.num_lanes_;
        for (LaneId lane_id = lane_group.num_scheduled_; lane_id < num_lanes; ++lane_id) {
          u32 worker_id;
          if (lane_group.IsLowLatency()) {
            u32 worker_off = count_lowlat_ % HRUN_WORK_ORCHESTRATOR->dworkers_.size();
            count_lowlat_ += 1;
            Worker &worker = *HRUN_WORK_ORCHESTRATOR->dworkers_[worker_off];
            worker.PollQueues({WorkEntry(lane_group.prio_, lane_id, &queue)});
            worker_id = worker.id_;
            HILOG(kInfo, "(node {}) Scheduling the queue {} (prio {}, lane {}, worker {})",
                  CHM_CLIENT->node_id_, queue.id_, lane_group.prio_, lane_id, worker.id_);
          } else {
            u32 worker_off = count_highlat_ % HRUN_WORK_ORCHESTRATOR->oworkers_.size();
            count_highlat_ += 1;
            Worker &worker = *HRUN_WORK_ORCHESTRATOR->oworkers_[worker_off];
            worker.PollQueues({WorkEntry(lane_group.prio_, lane_id, &queue)});
            worker_id = worker.id_;
            HILOG(kInfo, "(node {}) Scheduling the queue {} (prio {}, lane {}, worker {})",
                  CHM_CLIENT->node_id_, queue.id_, lane_group.prio_, lane_id, worker.id_);
          }
          Lane &lane = lane_group.GetLane(lane_id);
          lane.worker_id_ = worker_id;
        }
        lane_group.num_scheduled_ = num_lanes;
      }
    }
  }
  void MonitorSchedule(u32 mode, ScheduleTask *task, RunContext &rctx) {
  }

#include "worch_queue_round_robin/worch_queue_round_robin_lib_exec.h"
};

}  // namespace chm

HRUN_TASK_CC(chm::worch_queue_round_robin::Server, "worch_queue_round_robin");
