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
#include "TASK_NAME/TASK_NAME.h"
#include "chimaera/monitor/monitor.h"

namespace chi::TASK_NAME {

class Server : public Module {
 public:
  Server() = default;

  /** Construct TASK_NAME */
  void Create(CreateTask *task, RunContext &rctx) {
    // Create a set of lanes for holding tasks
    CreateLaneGroup(0, 1, QUEUE_LOW_LATENCY);
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
  }

  /** Route a task to a lane */
  Lane* Route(const Task *task) override {
    // Route tasks to lanes based on their properties
    // E.g., a strongly consistent filesystem could map tasks to a lane
    // by the hash of an absolute filename path.
    return GetLaneByHash(0, 0);
  }

  /** Destroy TASK_NAME */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** A custom method */
  void Custom(CustomTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorCustom(MonitorModeId mode, CustomTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<CustomTask *>(
            replicas[0].ptr_);
      }
    }
  }
 public:
#include "TASK_NAME/TASK_NAME_lib_exec.h"
};

}  // namespace chi::TASK_NAME

CHI_TASK_CC(chi::TASK_NAME::Server, "TASK_NAME");
