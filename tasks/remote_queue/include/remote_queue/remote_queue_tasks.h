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

namespace chm::remote_queue {

#include "chimaera/chimaera_namespace.h"
#include "remote_queue_methods.h"

/**
 * A task to create remote_queue
 * */
using chm::Admin::CreateTaskStateTask;
struct ConstructTask : public CreateTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ConstructTask(hipc::Allocator *alloc) : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ConstructTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainId &domain_id,
                const std::string &state_name,
                const TaskStateId &id,
                const std::vector<PriorityInfo> &queue_info)
      : CreateTaskStateTask(alloc, task_node, domain_id, state_name,
                            "remote_queue", id, queue_info) {
    // Custom params
  }
};

/** A task to destroy remote_queue */
using chm::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc) : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainId &domain_id,
               TaskStateId &state_id)
      : DestroyTaskStateTask(alloc, task_node, domain_id, state_id) {}

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

struct PushTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  PushTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  PushTask(hipc::Allocator *alloc,
           const TaskNode &task_node,
           const DomainId &domain_id,
           TaskStateId &state_id) : Task(alloc) {}
};

struct PushCompleteTask : public Task, TaskFlags<TF_LOCAL> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  PushCompleteTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  PushCompleteTask(hipc::Allocator *alloc,
                   const TaskNode &task_node,
                   const DomainId &domain_id,
                   TaskStateId &state_id) : Task(alloc) {}
};

} // namespace chm::remote_queue

#endif //HRUN_TASKS_REMOTE_QUEUE_INCLUDE_REMOTE_QUEUE_REMOTE_QUEUE_TASKS_H_
