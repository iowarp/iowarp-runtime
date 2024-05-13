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

namespace chm {

/** Create the server-side API */
Runtime* Runtime::Create(std::string server_config_path) {
  hshm::ScopedMutex lock(lock_, 1);
  if (is_initialized_) {
    return this;
  }
  mode_ = HrunMode::kServer;
  is_being_initialized_ = true;
  ServerInit(std::move(server_config_path));
  is_initialized_ = true;
  is_being_initialized_ = false;
  return this;
}

/** Initialize */
void Runtime::ServerInit(std::string server_config_path) {
  LoadServerConfig(server_config_path);
  HILOG(kInfo, "Initializing shared memory")
  InitSharedMemory();
  HILOG(kInfo, "Initializing RPC")
  rpc_.ServerInit(&server_config_);
  HILOG(kInfo, "Initializing thallium")
  thallium_.ServerInit(&rpc_);
  HILOG(kInfo, "Initializing queues + workers")
  header_->node_id_ = rpc_.node_id_;
  header_->unique_ = 0;
  header_->num_nodes_ = server_config_.rpc_.host_names_.size();
  task_registry_.ServerInit(&server_config_, rpc_.node_id_, header_->unique_);
  // Queue manager + client must be initialized before Work Orchestrator
  queue_manager_.ServerInit(main_alloc_,
                            rpc_.node_id_,
                            &server_config_,
                            header_->queue_manager_);
  CHM_CLIENT->Create(server_config_path, "", true);
  HERMES_THREAD_MODEL->SetThreadModel(hshm::ThreadType::kArgobots);
  work_orchestrator_.ServerInit(&server_config_, queue_manager_);
  hipc::mptr<Admin::CreateTaskStateTask> admin_task;
  u32 max_lanes = CHM_RUNTIME->queue_manager_.max_lanes_;
  size_t max_workers = server_config_.wo_.max_dworkers_ +
                       server_config_.wo_.max_oworkers_;
  std::vector<UpdateDomainInfo> ops;
  std::vector<SubDomainId> lanes;

  // Create the admin library
  CHM_CLIENT->MakeTaskStateId();
  admin_task = hipc::make_mptr<Admin::CreateTaskStateTask>();
  task_registry_.RegisterTaskLib("chimaera_admin");
  ops = CHM_RPC->CreateDefaultDomains(
      CHM_QM_CLIENT->admin_task_state_,
      CHM_QM_CLIENT->admin_task_state_,
      DomainQuery::GetGlobal(chm::SubDomainId::kLaneSet, 0),
      CHM_RPC->hosts_.size(), 1);
  CHM_RPC->UpdateDomains(ops);
  lanes = CHM_RPC->GetLocalLanes(CHM_QM_CLIENT->admin_task_state_);
  task_registry_.CreateTaskState(
      "chimaera_admin",
      "chimaera_admin",
      CHM_QM_CLIENT->admin_task_state_,
      admin_task.get(),
      lanes);

  // Create the work orchestrator queue scheduling library
  TaskStateId queue_sched_id = CHM_CLIENT->MakeTaskStateId();
  admin_task = hipc::make_mptr<Admin::CreateTaskStateTask>();
  task_registry_.RegisterTaskLib("worch_queue_round_robin");
  ops = CHM_RPC->CreateDefaultDomains(
      queue_sched_id,
      CHM_QM_CLIENT->admin_task_state_,
      DomainQuery::GetGlobal(chm::SubDomainId::kLocalLaneSet, 0),
      1, 1);
  CHM_RPC->UpdateDomains(ops);
  lanes = CHM_RPC->GetLocalLanes(queue_sched_id);
  task_registry_.CreateTaskState(
      "worch_queue_round_robin",
      "worch_queue_round_robin",
      queue_sched_id,
      admin_task.get(),
      lanes);
  TaskState *state = task_registry_.GetAnyTaskState(queue_sched_id);

  // Initially schedule queues to workers
  auto queue_task = CHM_CLIENT->NewTask<ScheduleTask>(
      CHM_CLIENT->MakeTaskNodeId(),
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      queue_sched_id);
  state->Run(queue_task->method_,
             queue_task.ptr_,
             queue_task->rctx_);
  CHM_CLIENT->DelTask(queue_task);

  // Create the work orchestrator process scheduling library
  TaskStateId proc_sched_id = CHM_CLIENT->MakeTaskStateId();
  admin_task = hipc::make_mptr<Admin::CreateTaskStateTask>();
  task_registry_.RegisterTaskLib("worch_proc_round_robin");
  ops = CHM_RPC->CreateDefaultDomains(
      proc_sched_id,
      CHM_QM_CLIENT->admin_task_state_,
      DomainQuery::GetGlobal(chm::SubDomainId::kLocalLaneSet, 0),
      1, 1);
  CHM_RPC->UpdateDomains(ops);
  lanes = CHM_RPC->GetLocalLanes(proc_sched_id);
  task_registry_.CreateTaskState(
      "worch_proc_round_robin",
      "worch_proc_round_robin",
      proc_sched_id,
      admin_task.get(),
      lanes);

  // Set the work orchestrator queue scheduler
  CHM_ADMIN->SetWorkOrchQueuePolicyRoot(
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      queue_sched_id);
  CHM_ADMIN->SetWorkOrchProcPolicyRoot(
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      proc_sched_id);

  // Create the remote queue library
  task_registry_.RegisterTaskLib("remote_queue");
  ops = CHM_RPC->CreateDefaultDomains(
      proc_sched_id,
      CHM_QM_CLIENT->admin_task_state_,
      DomainQuery::GetGlobal(chm::SubDomainId::kLocalLaneSet, 0),
      max_lanes, 1);
  CHM_RPC->UpdateDomains(ops);
  lanes = CHM_RPC->GetLocalLanes(proc_sched_id);
  remote_queue_.CreateRoot(
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      "remote_queue",
      CreateContext{CHM_CLIENT->MakeTaskStateId(), max_lanes, 1});
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
        CHM_CLIENT->node_id_)
  HRUN_WORK_ORCHESTRATOR->Join();
  HILOG(kInfo, "(node {}) Daemon is exiting",
        CHM_CLIENT->node_id_)
}

/** Stop the Hermes core Daemon */
void Runtime::StopDaemon() {
  HRUN_WORK_ORCHESTRATOR->FinalizeRuntime();
}

}  // namespace chm

/** Runtime singleton */
DEFINE_SINGLETON_CC(chm::Runtime)