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

  /** Basic monitoring function */
  void MonitorBase(MonitorModeId mode, MethodId method, Task *task,
                   RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[method].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[method].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[method].DoTrain();
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        break;
      }
    }
  }

  /** Create the state */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_LOW_LATENCY);
    for (int i = 0; i < Method::kCount; ++i) {
      monitor_[i].Shape(hshm::Formatter::format("{}-method-{}", name_, i));
    }
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {
    MonitorBase(mode, Method::kCreate, task, rctx);
  }

  /** Destroy the state */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
    MonitorBase(mode, Method::kDestroy, task, rctx);
  }

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    return GetLaneByHash(kDefaultGroup, task->prio_, 0);
  }

  /** Update number of lanes */
  void UpdateDomain(UpdateDomainTask *task, RunContext &rctx) {
    std::vector<UpdateDomainInfo> ops = task->ops_.vec();
    CHI_RPC->UpdateDomains(ops);
  }
  void MonitorUpdateDomain(MonitorModeId mode, UpdateDomainTask *task,
                           RunContext &rctx) {
    MonitorBase(mode, Method::kUpdateDomain, task, rctx);
  }

  /** Register a module dynamically */
  void RegisterModule(RegisterModuleTask *task, RunContext &rctx) {
    std::string lib_name = task->lib_name_.str();
    HILOG(kInfo, "Registering module? {}", lib_name);
    CHI_MOD_REGISTRY->RegisterModule(lib_name);
  }
  void MonitorRegisterModule(MonitorModeId mode, RegisterModuleTask *task,
                             RunContext &rctx) {
    MonitorBase(mode, Method::kRegisterModule, task, rctx);
  }

  /** Destroy a module */
  void DestroyModule(DestroyModuleTask *task, RunContext &rctx) {
    std::string lib_name = task->lib_name_.str();
    CHI_MOD_REGISTRY->DestroyModule(lib_name);
  }
  void MonitorDestroyModule(MonitorModeId mode, DestroyModuleTask *task,
                            RunContext &rctx) {
    MonitorBase(mode, Method::kDestroyModule, task, rctx);
  }

  /** Upgrade a module dynamically */
  void UpgradeModule(UpgradeModuleTask *task, RunContext &rctx) {
    ScopedCoRwWriteLock upgrade_lock(CHI_MOD_REGISTRY->upgrade_lock_);
    // Get the set of ChiContainers
    std::string lib_name = task->lib_name_.str();
    std::vector<Container *> containers =
        CHI_MOD_REGISTRY->GetContainers(lib_name);
    std::vector<Container *> new_containers;
    // Load the updated code
    ModuleInfo new_info;
    CHI_MOD_REGISTRY->LoadModule(lib_name, new_info);
    // Copy the old state to the new
    for (Container *container : containers) {
      Container *new_container = new_info.alloc_state_();
      (*new_container) = (*container);
      task->old_ = container;
      new_container->Run(Method::kUpgrade, task, rctx);
      new_containers.emplace_back(new_container);
    }
    // Get current iter count for each worker
    std::vector<size_t> iter_counts;
    for (std::unique_ptr<Worker> &worker : CHI_WORK_ORCHESTRATOR->workers_) {
      iter_counts.push_back(worker->iter_count_);
    }
    // Plug all module-related lanes
    for (Container *container : containers) {
      container->PlugAllLanes();
    }
    // Wait for at least two iterations per-worker
    for (size_t i = 0; i < iter_counts.size(); ++i) {
      while (CHI_WORK_ORCHESTRATOR->workers_[i]->iter_count_ <
             iter_counts[i] + 2) {
        task->Yield();
      }
    }
    HILOG(kInfo, "Upgrading on worker {}",
          CHI_WORK_ORCHESTRATOR->GetCurrentWorker()->id_);
    // Wait for all active tasks to complete
    for (Container *container : containers) {
      while (container->GetNumActiveTasks() > 0) {
        //        HILOG(kInfo, "Active tasks: {}",
        //        container->GetNumActiveTasks());
        task->Yield();
      }
    }
    // Plug the module & replace pointers
    CHI_MOD_REGISTRY->PlugModule(lib_name);
    CHI_MOD_REGISTRY->ReplaceModule(new_info);
    for (Container *new_container : new_containers) {
      CHI_MOD_REGISTRY->ReplaceContainer(new_container);
    }
    // Unplug everything
    for (Container *container : containers) {
      container->UnplugAllLanes();
    }
    CHI_MOD_REGISTRY->UnplugModule(lib_name);
  }
  void MonitorUpgradeModule(MonitorModeId mode, UpgradeModuleTask *task,
                            RunContext &rctx) {
    MonitorBase(mode, Method::kUpgradeModule, task, rctx);
  }

  /** Create a pool */
  void CreatePool(CreatePoolTask *task, RunContext &rctx) {
    ScopedCoMutex lock(CHI_CUR_LANE->comux_);
    std::string lib_name = task->lib_name_.str();
    std::string pool_name = task->pool_name_.str();
    // Check local registry for pool
    bool state_existed = false;
    PoolId found_pool = CHI_MOD_REGISTRY->PoolExists(pool_name, task->ctx_.id_);
    if (!found_pool.IsNull()) {
      task->ctx_.id_ = found_pool;
      state_existed = true;
      return;
    }
    // Check global registry for pool
    if (task->ctx_.id_.IsNull()) {
      task->ctx_.id_ = CHI_MOD_REGISTRY->GetOrCreatePoolId(pool_name);
    }
    // Create the pool
    HILOG(kInfo, "(node {}) Creating pool {} ({}) with id {} (task_node={})",
          CHI_CLIENT->node_id_, pool_name, pool_name.size(), task->ctx_.id_,
          task->task_node_);
    if (task->ctx_.id_.IsNull()) {
      HELOG(kError, "(node {}) The pool {} with id {} is NULL.",
            CHI_CLIENT->node_id_, pool_name, task->ctx_.id_);
      return;
    }
    // Get # of lanes to create
    u32 global_containers = task->ctx_.global_containers_;
    u32 local_containers_pn = task->ctx_.local_containers_pn_;
    u32 lanes_per_container = task->ctx_.lanes_per_container_;
    if (global_containers == 0) {
      global_containers = CHI_RPC->hosts_.size();
    }
    if (local_containers_pn == 0) {
      local_containers_pn = CHI_RUNTIME->GetMaxContainersPn();
    }
    // Update the default domains for the state
    std::vector<UpdateDomainInfo> ops = CHI_RPC->CreateDefaultDomains(
        task->ctx_.id_, chi::ADMIN_POOL_ID, task->affinity_, global_containers,
        local_containers_pn);
    CHI_RPC->UpdateDomains(ops);
    std::vector<SubDomainId> containers =
        CHI_RPC->GetLocalContainers(task->ctx_.id_);
    // Print the created domain
    //    CHI_RPC->PrintDomain(DomainId{task->ctx_.id_,
    //    SubDomainId::kContainerSet}); CHI_RPC->PrintSubdomainSet(containers);
    // Create the pool
    bool did_create = CHI_MOD_REGISTRY->CreatePool(
        lib_name.c_str(), pool_name.c_str(), task->ctx_.id_, task, containers);
    if (!did_create) {
      HELOG(kFatal, "Failed to create container: {}", pool_name);
      return;
    }
    if (task->root_) {
      // Broadcast the state creation to all nodes
      CHI_ADMIN->CreatePool(HSHM_MCTX, task->affinity_, *task);
      HILOG(kInfo,
            "(node {}) Broadcasting container creation (task_node={}): pool {}",
            CHI_RPC->node_id_, task->task_node_, task->pool_name_.str());
    }
    HILOG(kInfo, "(node {}) Created containers for task {}", CHI_RPC->node_id_,
          task->task_node_);
  }
  void MonitorCreatePool(MonitorModeId mode, CreatePoolTask *task,
                         RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kCreatePool].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kCreatePool].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kCreatePool].DoTrain();
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<CreatePoolTask *>(replicas[0].ptr_);
        task->ctx_ = replica->ctx_;
        break;
      }
    }
  }

  /** Get pool id, fail if DNE */
  void GetPoolId(GetPoolIdTask *task, RunContext &rctx) {
    std::string pool_name = task->pool_name_.str();
    task->id_ = CHI_MOD_REGISTRY->GetPoolId(pool_name);
  }
  void MonitorGetPoolId(MonitorModeId mode, GetPoolIdTask *task,
                        RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kGetPoolId].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kGetPoolId].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kGetPoolId].DoTrain();
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<GetPoolIdTask *>(replicas[0].ptr_);
        task->id_ = replica->id_;
      }
    }
  }

  /** Destroy a pool */
  void DestroyContainer(DestroyContainerTask *task, RunContext &rctx) {
    CHI_MOD_REGISTRY->DestroyContainer(task->id_);
  }
  void MonitorDestroyContainer(MonitorModeId mode, DestroyContainerTask *task,
                               RunContext &rctx) {
    MonitorBase(mode, Method::kDestroyContainer, task, rctx);
  }

  /** Stop this runtime */
  void StopRuntime(StopRuntimeTask *task, RunContext &rctx) {
    if (task->root_) {
      HILOG(kInfo, "(node {}) Broadcasting runtime stop (task_node={})",
            CHI_RPC->node_id_, task->task_node_);
      CHI_ADMIN->AsyncStopRuntime(HSHM_MCTX, DomainQuery::GetGlobalBcast(),
                                  false);
    } else if (CHI_RPC->node_id_ == task->task_node_.root_.node_id_) {
      HILOG(kInfo, "(node {}) Ignoring runtime stop (task_node={})",
            CHI_RPC->node_id_, task->task_node_);
      return;
    }
    HILOG(kInfo, "(node {}) Handling runtime stop (task_node={})",
          CHI_RPC->node_id_, task->task_node_);
    CHI_THALLIUM->StopThisDaemon();
    CHI_WORK_ORCHESTRATOR->FinalizeRuntime();
  }
  void MonitorStopRuntime(MonitorModeId mode, StopRuntimeTask *task,
                          RunContext &rctx) {
    MonitorBase(mode, Method::kStopRuntime, task, rctx);
  }

  /** Set work orchestrator policy */
  void SetWorkOrchQueuePolicy(SetWorkOrchQueuePolicyTask *task,
                              RunContext &rctx) {}
  void MonitorSetWorkOrchQueuePolicy(MonitorModeId mode,
                                     SetWorkOrchQueuePolicyTask *task,
                                     RunContext &rctx) {
    MonitorBase(mode, Method::kSetWorkOrchQueuePolicy, task, rctx);
  }

  /** Set work orchestration policy */
  void SetWorkOrchProcPolicy(SetWorkOrchProcPolicyTask *task,
                             RunContext &rctx) {}
  void MonitorSetWorkOrchProcPolicy(MonitorModeId mode,
                                    SetWorkOrchProcPolicyTask *task,
                                    RunContext &rctx) {
    MonitorBase(mode, Method::kSetWorkOrchProcPolicy, task, rctx);
  }

  /** Flush the runtime */
  void Flush(FlushTask *task, RunContext &rctx) {}
  void MonitorFlush(MonitorModeId mode, FlushTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kEstLoad: {
        rctx.load_.cpu_load_ = monitor_[Method::kFlush].Predict();
        break;
      }
      case MonitorMode::kSampleLoad: {
        monitor_[Method::kFlush].Add(rctx.timer_.GetNsec(), rctx.load_);
        break;
      }
      case MonitorMode::kReinforceLoad: {
        monitor_[Method::kFlush].DoTrain();
        break;
      }
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<FlushTask *>(replicas[0].ptr_);
        task->work_done_ += replica->work_done_;
      }
    }
  }

  /** Get the domain size */
  void GetDomainSize(GetDomainSizeTask *task, RunContext &rctx) {
    task->dom_size_ = CHI_RPC->GetDomainSize(task->dom_id_);
  }
  void MonitorGetDomainSize(MonitorModeId mode, GetDomainSizeTask *task,
                            RunContext &rctx) {
    MonitorBase(mode, Method::kGetDomainSize, task, rctx);
  }

  /** The PollStats method */
  void PollStats(PollStatsTask *task, RunContext &rctx) {
    task->stats_.resize(CHI_WORK_ORCHESTRATOR->workers_.size());
    for (const std::unique_ptr<Worker> &worker :
         CHI_WORK_ORCHESTRATOR->workers_) {
      WorkerStats &stats = task->stats_[worker->id_];
      stats.worker_id_ = worker->id_;
      stats.num_tasks_ = worker->active_.active_lanes_.GetStats(stats.lanes_);
    }
  }
  void MonitorPollStats(MonitorModeId mode, PollStatsTask *task,
                        RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
      }
    }
  }

 public:
#include "chimaera_admin/chimaera_admin_lib_exec.h"
};

}  // namespace chi::Admin

CHI_TASK_CC(chi::Admin::Server, "chimaera_admin");
