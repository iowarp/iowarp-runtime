//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
#define CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_

#include "chimaera/chimaera_namespace.h"
#include "chimaera/work_orchestrator/scheduler.h"

namespace chi::worch_queue_round_robin {

CHI_NAMESPACE_INIT

/** The set of methods in the worch task */
typedef SchedulerMethod Method;

/**
 * A task to create worch_queue_round_robin
 * */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "chimaera_worch_queue_round_robin";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;

/** A task to destroy worch_queue_round_robin */
typedef chi::Admin::DestroyContainerTask DestroyTask;

CHI_AUTOGEN_METHODS  // keep at class bottom

}  // namespace chi::worch_queue_round_robin

#endif  // CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
