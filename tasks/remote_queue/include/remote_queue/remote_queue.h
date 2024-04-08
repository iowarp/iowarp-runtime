//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef HRUN_remote_queue_H_
#define HRUN_remote_queue_H_

#include "remote_queue_tasks.h"

namespace chm::remote_queue {

/**
 * Create remote_queue requests
 *
 * This is ONLY used in the Hermes runtime, and
 * should never be called in client programs!!!
 * */
class Client : public TaskLibClient {
 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Async create a task state */
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

  /** Destroy task state + queue */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainId &domain_id) {
    CHM_ADMIN->DestroyTaskStateRoot(domain_id, id_);
  }

  /** Construct submit aggregator */
  void AsyncClientSubmitConstruct(ClientSubmitTask *task,
                                  const TaskNode &task_node,
                                  const DomainId &domain_id,
                                  size_t lane_hash) {
    HRUN_CLIENT->ConstructTask<ClientSubmitTask>(
        task, task_node, domain_id, id_, lane_hash);
  }
  HRUN_TASK_NODE_PUSH_ROOT(ClientSubmit)

  /** Construct complete aggregator */
  void AsyncServerCompleteConstruct(ServerCompleteTask *task,
                                    const TaskNode &task_node,
                                    const DomainId &domain_id,
                                    size_t lane_hash) {
    HRUN_CLIENT->ConstructTask<ServerCompleteTask>(
        task, task_node, domain_id, id_, lane_hash);
  }
  HRUN_TASK_NODE_PUSH_ROOT(ServerComplete)
};

}  // namespace chm

#endif  // HRUN_remote_queue_H_
