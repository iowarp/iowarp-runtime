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
#include "chimaera/work_orchestrator/comutex.h"
#include "chimaera/work_orchestrator/scheduler.h"

namespace chi::Admin {

class Server : public Module {
 public:
  Task *queue_sched_;
  Task *proc_sched_;

 public:
  Server() : queue_sched_(nullptr), proc_sched_(nullptr) {}

  /** Create the state */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateLaneGroup(0, 1, QUEUE_LOW_LATENCY);
    task->SetModuleComplete();
  }
  void MonitorCreate(u32 mode, CreateTask *task, RunContext &rctx) {
  }

  /** Destroy the state */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(u32 mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Route a task to a lane */
  Lane* Route(const Task *task) override {
    return GetLaneByHash(0, 0);
  }

  /** Update number of lanes */
  void UpdateDomain(UpdateDomainTask *task, RunContext &rctx) {
    std::vector<UpdateDomainInfo> ops = task->ops_.vec();
    CHI_RPC->UpdateDomains(ops);
    task->SetModuleComplete();
  }
  void MonitorUpdateDomain(u32 mode,
                           UpdateDomainTask *task,
                           RunContext &rctx) {
  }

  /** Register a module dynamically */
  void RegisterModule(RegisterModuleTask *task, RunContext &rctx) {
    std::string lib_name = task->lib_name_.str();
    CHI_TASK_REGISTRY->RegisterModule(lib_name);
    task->SetModuleComplete();
  }
  void MonitorRegisterModule(u32 mode,
                              RegisterModuleTask *task,
                              RunContext &rctx) {
  }

  /** Destroy a module */
  void DestroyModule(DestroyModuleTask *task, RunContext &rctx) {
    std::string lib_name = task->lib_name_.str();
    CHI_TASK_REGISTRY->DestroyModule(lib_name);
    task->SetModuleComplete();
  }
  void MonitorDestroyModule(u32 mode,
                             DestroyModuleTask *task,
                             RunContext &rctx) {
  }

  /** Upgrade a module dynamically */
  void UpgradeModule(UpgradeModuleTask *task, RunContext &rctx) {
    // Get the set of ChiContainers
    std::string lib_name = task->lib_name_.str();
    std::vector<Container*> containers =
        CHI_TASK_REGISTRY->GetContainers(lib_name);
    std::vector<Container*> new_containers;
    // Load the updated code
    ModuleInfo new_info;
    CHI_TASK_REGISTRY->LoadModule(lib_name, new_info);
    // Copy the old state to the new
    for (Container *container : containers) {
      Container *new_container = new_info.alloc_state_();
      (*new_container) = (*container);
      task->old_ = container;
      new_container->Run(Method::kUpgrade, task, rctx);
      new_containers.emplace_back(new_container);
    }
    task->UnsetModuleComplete();
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
      while (CHI_WORK_ORCHESTRATOR->workers_[i]->iter_count_ < iter_counts[i] + 2) {
        task->Yield();
      }
    }
    HILOG(kInfo, "Upgrading on worker {}",
          CHI_WORK_ORCHESTRATOR->GetCurrentWorker()->id_);
    // Wait for all active tasks to complete
    for (Container *container : containers) {
      while (container->GetNumActiveTasks() > 0) {
        HILOG(kInfo, "Active tasks: {}", container->GetNumActiveTasks());
        task->Yield();
      }
    }
    // Plug the module & replace pointers
    CHI_TASK_REGISTRY->PlugModule(lib_name);
    CHI_TASK_REGISTRY->ReplaceModule(new_info);
    for (Container *new_container : new_containers) {
      CHI_TASK_REGISTRY->ReplaceContainer(new_container);
    }
    // Unplug everything
    for (Container *container : containers) {
      container->UnplugAllLanes();
    }
    CHI_TASK_REGISTRY->UnplugModule(lib_name);
    task->SetModuleComplete();
  }
  void MonitorUpgradeModule(u32 mode,
                            UpgradeModuleTask *task,
                            RunContext &rctx) {
  }

  /** Create a task state */
  void CreateContainer(CreateContainerTask *task, RunContext &rctx) {
    ScopedCoMutex lock(CHI_WORK_ORCHESTRATOR->GetCurrentLane()->comux_);
    std::string lib_name = task->lib_name_.str();
    std::string pool_name = task->pool_name_.str();
    // Check local registry for task state
    bool state_existed = false;
    PoolId found_pool = CHI_TASK_REGISTRY->PoolExists(
        pool_name, task->ctx_.id_);
    if (!found_pool.IsNull()) {
      task->ctx_.id_ = found_pool;
      state_existed = true;
      task->SetModuleComplete();
      return;
    }
    // Check global registry for task state
    if (task->ctx_.id_.IsNull()) {
      task->ctx_.id_ = CHI_TASK_REGISTRY->GetOrCreatePoolId(pool_name);
    }
    // Create the task state
    HILOG(kInfo, "(node {}) Creating task state {} with id {} (task_node={})",
          CHI_CLIENT->node_id_, pool_name, task->ctx_.id_, task->task_node_);
    if (task->ctx_.id_.IsNull()) {
      HELOG(kError, "(node {}) The task state {} with id {} is NULL.",
            CHI_CLIENT->node_id_, pool_name, task->ctx_.id_);
      task->SetModuleComplete();
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
        task->ctx_.id_,
        CHI_QM_CLIENT->admin_pool_id_,
        task->affinity_,
        global_containers,
        local_containers_pn);
    CHI_RPC->UpdateDomains(ops);
    std::vector<SubDomainId> containers =
        CHI_RPC->GetLocalContainers(task->ctx_.id_);
    // Print the created domain
//    CHI_RPC->PrintDomain(DomainId{task->ctx_.id_, SubDomainId::kContainerSet});
//    CHI_RPC->PrintSubdomainSet(containers);
    // Create the task state
    CHI_TASK_REGISTRY->CreateContainer(
        lib_name.c_str(),
        pool_name.c_str(),
        task->ctx_.id_,
        task, containers);
    if (task->root_) {
      // Broadcast the state creation to all nodes
      Container *exec = CHI_TASK_REGISTRY->GetStaticContainer(task->ctx_.id_);
      LPointer<Task> bcast;
      exec->NewCopyStart(Method::kCreate, task, bcast, true);
      auto *bcast_ptr = reinterpret_cast<CreateContainerTask *>(
          bcast.ptr_);
      bcast_ptr->task_node_ += 1;
      bcast_ptr->root_ = false;
      bcast_ptr->dom_query_ = bcast_ptr->affinity_;
      bcast_ptr->method_ = Method::kCreateContainer;
      bcast_ptr->pool_ = CHI_ADMIN->id_;
      ingress::MultiQueue *queue =
          CHI_QM_CLIENT->GetQueue(CHI_QM_CLIENT->admin_queue_id_);
      bcast->YieldInit(task);
      queue->Emplace(bcast->prio_, bcast->GetContainerId(), bcast.shm_);
      task->Wait(bcast);
      exec->Del(Method::kCreate, bcast.ptr_);
    }
    HILOG(kInfo, "(node {}) Created containers for task {}",
          CHI_RPC->node_id_, task->task_node_);
    task->SetModuleComplete();
  }
  void MonitorCreateContainer(u32 mode, CreateContainerTask *task,
                              RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<CreateContainerTask *>(
            replicas[0].ptr_);
        task->ctx_ = replica->ctx_;
      }
    }
  }

  /** Get task state id, fail if DNE */
  void GetPoolId(GetPoolIdTask *task, RunContext &rctx) {
    std::string pool_name = task->pool_name_.str();
    task->id_ = CHI_TASK_REGISTRY->GetPoolId(pool_name);
    task->SetModuleComplete();
  }
  void MonitorGetPoolId(u32 mode,
                             GetPoolIdTask *task,
                             RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<GetPoolIdTask *>(
            replicas[0].ptr_);
        task->id_ = replica->id_;
      }
    }
  }

  /** Destroy a task state */
  void DestroyContainer(DestroyContainerTask *task, RunContext &rctx) {
    CHI_TASK_REGISTRY->DestroyContainer(task->id_);
    task->SetModuleComplete();
  }
  void MonitorDestroyContainer(u32 mode,
                               DestroyContainerTask *task,
                               RunContext &rctx) {
  }

  /** Stop this runtime */
  void StopRuntime(StopRuntimeTask *task, RunContext &rctx) {
    if (task->root_) {
      HILOG(kInfo, "(node {}) Broadcasting runtime stop (task_node={})",
            CHI_RPC->node_id_, task->task_node_);
      CHI_ADMIN->AsyncStopRuntime(
          DomainQuery::GetGlobalBcast(), false);
    } else if (CHI_RPC->node_id_ == task->task_node_.root_.node_id_) {
      task->SetModuleComplete();
      HILOG(kInfo, "(node {}) Ignoring runtime stop (task_node={})",
            CHI_RPC->node_id_, task->task_node_);
      return;
    }
    HILOG(kInfo, "(node {}) Handling runtime stop (task_node={})",
          CHI_RPC->node_id_, task->task_node_);
    CHI_THALLIUM->StopThisDaemon();
    CHI_WORK_ORCHESTRATOR->FinalizeRuntime();
    task->SetModuleComplete();
  }
  void MonitorStopRuntime(u32 mode, StopRuntimeTask *task, RunContext &rctx) {
  }

  /** Set work orchestrator policy */
  void SetWorkOrchQueuePolicy(SetWorkOrchQueuePolicyTask *task, RunContext &rctx) {
    if (queue_sched_) {
      queue_sched_->SetModuleComplete();
    }
    if (queue_sched_ && !queue_sched_->IsComplete()) {
      return;
    }
    auto queue_sched = CHI_CLIENT->NewTask<ScheduleTask>(
        task->task_node_,
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
        task->policy_id_,
        250);
    queue_sched_ = queue_sched.ptr_;
    ingress::MultiQueue *queue = CHI_CLIENT->GetQueue(queue_id_);
    queue->Emplace(TaskPrio::kLowLatency, 0, queue_sched.shm_);
    task->SetModuleComplete();
  }
  void MonitorSetWorkOrchQueuePolicy(u32 mode,
                                     SetWorkOrchQueuePolicyTask *task,
                                     RunContext &rctx) {
  }

  /** Set work orchestration policy */
  void SetWorkOrchProcPolicy(SetWorkOrchProcPolicyTask *task,
                             RunContext &rctx) {
    if (proc_sched_) {
      proc_sched_->SetModuleComplete();
    }
    if (proc_sched_ && !proc_sched_->IsComplete()) {
      return;
    }
    auto proc_sched = CHI_CLIENT->NewTask<ScheduleTask>(
        task->task_node_,
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
        task->policy_id_,
        1000);
    proc_sched_ = proc_sched.ptr_;
    ingress::MultiQueue *queue = CHI_CLIENT->GetQueue(queue_id_);
    queue->Emplace(0, 0, proc_sched.shm_);
    task->SetModuleComplete();
  }
  void MonitorSetWorkOrchProcPolicy(u32 mode,
                                    SetWorkOrchProcPolicyTask *task,
                                    RunContext &rctx) {
  }

  /** Flush the runtime */
  void Flush(FlushTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorFlush(u32 mode, FlushTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<LPointer<Task>> &replicas = *rctx.replicas_;
        auto replica = reinterpret_cast<FlushTask *>(
            replicas[0].ptr_);
        task->work_done_ += replica->work_done_;
      }
    }
  }

  /** Get the domain size */
  void GetDomainSize(GetDomainSizeTask *task, RunContext &rctx) {
    task->dom_size_ =
        CHI_RPC->GetDomainSize(task->dom_id_);
    task->SetModuleComplete();
  }
  void MonitorGetDomainSize(u32 mode, GetDomainSizeTask *task, RunContext &rctx) {
  }

 public:
#include "chimaera_admin/chimaera_admin_lib_exec.h"
};

}  // namespace chi

CHI_TASK_CC(chi::Admin::Server, "chimaera_admin");
