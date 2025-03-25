//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef CHI_TASKS_TASK_TEMPL_INCLUDE_compressor_compressor_TASKS_H_
#define CHI_TASKS_TASK_TEMPL_INCLUDE_compressor_compressor_TASKS_H_

#include "chimaera/chimaera_namespace.h"

namespace chi::compressor {

#include "compressor_methods.h"
CHI_NAMESPACE_INIT

CHI_BEGIN(Create)
/** A task to create compressor */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "chimaera_compressor";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;
CHI_END(Create)

CHI_BEGIN(Destroy)
/** A task to destroy compressor */
typedef chi::Admin::DestroyContainerTask DestroyTask;
CHI_END(Destroy)

CHI_AUTOGEN_METHODS  // keep at class bottom

}  // namespace chi::compressor

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_compressor_compressor_TASKS_H_
