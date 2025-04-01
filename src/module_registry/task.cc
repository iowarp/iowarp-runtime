//
// Created by llogan on 7/22/24.
//
#include "chimaera/module_registry/task.h"
#ifdef CHIMAERA_RUNTIME
#include <thallium.hpp>

#include "chimaera/work_orchestrator/work_orchestrator.h"
#endif

namespace chi {

HSHM_CROSS_FUN
void Task::Wait(chi::IntFlag flags) {
#if defined(CHIMAERA_RUNTIME) and defined(HSHM_IS_HOST)
  Task *parent_task = CHI_CUR_TASK;
  if (this != parent_task) {
    parent_task->Wait(this, flags);
  } else {
    SpinWaitCo(flags);
  }
#else
  SpinWait(flags);
#endif
}

}  // namespace chi