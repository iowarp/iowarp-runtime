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

#include <hermes_shm/util/singleton.h>

#include "chimaera/module_registry/task.h"

namespace chi {

/** Create the server-side API */
void Runtime::Create(std::string server_config_path) {
  hshm::ScopedMutex lock(lock_, 1);
  if (is_initialized_) {
    return;
  }
  is_being_initialized_ = true;
  ServerInit(std::move(server_config_path));
  is_initialized_ = true;
  is_being_initialized_ = false;
}

/** Initialize */
void Runtime::ServerInit(std::string server_config_path) {
  LoadServerConfig(server_config_path);
  RefreshNumGpus();
  InitSharedMemory();
  CHI_RPC->ServerInit(server_config_);
  CHI_THALLIUM->ServerInit(CHI_RPC);
  InitSharedMemoryGpu();
  // Create module registry
  CHI_MOD_REGISTRY->ServerInit(server_config_, CHI_RPC->node_id_,
                               header_->unique_);
  CHI_MOD_REGISTRY->RegisterModule("chimaera_chimaera_admin");
  CHI_MOD_REGISTRY->RegisterModule("chimaera_worch_queue_round_robin");
  CHI_MOD_REGISTRY->RegisterModule("chimaera_worch_proc_round_robin");
  CHI_MOD_REGISTRY->RegisterModule("chimaera_remote_queue");
  CHI_MOD_REGISTRY->RegisterModule("chimaera_bdev");
  // Queue manager + client must be initialized before Work Orchestrator
  CHI_QM->ServerInit(main_alloc_, CHI_RPC->node_id_, server_config_,
                     header_->queue_manager_);
  CHI_CLIENT->Create(server_config_path.c_str(), "", true);
  CHI_WORK_ORCHESTRATOR->ServerInit(server_config_);
  Admin::CreateTask *admin_create_task;
  Admin::CreatePoolTask *create_task;
  std::vector<UpdateDomainInfo> ops;
  std::vector<SubDomainId> containers;

  // Create the admin library
  CHI_CLIENT->MakePoolId();
  admin_create_task =
      CHI_CLIENT->AllocateTask<Admin::CreateTask>(HSHM_MCTX).ptr_;
  ops = CHI_RPC->CreateDefaultDomains(
      chi::ADMIN_POOL_ID, chi::ADMIN_POOL_ID,
      DomainQuery::GetGlobal(chi::SubDomain::kContainerSet, 0),
      CHI_RPC->hosts_.size(), 1);
  CHI_RPC->UpdateDomains(ops);
  containers = CHI_RPC->GetLocalContainers(chi::ADMIN_POOL_ID);
  CHI_MOD_REGISTRY->CreatePool("chimaera_chimaera_admin", "chimaera_admin",
                               chi::ADMIN_POOL_ID, admin_create_task,
                               containers);

  // Create the work orchestrator queue scheduling library
  PoolId queue_sched_id = CHI_CLIENT->MakePoolId();
  create_task = CHI_CLIENT->AllocateTask<Admin::CreatePoolTask>(HSHM_MCTX).ptr_;
  ops = CHI_RPC->CreateDefaultDomains(queue_sched_id, chi::ADMIN_POOL_ID,
                                      DomainQuery::GetLocalHash(0), 1, 1);
  CHI_RPC->UpdateDomains(ops);
  containers = CHI_RPC->GetLocalContainers(queue_sched_id);
  CHI_MOD_REGISTRY->CreatePool("chimaera_worch_queue_round_robin",
                               "worch_queue_round_robin", queue_sched_id,
                               create_task, containers);

  // Create the work orchestrator process scheduling library
  PoolId proc_sched_id = CHI_CLIENT->MakePoolId();
  create_task = CHI_CLIENT->AllocateTask<Admin::CreatePoolTask>(HSHM_MCTX).ptr_;
  ops = CHI_RPC->CreateDefaultDomains(proc_sched_id, chi::ADMIN_POOL_ID,
                                      DomainQuery::GetLocalHash(0), 1, 1);
  CHI_RPC->UpdateDomains(ops);
  containers = CHI_RPC->GetLocalContainers(proc_sched_id);
  CHI_MOD_REGISTRY->CreatePool("chimaera_worch_proc_round_robin",
                               "worch_proc_round_robin", proc_sched_id,
                               create_task, containers);

  // Set the work orchestrator queue scheduler
  CHI_ADMIN->SetWorkOrchQueuePolicyRN(HSHM_MCTX, DomainQuery::GetLocalHash(0),
                                      queue_sched_id);
  CHI_ADMIN->SetWorkOrchProcPolicyRN(HSHM_MCTX, DomainQuery::GetLocalHash(0),
                                     proc_sched_id);

  // Create the remote queue library
  remote_queue_.Create(HSHM_MCTX, DomainQuery::GetLocalHash(0),
                       DomainQuery::GetLocalHash(0), "remote_queue",
                       CreateContext{CHI_CLIENT->MakePoolId(), 1, 1});
  remote_created_ = true;
}

/** Initialize shared-memory between daemon and client */
void Runtime::InitSharedMemory() {
  // Create shared-memory allocator
  config::QueueManagerInfo &qm = server_config_->queue_manager_;
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  if (qm.shm_size_ == 0) {
    qm.shm_size_ = hipc::MemoryManager::GetDefaultBackendSize();
  }
  // Create general allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(hipc::MemoryBackendId::Get(0),
                                              qm.shm_size_, qm.shm_name_);
  main_alloc_ = mem_mngr->CreateAllocator<CHI_MAIN_ALLOC_T>(
      hipc::MemoryBackendId::Get(0), main_alloc_id_, sizeof(ChiShm));
  header_ = main_alloc_->GetCustomHeader<ChiShm>();
  mem_mngr->SetDefaultAllocator(main_alloc_);
  // Create separate data allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(
      hipc::MemoryBackendId::Get(1), qm.data_shm_size_, qm.data_shm_name_);
  data_alloc_ = mem_mngr->CreateAllocator<CHI_DATA_ALLOC_T>(
      hipc::MemoryBackendId::Get(1), data_alloc_id_, 0);
  // Create separate runtime data allocator
  mem_mngr->CreateBackend<hipc::PosixShmMmap>(
      hipc::MemoryBackendId::Get(2), qm.rdata_shm_size_, qm.rdata_shm_name_);
  rdata_alloc_ = mem_mngr->CreateAllocator<CHI_RDATA_ALLOC_T>(
      hipc::MemoryBackendId::Get(2), rdata_alloc_id_, 0);
}

/** Initialize shared-memory between daemon and client */
void Runtime::InitSharedMemoryGpu() {
  // Finish initializing shared memory
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  header_->node_id_ = CHI_RPC->node_id_;
  header_->unique_ = 0;
  header_->num_nodes_ = server_config_->rpc_.host_names_.size();

  // Create per-gpu allocator
#if defined(CHIMAERA_ENABLE_CUDA) or defined(CHIMAERA_ENABLE_ROCM)
  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
    hipc::MemoryBackendId backend_id = GetGpuCpuBackendId(gpu_id);
    hipc::AllocatorId alloc_id = GetGpuCpuAllocId(gpu_id);
    hipc::chararr name = GetGpuCpuAllocName(gpu_id);
    mem_mngr->CreateBackend<hipc::GpuShmMmap>(
        backend_id, server_config_->queue_manager_.gpu_md_shm_size_, name,
        gpu_id);
    gpu_alloc_[gpu_id] = mem_mngr->CreateAllocator<CHI_ALLOC_T>(
        backend_id, alloc_id, sizeof(ChiShm));
    ChiShm *header = main_alloc_->GetCustomHeader<ChiShm>();
    header->node_id_ = CHI_RPC->node_id_;
    header->unique_ =
        (((u64)1) << 32); // TODO(llogan): Make a separate unique for gpus
    header->num_nodes_ = server_config_->rpc_.host_names_.size();
  }

  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
    hipc::MemoryBackendId backend_id = GetGpuDataBackendId(gpu_id);
    hipc::AllocatorId alloc_id = GetGpuDataAllocId(gpu_id);
    hipc::chararr name = GetGpuDataAllocName(gpu_id);
    mem_mngr->CreateBackend<hipc::GpuMalloc>(
        backend_id, server_config_->queue_manager_.gpu_data_shm_size_, name,
        gpu_id);
    // gpu_data_alloc_[gpu_id] =
    // mem_mngr->CreateAllocator<CHI_DATA_GPU_ALLOC_T>(
    //     backend_id, alloc_id, 0);
    mem_mngr->CreateAllocatorGpu<CHI_DATA_GPU_ALLOC_T>(gpu_id, backend_id,
                                                       alloc_id, 0);
  }
#endif
}

/** Finalize Hermes explicitly */
void Runtime::Finalize() {}

/** Run the Hermes core Daemon */
void Runtime::RunDaemon() {
  thallium_.RunDaemon();
  HILOG(kInfo, "(node {}) Daemon is exiting", CHI_CLIENT->node_id_);
  exit(0);
}

} // namespace chi