//
// Created by llogan on 7/22/24.
//
#include "chimaera/module_registry/task.h"

#include "chimaera/api/chimaera_client.h"
#ifdef CHIMAERA_RUNTIME
#include <thallium.hpp>

#include "chimaera/work_orchestrator/work_orchestrator.h"
#endif

namespace chi {

HSHM_CROSS_FUN
void Task::SetBlocked(int count) {
  int ret = rctx_.block_count_.fetch_add(count) + count;
  if (ret != 0) {
    task_flags_.SetBits(TASK_BLOCKED | TASK_YIELDED);
  } else {
    HELOG(kFatal, "(node {}) block count should never be negative here: {}",
          CHI_CLIENT->node_id_, ret);
  }
}

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