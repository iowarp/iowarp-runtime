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

#include "worch_proc_round_robin/worch_proc_round_robin.h"

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera_admin/chimaera_admin.h"

namespace chi::worch_proc_round_robin {

class Server : public Module {
 public:
  CLS_CONST LaneGroupId kDefaultGroup = 0;

  /** Construct the work orchestrator process scheduler */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_HIGH_LATENCY);
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    return GetLaneByHash(kDefaultGroup, task->prio_, 0);
  }

  /** Destroy the work orchestrator process queue */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Schedule running processes */
  void Schedule(ScheduleTask *task, RunContext &rctx) {
    CHI_WORK_ORCHESTRATOR->DedicateCores();
  }
  void MonitorSchedule(MonitorModeId mode, ScheduleTask *task,
                       RunContext &rctx) {}

#include "worch_proc_round_robin/worch_proc_round_robin_lib_exec.h"
};

}  // namespace chi::worch_proc_round_robin

CHI_TASK_CC(chi::worch_proc_round_robin::Server, "worch_proc_round_robin");
