//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
#define CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/module_registry/module.h"
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
                const DomainQuery &affinity,
                const std::string &pool_name,
                const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, dom_query, affinity,
                            pool_name, "worch_queue_round_robin", ctx) {
  }

  /** Destructor */
  HSHM_ALWAYS_INLINE
  ~CreateTask() {}

  /** Duplicate message */
  template<typename CreateTaskT = CreateContainerTask>
  void CopyStart(const CreateTaskT &other, bool deep) {
    BaseCopyStart(other, deep);
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    BaseSerializeStart(ar);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    BaseSerializeEnd(ar);
  }
};

/** A task to destroy worch_queue_round_robin */
typedef chi::Admin::DestroyContainerTask DestroyTask;

}  // namespace chi::worch_queue_round_robin

#endif  // CHI_WORCH_QUEUE_ROUND_ROBIN_TASKS_H_
