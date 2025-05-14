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

#ifndef CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_
#define CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_

#include "chimaera_client_defn.h"

namespace chi {

/** Initialize the client (GPU) */
#if defined(CHIMAERA_ENABLE_ROCM) || defined(CHIMAERA_ENABLE_CUDA)
HSHM_GPU_KERNEL static void CreateClientKernel(hipc::AllocatorId cpu_alloc,
                                               hipc::AllocatorId data_alloc) {
  CHI_CLIENT->CreateOnGpu(cpu_alloc, data_alloc);
}

template <int NOTHING>
HSHM_GPU_FUN void Client::CreateOnGpu(hipc::AllocatorId cpu_alloc,
                                      hipc::AllocatorId data_alloc) {
#ifdef HSHM_IS_GPU
  main_alloc_ = HSHM_MEMORY_MANAGER->GetAllocator<CHI_MAIN_ALLOC_T>(cpu_alloc);
  printf("Chimaera GPU Client: %p\n", this);
  printf("Memory Manager: %p\n", HSHM_MEMORY_MANAGER);
  printf("Main Alloc: %p\n", main_alloc_);
  data_alloc_ = HSHM_MEMORY_MANAGER->GetAllocator<CHI_DATA_ALLOC_T>(data_alloc);
  printf("Data Alloc: %p (%d.%d)\n", data_alloc_, data_alloc.bits_.major_,
         data_alloc.bits_.minor_);
  rdata_alloc_ = nullptr;
  HSHM_MEMORY_MANAGER->SetDefaultAllocator(main_alloc_);
  header_ = main_alloc_->GetCustomHeader<ChiShm>();
  unique_ = &header_->unique_;
  node_id_ = header_->node_id_;
  auto *p1 = header_->queue_manager_.queue_map_.get();
  CHI_QM->ClientInit(main_alloc_, header_->queue_manager_, header_->node_id_);
  auto *p2 = CHI_QM->queue_map_;
  is_initialized_ = true;
  is_being_initialized_ = false;
#endif
}
#endif

/**
 * Creates the CHI_CLIENT on the GPU
 * This needs to be templated because each module using GPU
 * must call it to initialize singleton. The singleton is not
 * propogated across shared objects. CUDA and ROCm are very picky.
 */
template <int NOTHING>
void Client::CreateClientOnHostForGpu() {
#if defined(CHIMAERA_ENABLE_ROCM) || defined(CHIMAERA_ENABLE_CUDA)
  for (int gpu_id = 0; gpu_id < ngpu_; ++gpu_id) {
    hshm::GpuApi::SetDevice(gpu_id);
    CreateClientKernel<<<1, 1>>>(GetGpuCpuAllocId(gpu_id),
                                 GetGpuDataAllocId(gpu_id));
    hshm::GpuApi::Synchronize();
  }
#endif
}

/** Allocate a buffer */
template <bool FROM_REMOTE>
HSHM_INLINE_CROSS_FUN FullPtr<char> Client::AllocateBufferSafe(
    const hipc::CtxAllocator<CHI_DATA_ALLOC_T> &alloc, size_t size) {
  FullPtr<char> p(FullPtr<char>::GetNull());
  // HILOG(kInfo, "(node {}) Beginning to allocate {} from {}",
  //       CHI_CLIENT->node_id_, size, alloc->GetId());
  if (size == 0) {
    return FullPtr<char>::GetNull();
  }
  while (true) {
#ifdef HSHM_IS_HOST
    try {
      p = alloc->AllocateLocalPtr<char>(alloc.ctx_, size);
    } catch (hshm::Error &e) {
      p.shm_.SetNull();
    }
#else
    p = alloc->AllocateLocalPtr<char>(alloc.ctx_, size);
#endif
    if (!p.shm_.IsNull()) {
      break;
    }
#ifdef CHIMAERA_RUNTIME
    if constexpr (FROM_REMOTE) {
      HSHM_THREAD_MODEL->Yield();
    } else {
      Task *task = CHI_CUR_TASK;
      task->Yield();
    }
#else
    HSHM_THREAD_MODEL->Yield();
#endif

    // HILOG(kInfo, "(node {}) Trying {} from {}", CHI_CLIENT->node_id_, size,
    //       alloc->GetId());
  }
  // HILOG(kInfo, "(node {}) Allocated {} from {}", CHI_CLIENT->node_id_, size,
  //       alloc->GetId());
  return p;
}

/** Send a task to the runtime */
template <typename TaskT>
HSHM_INLINE_CROSS_FUN void Client::ScheduleTask(Task *parent_task,
                                                const FullPtr<TaskT> &task) {
#ifndef CHIMAERA_RUNTIME
  chi::ingress::MultiQueue *queue = CHI_CLIENT->GetQueue(chi::PROCESS_QUEUE_ID);
  // HILOG(kInfo, "Scheduling task (client, prior, node={}): {} pool={} dom={}",
  //       node_id_, task->task_node_, task->pool_, task->dom_query_);
  queue->Emplace(chi::TaskPrioOpt::kLowLatency,
                 hshm::hash<chi::DomainQuery>{}(task->dom_query_), task.shm_);
  // HILOG(kInfo, "Scheduling task (client, node={}): {} pool={} dom={}",
  // node_id_,
  //       task->task_node_, task->pool_, task->dom_query_);
#else
  // HILOG(kInfo, "Scheduling task (runtime, prior, node={}): {} pool={}
  // dom={}",
  //       node_id_, task->task_node_, task->pool_, task->dom_query_);
  task->YieldInit(parent_task);
  Worker *cur_worker = CHI_CUR_WORKER;
  if (cur_worker->IsNull()) {
    cur_worker = &CHI_WORK_ORCHESTRATOR->GetWorker(0);
    cur_worker->active_.GetFail().push(task.template Cast<Task>());
  } else {
    cur_worker->active_.push(task);
  }
  // HILOG(kInfo, "Scheduling task (runtime, node={}): {} pool={} dom={}",
  //       node_id_, task->task_node_, task->pool_, task->dom_query_);
#endif
}

/** Allocate + send a task to the runtime */
template <typename TaskT, typename... Args>
HSHM_INLINE_CROSS_FUN hipc::FullPtr<TaskT> Client::ScheduleNewTask(
    const hipc::MemContext &mctx, const PoolId &pool_id, Args &&...args) {
#ifndef CHIMAERA_RUNTIME
  TaskNode task_node = CHI_CLIENT->MakeTaskNodeId();
  chi::Task *parent_task = nullptr;
#else
  chi::Task *parent_task = CHI_CUR_TASK;
  TaskNode task_node;
  if (parent_task) {
    task_node = parent_task->task_node_ + 1;
  } else {
    task_node = CHI_CLIENT->MakeTaskNodeId();
  }
#endif
  return ScheduleNewTask<TaskT>(mctx, parent_task, task_node, pool_id,
                                std::forward<Args>(args)...);
}

/** Allocate + send a task to the runtime */
template <typename TaskT, typename... Args>
HSHM_INLINE_CROSS_FUN hipc::FullPtr<TaskT> Client::ScheduleNewTask(
    const hipc::MemContext &mctx, chi::Task *parent_task,
    const TaskNode &task_node, const PoolId &pool_id, Args &&...args) {
  FullPtr<TaskT> task = CHI_CLIENT->NewTask<TaskT>(mctx, task_node, pool_id,
                                                   std::forward<Args>(args)...);
  CHI_CLIENT->ScheduleTask(parent_task, task);
  return task;
}

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_
