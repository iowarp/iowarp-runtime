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
template<bool FROM_REMOTE>
HSHM_INLINE
LPointer<char> Client::AllocateBufferSafe(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, size_t size) {
  LPointer<char> p;
  while (true) {
    try {
      p = alloc->AllocateLocalPtr<char>(alloc.ctx_, size);
    } catch (hshm::Error &e) {
      p.shm_.SetNull();
    }
    if (!p.shm_.IsNull()) {
      break;
    }
    if constexpr(FROM_REMOTE) {
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
}

/** Schedule a task locally */
#ifdef CHIMAERA_RUNTIME
template<typename TaskT>
void Client::ScheduleTaskRuntime(Task *parent_task,
                                 LPointer<TaskT> &task,
                                 const QueueId &ig_queue_id) {
  task->YieldInit(parent_task);
  CHI_CUR_WORKER->active_.push(task);
}
#endif

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_CLIENT_CHI_CLIENT_H_
