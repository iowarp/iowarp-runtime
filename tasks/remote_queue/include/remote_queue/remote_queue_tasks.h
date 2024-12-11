//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef CHI_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_
#define CHI_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_

#include "chimaera/chimaera_namespace.h"
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <thallium/serialization/stl/list.hpp>

namespace tl = thallium;

namespace chi::remote_queue {

#include "remote_queue_methods.h"
CHI_NAMESPACE_INIT

/**
 * A task to create remote_queue
 * */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "remote_queue";

  CreateTaskParams() = default;

  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  void serialize(Ar &ar) {}
};
typedef chi::Admin::CreateContainerBaseTask<CreateTaskParams> CreateTask;

/** A task to destroy remote_queue */
typedef chi::Admin::DestroyContainerTask DestroyTask;

struct ClientPushSubmitTask : public Task, TaskFlags<TF_LOCAL> {
  IN Task *orig_task_;

  /** SHM default constructor */
  HSHM_INLINE explicit
  ClientPushSubmitTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit
  ClientPushSubmitTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                       const TaskNode &task_node,
                       const PoolId &pool_id,
                       const DomainQuery &dom_query,
                       Task *orig_task) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = orig_task->prio_;
    pool_ = pool_id;
    method_ = Method::kClientPushSubmit;
    task_flags_.SetBits(TASK_COROUTINE |
        TASK_FIRE_AND_FORGET |
        TASK_REMOTE_DEBUG_MARK);
    if (orig_task->IsFlush()) {
      task_flags_.SetBits(TASK_FLUSH);
    }
    dom_query_ = dom_query;

    orig_task_ = orig_task;
  }
};

struct ClientSubmitTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_INLINE explicit
  ClientSubmitTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit
  ClientSubmitTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                   const TaskNode &task_node,
                   const PoolId &pool_id,
                   const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kHighLatency;
    pool_ = pool_id;
    method_ = Method::kClientSubmit;
    task_flags_.SetBits(TASK_LONG_RUNNING | TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
  }
};

struct ServerPushCompleteTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_INLINE explicit
  ServerPushCompleteTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit
  ServerPushCompleteTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                         const TaskNode &task_node,
                         const PoolId &pool_id,
                         const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kServerPushComplete;
    task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
  }
};

struct ServerCompleteTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_INLINE explicit
  ServerCompleteTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit
  ServerCompleteTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                     const TaskNode &task_node,
                     const PoolId &pool_id,
                     const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kHighLatency;
    pool_ = pool_id;
    method_ = Method::kServerComplete;
    task_flags_.SetBits(TASK_LONG_RUNNING | TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
    SetPeriodUs(15);
  }
};

} // namespace chi::remote_queue

#endif //CHI_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_
