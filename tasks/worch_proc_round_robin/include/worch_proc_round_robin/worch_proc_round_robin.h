//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef HRUN_worch_proc_round_robin_H_
#define HRUN_worch_proc_round_robin_H_

#include "worch_proc_round_robin_tasks.h"

namespace chm::worch_proc_round_robin {

/** Create admin requests */
class Client : public TaskLibClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a worch_proc_round_robin */
  void AsyncCreateConstruct(CreateTask *task,
                            const TaskNode &task_node,
                            const DomainId &domain_id,
                            const std::string &state_name,
                            const TaskStateId &id) {
    HRUN_CLIENT->ConstructTask<CreateTask>(
        task, task_node, domain_id, state_name, id);
  }
  void CreateRoot(const DomainId &domain_id,
                  const std::string &state_name,
                  const TaskStateId &id = TaskId::GetNull()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        domain_id, state_name, id);
    task->Wait();
    Init(task->id_);
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_PUSH_ROOT(Create);

  /** Destroy state */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainId &domain_id) {
    CHM_ADMIN->DestroyTaskStateRoot(domain_id, id_);
  }
};

}  // namespace chm

#endif  // HRUN_worch_proc_round_robin_H_
