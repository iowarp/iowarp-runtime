//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef CHI_WORCH_PROC_ROUND_ROBIN_TASKS_H__
#define CHI_WORCH_PROC_ROUND_ROBIN_TASKS_H__

#include "chimaera/chimaera_namespace.h"
#include "chimaera/work_orchestrator/scheduler.h"

namespace chi::worch_proc_round_robin {

CHI_NAMESPACE_INIT

/** The set of methods in the worch task */
typedef SchedulerMethod Method;

/**
 * A task to create worch_proc_round_robin
 * */
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(const hipc::CtxAllocator<HSHM_DEFAULT_ALLOC> &alloc) : CreateContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE
  CreateTask(const hipc::CtxAllocator<HSHM_DEFAULT_ALLOC> &alloc,
             const TaskNode &task_node,
             const PoolId &pool_id,
             const DomainQuery &dom_query,
             const DomainQuery &affinity,
             const std::string &pool_name,
             const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, pool_id, dom_query, affinity,
                            pool_name, "worch_proc_round_robin", ctx) {
  }

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

/** A task to destroy worch_proc_round_robin */
typedef chi::Admin::DestroyContainerTask DestroyTask;

}  // namespace chi::worch_proc_round_robin

#endif  // CHI_WORCH_PROC_ROUND_ROBIN_TASKS_H__
