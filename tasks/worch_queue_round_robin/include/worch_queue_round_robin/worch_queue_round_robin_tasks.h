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

namespace chm::worch_queue_round_robin {

#include "chimaera/chimaera_namespace.h"

/** The set of methods in the worch task */
typedef SchedulerMethod Method;

/**
 * A task to create worch_queue_round_robin
 * */
using chm::Admin::CreateTaskStateTask;
struct ConstructTask : public CreateTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ConstructTask(hipc::Allocator *alloc) : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  ConstructTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainId &domain_id,
                const std::string &state_name,
                const TaskStateId &id,
                const std::vector<PriorityInfo> &queue_info)
      : CreateTaskStateTask(alloc, task_node, domain_id, state_name,
                            "worch_queue_round_robin", id, queue_info) {
  }

  /** Destructor */
  HSHM_ALWAYS_INLINE
  ~ConstructTask() {}
};

/** A task to destroy worch_queue_round_robin */
using chm::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               TaskStateId &state_id,
               const DomainId &domain_id)
      : DestroyTaskStateTask(alloc, task_node, domain_id, state_id) {}

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

}  // namespace chm::worch_queue_round_robin

#endif  // HRUN_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
