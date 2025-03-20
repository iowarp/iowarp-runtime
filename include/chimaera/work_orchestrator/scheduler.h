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

#ifndef CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_SCHEDULER_H_
#define CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_SCHEDULER_H_

#include "chimaera/module_registry/task.h"

namespace chi {

/** The set of methods in the admin task */
struct SchedulerMethod : public TaskMethod {
  TASK_METHOD_T kSchedule = TaskMethod::kCustomBegin;
};

/** The task type used for scheduling */
struct ScheduleTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_INLINE explicit ScheduleTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit ScheduleTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const DomainQuery &dom_query, PoolId &pool_id, size_t period_ms)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kHighLatency;
    pool_ = pool_id;
    method_ = SchedulerMethod::kSchedule;
    task_flags_.SetBits(TASK_LONG_RUNNING | TASK_REMOTE_DEBUG_MARK);
    SetPeriodMs(period_ms);
    dom_query_ = dom_query;
  }
};

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_WORK_ORCHESTRATOR_SCHEDULER_H_
