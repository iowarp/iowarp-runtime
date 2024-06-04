//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef HRUN_WORCH_PROC_ROUND_ROBIN_TASKS_H__
#define HRUN_WORCH_PROC_ROUND_ROBIN_TASKS_H__

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"
#include "chimaera/work_orchestrator/scheduler.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"

namespace chi::worch_proc_round_robin {

#include "chimaera/chimaera_namespace.h"

/** The set of methods in the worch task */
typedef SchedulerMethod Method;

/**
 * A task to create worch_proc_round_robin
 * */
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc) : CreateContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  CreateTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainQuery &dom_query,
                const DomainQuery &scope_query,
                const std::string &pool_name,
                const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, dom_query, scope_query,
                            pool_name, "worch_proc_round_robin", ctx) {
  }
};

/** A task to destroy worch_proc_round_robin */
using chi::Admin::DestroyContainerTask;
struct DestructTask : public DestroyContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               PoolId &pool_id)
      : DestroyContainerTask(alloc, task_node, dom_query, pool_id) {}
};

}  // namespace chi::worch_proc_round_robin

#endif  // HRUN_WORCH_PROC_ROUND_ROBIN_TASKS_H__
