//
// Created by lukemartinlogan on 8/14/23.
//

#ifndef HRUN_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_
#define HRUN_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"
#include "chimaera_admin/chimaera_admin.h"
#include "chimaera/queue_manager/queue_manager_client.h"

#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <thallium/serialization/stl/list.hpp>

namespace tl = thallium;

namespace chi::remote_queue {

#include "chimaera/chimaera_namespace.h"
#include "remote_queue_methods.h"

/**
 * A task to create remote_queue
 * */
using chi::Admin::CreateTaskStateTask;
struct CreateTask : public CreateTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc) : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(hipc::Allocator *alloc,
             const TaskNode &task_node,
             const DomainQuery &dom_query,
             const DomainQuery &scope_query,
             const std::string &state_name,
             const CreateContext &ctx)
      : CreateTaskStateTask(alloc, task_node, dom_query, scope_query,
                            state_name, "remote_queue", ctx) {
    // Custom params
  }
};

/** A task to destroy remote_queue */
using chi::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainQuery &dom_query,
               TaskStateId &state_id)
      : DestroyTaskStateTask(alloc, task_node, dom_query, state_id) {}
};

struct ClientPushSubmitTask : public Task, TaskFlags<TF_LOCAL> {
  IN Task *orig_task_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ClientPushSubmitTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ClientPushSubmitTask(hipc::Allocator *alloc,
                       const TaskNode &task_node,
                       const DomainQuery &dom_query,
                       TaskStateId &state_id,
                       Task *orig_task) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = orig_task->prio_;
    task_state_ = state_id;
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
  HSHM_ALWAYS_INLINE explicit
  ClientSubmitTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ClientSubmitTask(hipc::Allocator *alloc,
                   const TaskNode &task_node,
                   const DomainQuery &dom_query,
                   TaskStateId &state_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kHighLatency;
    task_state_ = state_id;
    method_ = Method::kClientSubmit;
    task_flags_.SetBits(TASK_LONG_RUNNING | TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
  }
};

struct ServerPushCompleteTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ServerPushCompleteTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ServerPushCompleteTask(hipc::Allocator *alloc,
                         const TaskNode &task_node,
                         const DomainQuery &dom_query,
                         TaskStateId &state_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = state_id;
    method_ = Method::kServerPushComplete;
    task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
  }
};

struct ServerCompleteTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ServerCompleteTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ServerCompleteTask(hipc::Allocator *alloc,
                     const TaskNode &task_node,
                     const DomainQuery &dom_query,
                     TaskStateId &state_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kHighLatency;
    task_state_ = state_id;
    method_ = Method::kServerComplete;
    task_flags_.SetBits(TASK_LONG_RUNNING | TASK_REMOTE_DEBUG_MARK);
    dom_query_ = dom_query;
    SetPeriodUs(15);
  }
};


} // namespace chi::remote_queue

#endif //HRUN_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_
