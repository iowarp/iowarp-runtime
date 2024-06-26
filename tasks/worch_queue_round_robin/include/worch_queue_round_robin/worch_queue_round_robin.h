//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef HRUN_worch_queue_round_robin_H_
#define HRUN_worch_queue_round_robin_H_

#include "worch_queue_round_robin_tasks.h"

namespace chi::worch_queue_round_robin {

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
                            const DomainQuery &affinity,
                            const std::string &pool_name,
                            const CreateContext &ctx) {
    CHI_CLIENT->ConstructTask<CreateTask>(
        task, task_node, dom_query, affinity, pool_name, ctx);
  }
  void CreateRoot(const DomainQuery &dom_query,
                  const DomainQuery &affinity,
                  const std::string &pool_name,
                  const CreateContext &ctx = CreateContext()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        dom_query, affinity, pool_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(Create);

  /** Destroy task state */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainerRoot(dom_query, id_);
  }
};

}  // namespace chi

#endif  // HRUN_worch_queue_round_robin_H_
