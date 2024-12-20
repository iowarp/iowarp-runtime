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

  CreateTaskParams() = default;

  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  void serialize(Ar &ar) {}
};
typedef chi::Admin::CreateContainerBaseTask<CreateTaskParams> CreateTask;

/** A task to destroy TASK_NAME */
typedef chi::Admin::DestroyContainerTask DestroyTask;

/**
 * A custom task in TASK_NAME
 * */
struct CustomTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_INLINE explicit CustomTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit CustomTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                                  const TaskNode &task_node,
                                  const PoolId &pool_id,
                                  const DomainQuery &dom_query)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCustom;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
  }

  /** Duplicate message */
  void CopyStart(const CustomTask &other, bool deep) {}

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {}

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {}
};

}  // namespace chi::TASK_NAME

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_TASK_NAME_TASK_NAME_TASKS_H_
