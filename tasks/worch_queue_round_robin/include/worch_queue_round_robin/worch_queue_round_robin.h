//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef HRUN_worch_queue_round_robin_H_
#define HRUN_worch_queue_round_robin_H_

#include "worch_queue_round_robin_tasks.h"

namespace chm::worch_queue_round_robin {

/** Create admin requests */
class Client : public TaskLibClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a worch_queue_round_robin */
  void AsyncCreateConstruct(CreateTask *task,
                            const TaskNode &task_node,
                            const DomainQuery &dom_query,
                            const DomainQuery &scope_query,
                            const std::string &state_name,
                            const CreateContext &ctx) {
    CHM_CLIENT->ConstructTask<CreateTask>(
        task, task_node, dom_query, scope_query, state_name, ctx);
  }
  void CreateRoot(const DomainQuery &dom_query,
                  const DomainQuery &scope_query,
                  const std::string &state_name,
                  const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        dom_query, scope_query, state_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_PUSH_ROOT(Create);

  /** Destroy task state */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainQuery &dom_query) {
    CHM_ADMIN->DestroyTaskStateRoot(dom_query, id_);
  }
};

}  // namespace chm

#endif  // HRUN_worch_queue_round_robin_H_
