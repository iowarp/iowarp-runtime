//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef CHI_TASKS_TASK_TEMPL_INCLUDE_TASK_NAME_TASK_NAME_TASKS_H_
#define CHI_TASKS_TASK_TEMPL_INCLUDE_TASK_NAME_TASK_NAME_TASKS_H_

#include "chimaera/chimaera_namespace.h"

namespace chi::TASK_NAME {

#include "TASK_NAME_methods.h"
CHI_NAMESPACE_INIT

/**
 * A task to create TASK_NAME
 * */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "TASK_NAME";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;

/** A task to destroy TASK_NAME */
typedef chi::Admin::DestroyContainerTask DestroyTask;

}  // namespace chi::TASK_NAME

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_TASK_NAME_TASK_NAME_TASKS_H_
