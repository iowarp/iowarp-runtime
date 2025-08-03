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

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/monitor/monitor.h"
#include "chimaera/work_orchestrator/comutex.h"
#include "chimaera/work_orchestrator/corwlock.h"
#include "chimaera/work_orchestrator/scheduler.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera_admin/chimaera_admin_client.h"

namespace chi::Admin {

class Server : public Module {
public:
  CLS_CONST LaneGroupId kDefaultGroup = 0;
  Task *queue_sched_;
  Task *proc_sched_;
  RollingAverage monitor_[Method::kCount];

public:
  Server() : queue_sched_(nullptr), proc_sched_(nullptr) {}

  CHI_BEGIN(Create)
  /** Create the state */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_LOW_LATENCY);
    for (int i = 0; i < Method::kCount; ++i) {
      monitor_[i].Shape(hshm::Formatter::format("{}-method-{}", name_, i));
    }
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}
  CHI_END(Create)

  CHI_BEGIN(Destroy)
  /** Destroy the state */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }
  CHI_END(Destroy)

  CHI_BEGIN(CreatePool)
  /** Create a pool */
  void CreatePool(CreatePoolTask *task, RunContext &rctx) {}
  void MonitorCreatePool(MonitorModeId mode, CreatePoolTask *task,
                         RunContext &rctx) {}
  CHI_END(CreatePool)

  CHI_BEGIN(StopRuntime)
  /** Stop this runtime */
  void StopRuntime(StopRuntimeTask *task, RunContext &rctx) {
    // HILOG(kInfo, "(node {}) Handling runtime stop (task_node={})",
    //       CHI_RPC->node_id_, task->task_node_);
    CHI_WORK_ORCHESTRATOR->BeginShutdown();
  }
  void MonitorStopRuntime(MonitorModeId mode, StopRuntimeTask *task,
                          RunContext &rctx) {}
  CHI_END(StopRuntime)

  CHI_AUTOGEN_METHODS // keep at class bottom

      public:
#include "chimaera_admin/chimaera_admin_lib_exec.h"
};

} // namespace chi::Admin

CHI_TASK_CC(chi::Admin::Server, "chimaera_admin");
