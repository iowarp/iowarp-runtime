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

/** Allocate a buffer */
template <bool FROM_REMOTE>
HSHM_INLINE_CROSS_FUN FullPtr<char> Client::AllocateBufferSafe(
    const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, size_t size) {
#ifdef HSHM_IS_HOST
  FullPtr<char> p;
  while (true) {
    try {
      p = alloc->AllocateLocalPtr<char>(alloc.ctx_, size);
    } catch (hshm::Error &e) {
      p.shm_.SetNull();
    }
    if (!p.shm_.IsNull()) {
      break;
    }
    if constexpr (FROM_REMOTE) {
      Task::StaticYieldFactory<TASK_YIELD_ABT>();
    }
#ifdef CHIMAERA_RUNTIME
    Task *task = CHI_CUR_TASK;
    task->Yield();
#else
    Task::StaticYieldFactory<TASK_YIELD_STD>();
#endif
  }
  return p;
#else
  return FullPtr<char>();
#endif
}

/** Send a task to the runtime */
template <typename TaskT>
HSHM_INLINE_CROSS_FUN void Client::ScheduleTask(Task *parent_task,
                                                const FullPtr<TaskT> &task) {
#ifndef CHIMAERA_RUNTIME
  chi::ingress::MultiQueue *queue =
      CHI_CLIENT->GetQueue(CHI_QM->process_queue_id_);
  HILOG(kInfo, "Scheduling task (client, prior, node={}): {} pool={} dom={}",
        node_id_, task->task_node_, task->pool_, task->dom_query_);
  queue->Emplace(chi::TaskPrioOpt::kLowLatency,
                 hshm::hash<chi::DomainQuery>{}(task->dom_query_), task.shm_);
  HILOG(kInfo, "Scheduling task (client, node={}): {} pool={} dom={}", node_id_,
        task->task_node_, task->pool_, task->dom_query_);
#else
  HILOG(kInfo, "Scheduling task (runtime, prior, node={}): {} pool={} dom={}",
        node_id_, task->task_node_, task->pool_, task->dom_query_);
  task->YieldInit(parent_task);
  Worker *cur_worker = CHI_CUR_WORKER;
  if (!cur_worker) {
    cur_worker = &CHI_WORK_ORCHESTRATOR->GetWorker(0);
  }
  cur_worker->active_.push(task);
  HILOG(kInfo, "Scheduling task (runtime, node={}): {} pool={} dom={}",
        node_id_, task->task_node_, task->pool_, task->dom_query_);
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
