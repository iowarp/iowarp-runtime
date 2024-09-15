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
using chi::Admin::CreateContainerTask;
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc)
      : CreateContainerTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
             const TaskNode &task_node,
             const DomainQuery &dom_query,
             const PoolId &pool_id,
             const DomainQuery &affinity,
             const std::string &pool_name,
             const CreateContext &ctx)
      : CreateContainerTask(alloc, task_node, dom_query, pool_id, affinity,
                            pool_name, "TASK_NAME", ctx) {
    // Custom params
  }

  HSHM_ALWAYS_INLINE
  ~CreateTask() {
    // Custom params
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

/** A task to destroy TASK_NAME */
typedef chi::Admin::DestroyContainerTask DestroyTask;

/**
 * A custom task in TASK_NAME
 * */
struct CustomTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CustomTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CustomTask(hipc::Allocator *alloc,
             const TaskNode &task_node,
             const DomainQuery &dom_query,
             const PoolId &pool_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCustom;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom params
  }

  /** Duplicate message */
  void CopyStart(const CustomTask &other, bool deep) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

}  // namespace chi::TASK_NAME

#endif  // CHI_TASKS_TASK_TEMPL_INCLUDE_TASK_NAME_TASK_NAME_TASKS_H_
