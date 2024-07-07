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

#include "hermes_shm/util/singleton.h"
#include "chimaera/api/chimaera_runtime.h"

namespace chi {

/** Create lanes for the TaskLib */
void TaskLib::CreateLaneGroup(const LaneGroupId &id, u32 count, u32 flags) {
  lane_groups_.emplace(id, LaneGroup());
  LaneGroup &lane_group = lane_groups_[id];
  lane_group.lanes_.reserve(count);
  for (u32 i = 0; i < count; ++i) {
    ingress::Lane *ig_lane;
    u32 lane_prio;
    if (flags & QUEUE_LOW_LATENCY) {
      // Find least-burdened dedicated worker
      lane_prio = TaskPrio::kLowLatency;
    } else {
      lane_prio = TaskPrio::kHighLatency;
    }
    ig_lane = CHI_WORK_ORCHESTRATOR->GetLeastLoadedIngressLane(
        lane_prio);
    Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(ig_lane->worker_id_);
    worker.load_ += 1;
    lane_group.lanes_.emplace_back(Lane{lane_counter_++,
                                        ig_lane->id_,
                                        ig_lane->worker_id_});
  }
}

/** Create a container */
bool TaskRegistry::CreateContainer(const char *lib_name,
                                   const char *pool_name,
                                   const PoolId &pool_id,
                                   Admin::CreateContainerTask *task,
                                   const std::vector<SubDomainId> &containers) {
  // Ensure pool_id is not NULL
  if (pool_id.IsNull()) {
    HELOG(kError, "The task state ID cannot be null");
    task->SetModuleComplete();
    return false;
  }
//    HILOG(kInfo, "(node {}) Creating an instance of {} with name {}",
//          CHI_CLIENT->node_id_, lib_name, pool_name)

  // Find the task library to instantiate
  auto it = libs_.find(lib_name);
  if (it == libs_.end()) {
    HELOG(kError, "Could not find the task lib: {}", lib_name);
    task->SetModuleComplete();
    return false;
  }
  TaskLibInfo &info = it->second;

  // Create partitioned state
  pools_[pool_id].lib_name_ = lib_name;
  std::unordered_map<ContainerId, Container*> &states =
      pools_[pool_id].containers_;
  for (const SubDomainId &container_id : containers) {
    // Don't repeat if state exists
    if (states.find(container_id.minor_) != states.end()) {
      continue;
    }

    // Allocate the state
    Container *exec = info.new_state_(&pool_id, pool_name);
    if (!exec) {
      HELOG(kError, "Could not create the task state: {}", pool_name);
      task->SetModuleComplete();
      return false;
    }

    // Add the state to the registry
    exec->id_ = pool_id;
    exec->name_ = pool_name;
    exec->container_id_ = container_id.minor_;
    ScopedRwWriteLock lock(lock_, 0);
    pools_[pool_id].containers_[exec->container_id_] = exec;

    // Construct the state
    task->ctx_.id_ = pool_id;
    exec->Run(TaskMethod::kCreate, task, task->rctx_);
    task->UnsetModuleComplete();
  }
  HILOG(kInfo, "(node {})  Created an instance of {} with pool name {} "
               "and pool ID {} ({} containers)",
        CHI_CLIENT->node_id_, lib_name, pool_name,
        pool_id, containers.size());
  return true;
}

/** Schedule a task locally */
void Client::ScheduleTaskRuntime(Task *parent_task,
                                 LPointer<Task> &task,
                                 const QueueId &queue_id) {
  std::vector<ResolvedDomainQuery> resolved =
      CHI_RPC->ResolveDomainQuery(task->pool_, task->dom_query_, false);
  ingress::MultiQueue *queue = GetQueue(queue_id);
  DomainQuery dom_query = resolved[0].dom_;
  if (resolved.size() == 1 && resolved[0].node_ == CHI_RPC->node_id_ &&
      dom_query.flags_.All(DomainQuery::kLocal | DomainQuery::kId)) {
    // Determine the lane the task should map to within container
    ContainerId container_id = dom_query.sel_.id_;
    Container *exec = CHI_TASK_REGISTRY->GetContainer(task->pool_,
                                                      container_id);
    chi::Lane *chi_lane = exec->Route(task.ptr_);

    // Get the worker queue for the lane
    ingress::LaneGroup &lane_group = queue->GetGroup(task->prio_);
    u32 ig_lane_id = chi_lane->worker_id_ % lane_group.num_lanes_;
    ingress::Lane &ig_lane = lane_group.GetLane(ig_lane_id);
    ig_lane.emplace(task.shm_);
  } else {
    // Place on whatever queue...
    queue->Emplace(task->prio_,
                   std::hash<chi::DomainQuery>{}(task->dom_query_),
                   task.shm_);
  }
}

/** Create the server-side API */
Runtime* Runtime::Create(std::string server_config_path) {
  hshm::ScopedMutex lock(lock_, 1);
  if (is_initialized_) {
    return this;
  }
  is_being_initialized_ = true;
  ServerInit(std::move(server_config_path));
  is_initialized_ = true;
  is_being_initialized_ = false;
  return this;
}

/** Initialize */
void Runtime::ServerInit(std::string server_config_path) {
  LoadServerConfig(server_config_path);
  // HILOG(kInfo, "Initializing shared memory")
  InitSharedMemory();
  // HILOG(kInfo, "Initializing RPC")
  CHI_RPC->ServerInit(&server_config_);
  // HILOG(kInfo, "Initializing thallium")
  thallium_.ServerInit(CHI_RPC);
  // HILOG(kInfo, "Initializing queues + workers")
  header_->node_id_ = CHI_RPC->node_id_;
  header_->unique_ = 0;
  header_->num_nodes_ = server_config_.rpc_.host_names_.size();
  task_registry_.ServerInit(&server_config_, CHI_RPC->node_id_, header_->unique_);
  // Queue manager + client must be initialized before Work Orchestrator
  queue_manager_.ServerInit(main_alloc_,
                            CHI_RPC->node_id_,
                            &server_config_,
                            header_->queue_manager_);
  CHI_CLIENT->Create(server_config_path, "", true);
  HERMES_THREAD_MODEL->SetThreadModel(hshm::ThreadType::kArgobots);
  work_orchestrator_.ServerInit(&server_config_, queue_manager_);
  hipc::mptr<Admin::CreateTask> admin_create_task;
  hipc::mptr<Admin::CreateContainerTask> create_task;
  u32 max_containers_pn = CHI_RUNTIME->queue_manager_.max_containers_pn_;
  size_t max_workers = server_config_.wo_.max_dworkers_ +
                       server_config_.wo_.max_oworkers_;
  std::vector<UpdateDomainInfo> ops;
  std::vector<SubDomainId> containers;

  // Create the admin library
  CHI_CLIENT->MakePoolId();
  admin_create_task = hipc::make_mptr<Admin::CreateTask>();
  task_registry_.RegisterTaskLib("chimaera_admin");
  ops = CHI_RPC->CreateDefaultDomains(
      CHI_QM_CLIENT->admin_pool_id_,
      CHI_QM_CLIENT->admin_pool_id_,
      DomainQuery::GetGlobal(chi::SubDomainId::kContainerSet, 0),
      CHI_RPC->hosts_.size(), 1);
  CHI_RPC->UpdateDomains(ops);
  containers =
      CHI_RPC->GetLocalContainers(CHI_QM_CLIENT->admin_pool_id_);
  task_registry_.CreateContainer(
      "chimaera_admin",
      "chimaera_admin",
      CHI_QM_CLIENT->admin_pool_id_,
      admin_create_task.get(),
      containers);

  // Create the work orchestrator queue scheduling library
  PoolId queue_sched_id = CHI_CLIENT->MakePoolId();
  create_task = hipc::make_mptr<Admin::CreateContainerTask>();
  task_registry_.RegisterTaskLib("worch_queue_round_robin");
  ops = CHI_RPC->CreateDefaultDomains(
      queue_sched_id,
      CHI_QM_CLIENT->admin_pool_id_,
      DomainQuery::GetGlobal(chi::SubDomainId::kLocalContainers, 0),
      1, 1);
  CHI_RPC->UpdateDomains(ops);
  containers = CHI_RPC->GetLocalContainers(queue_sched_id);
  task_registry_.CreateContainer(
      "worch_queue_round_robin",
      "worch_queue_round_robin",
      queue_sched_id,
      create_task.get(),
      containers);
  Container *state = task_registry_.GetStaticContainer(queue_sched_id);

  // Initially schedule queues to workers
  auto queue_task = CHI_CLIENT->NewTask<ScheduleTask>(
      CHI_CLIENT->MakeTaskNodeId(),
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      queue_sched_id,
      250);
  state->Run(queue_task->method_,
             queue_task.ptr_,
             queue_task->rctx_);
  CHI_CLIENT->DelTask(queue_task);

  // Create the work orchestrator process scheduling library
  PoolId proc_sched_id = CHI_CLIENT->MakePoolId();
  create_task = hipc::make_mptr<Admin::CreateContainerTask>();
  task_registry_.RegisterTaskLib("worch_proc_round_robin");
  ops = CHI_RPC->CreateDefaultDomains(
      proc_sched_id,
      CHI_QM_CLIENT->admin_pool_id_,
      DomainQuery::GetGlobal(chi::SubDomainId::kLocalContainers, 0),
      1, 1);
  CHI_RPC->UpdateDomains(ops);
  containers = CHI_RPC->GetLocalContainers(proc_sched_id);
  task_registry_.CreateContainer(
      "worch_proc_round_robin",
      "worch_proc_round_robin",
      proc_sched_id,
      create_task.get(),
      containers);

//  CHI_RPC->PrintDomainResolution(
//      proc_sched_id,
//      DomainQuery::GetGlobal(chi::SubDomainId::kContainerSet, 0));
//  CHI_RPC->PrintDomainResolution(
//      proc_sched_id,
//      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0));
//  HELOG(kFatal, "End here...");

  // Set the work orchestrator queue scheduler
  CHI_ADMIN->SetWorkOrchQueuePolicyRoot(
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      queue_sched_id);
  CHI_ADMIN->SetWorkOrchProcPolicyRoot(
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      proc_sched_id);

  // Create the remote queue library
  task_registry_.RegisterTaskLib("remote_queue");
  remote_queue_.CreateRoot(
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      "remote_queue",
      CreateContext{CHI_CLIENT->MakePoolId(), 1, max_containers_pn});
  remote_created_ = true;
}

/** Initialize shared-memory between daemon and client */
void Runtime::InitSharedMemory() {
  // Create shared-memory allocator
  config::QueueManagerInfo &qm = server_config_.queue_manager_;
  auto mem_mngr = HERMES_MEMORY_MANAGER;
  if (qm.shm_size_ == 0) {
    qm.shm_size_ =
        hipc::MemoryManager::GetDefaultBackendSize();
  }
  // Create general allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(
      qm.shm_size_,
      qm.shm_name_);
  main_alloc_ =
      mem_mngr->CreateAllocator<hipc::ScalablePageAllocator>(
          qm.shm_name_,
          main_alloc_id_,
          sizeof(HrunShm));
  header_ = main_alloc_->GetCustomHeader<HrunShm>();
  // Create separate data allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(
      qm.data_shm_size_,
      qm.data_shm_name_);
  data_alloc_ =
      mem_mngr->CreateAllocator<hipc::ScalablePageAllocator>(
          qm.data_shm_name_,
          data_alloc_id_, 0);
  // Create separate runtime data allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(
      qm.rdata_shm_size_,
      qm.rdata_shm_name_);
  rdata_alloc_ =
      mem_mngr->CreateAllocator<hipc::ScalablePageAllocator>(
          qm.rdata_shm_name_,
          rdata_alloc_id_, 0);
}

/** Finalize Hermes explicitly */
void Runtime::Finalize() {
}

/** Run the Hermes core Daemon */
void Runtime::RunDaemon() {
  thallium_.RunDaemon();
  HILOG(kInfo, "(node {}) Finishing up last requests",
        CHI_CLIENT->node_id_)
  CHI_WORK_ORCHESTRATOR->Join();
  HILOG(kInfo, "(node {}) Daemon is exiting",
        CHI_CLIENT->node_id_)
}

/** Stop the Hermes core Daemon */
void Runtime::StopDaemon() {
  CHI_WORK_ORCHESTRATOR->FinalizeRuntime();
}

}  // namespace chi

/** Runtime singleton */
DEFINE_SINGLETON_CC(chi::Runtime)
DEFINE_SINGLETON_CC(chi::RpcContext)