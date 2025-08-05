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

#include "chimaera_admin/chimaera_admin_client.h"

namespace chi::Admin {

class Server : public Module {
public:
  Server() {}

  CHI_BEGIN(Create)
  /** Create the state */
  void Create(CreateTask *task, RunContext &rctx) {}
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}
  CHI_END(Create)

  CHI_BEGIN(Destroy)
  /** Destroy the state */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }
  CHI_END(Destroy)

  CHI_BEGIN(CreatePool)
  /** The CreatePool method */
  void CreatePool(CreatePoolTask *task, RunContext &rctx) {}
  void MonitorCreatePool(MonitorModeId mode, CreatePoolTask *task,
                         RunContext &rctx) {
    switch (mode) {
    case MonitorMode::kReplicaAgg: {
      std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
    }
    }
  }
  CHI_END(CreatePool)

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
