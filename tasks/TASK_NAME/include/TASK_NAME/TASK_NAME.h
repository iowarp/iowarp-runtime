/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HRUN_TASK_NAME_H_
#define HRUN_TASK_NAME_H_

#include "TASK_NAME_tasks.h"

namespace chm::TASK_NAME {

/** Create TASK_NAME requests */
class Client : public TaskLibClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a task state */
  void AsyncCreateConstruct(CreateTask *task,
                            const TaskNode &task_node,
                            const DomainQuery &dom_query,
                            const DomainQuery &scope_query,
                            const std::string &state_name,
                            const TaskStateId &id) {
    CHM_CLIENT->ConstructTask<CreateTask>(
        task, task_node, dom_query, scope_query, state_name, id);
  }
  void CreateRoot(const DomainQuery &dom_query,
                  const DomainQuery &scope_query,
                  const std::string &state_name,
                  const TaskStateId &id = TaskId::GetNull()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        dom_query, scope_query, state_name, id);
    task->Wait();
    Init(task->id_);
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_PUSH_ROOT(Create);

  /** Destroy task state + queue */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainQuery &dom_query) {
    CHM_ADMIN->DestroyTaskStateRoot(dom_query, id_);
  }

  /** Call a custom method */
  HSHM_ALWAYS_INLINE
  void AsyncCustomConstruct(CustomTask *task,
                            const TaskNode &task_node,
                            const DomainQuery &dom_query) {
    CHM_CLIENT->ConstructTask<CustomTask>(
        task, task_node, dom_query, id_);
  }
  HSHM_ALWAYS_INLINE
  void CustomRoot(const DomainQuery &dom_query) {
    LPointer<CustomTask> task = AsyncCustomRoot(dom_query);
    task.ptr_->Wait();
  }
  HRUN_TASK_NODE_PUSH_ROOT(Custom);
};

}  // namespace chm

#endif  // HRUN_TASK_NAME_H_
