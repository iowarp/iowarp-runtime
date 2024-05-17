//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef HRUN_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
#define HRUN_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/work_orchestrator/scheduler.h"
#include "chimaera/queue_manager/queue_manager_client.h"

namespace chi::worch_queue_round_robin {

#include "chimaera/chimaera_namespace.h"

/** The set of methods in the worch task */
typedef SchedulerMethod Method;

/**
 * A task to create worch_queue_round_robin
 * */
using chi::Admin::CreateTaskStateTask;
struct CreateTask : public CreateTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc) : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  CreateTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainQuery &dom_query,
                const DomainQuery &scope_query,
                const std::string &state_name,
                const CreateContext &ctx)
      : CreateTaskStateTask(alloc, task_node, dom_query, scope_query,
                            state_name, "worch_queue_round_robin", ctx) {
  }

  /** Destructor */
  HSHM_ALWAYS_INLINE
  ~CreateTask() {}
};

/** A task to destroy worch_queue_round_robin */
using chi::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               TaskStateId &state_id,
               const DomainQuery &dom_query)
      : DestroyTaskStateTask(alloc, task_node, dom_query, state_id) {}
};

}  // namespace chi::worch_queue_round_robin

#endif  // HRUN_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
