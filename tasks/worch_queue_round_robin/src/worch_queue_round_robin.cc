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
  CLS_CONST LaneGroupId kDefaultGroup = 0;
  u32 count_lowlat_;
  u32 count_highlat_;

 public:
  /** Construct work orchestrator queue scheduler */
  void Create(CreateTask *task, RunContext &rctx) {
    count_lowlat_ = 0;
    count_highlat_ = 0;
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_HIGH_LATENCY);
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}

  /** Route a task to a lane */
  Lane *Route(const Task *task) override {
    return GetLaneByHash(kDefaultGroup, task->prio_, 0);
  }

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
      return lane.prio_ == TaskPrioOpt::kLowLatency;
    }
    size_t avg_cpu_load = lane.load_.cpu_load_ / num_tasks;
    size_t avg_io_load = lane.load_.io_load_ / num_tasks;
    return avg_cpu_load < KILOBYTES(50) && avg_io_load < KILOBYTES(8);
  }

  /** Schedule work orchestrator queues */
  void Schedule(ScheduleTask *task, RunContext &rctx) {
    // TODO(llogan): Finish
    task->UnsetStarted();
    return;
  }
  void MonitorSchedule(MonitorModeId mode, ScheduleTask *task,
                       RunContext &rctx) {}

#include "worch_queue_round_robin/worch_queue_round_robin_lib_exec.h"
};

}  // namespace chi::worch_queue_round_robin

CHI_TASK_CC(chi::worch_queue_round_robin::Server, "worch_queue_round_robin");
