//
// Created by lukemartinlogan on 6/29/23.
//

#ifndef HRUN_small_message_H_
#define HRUN_small_message_H_

#include "small_message_tasks.h"

namespace chm::small_message {

/** Create admin requests */
class Client : public TaskLibClient {

 public:
  /** Default constructor */
  Client() = default;

  /** Destructor */
  ~Client() = default;

  /** Create a small_message */
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

  /** Destroy state + queue */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainId &domain_id) {
    CHM_ADMIN->DestroyTaskStateRoot(domain_id, id_);
  }

  /** Metadata task */
  void AsyncMdConstruct(MdTask *task,
                        const TaskNode &task_node,
                        const DomainId &domain_id,
                        u32 lane_hash, u32 depth, u32 flags) {
    HRUN_CLIENT->ConstructTask<MdTask>(
        task, task_node, domain_id, id_, lane_hash, depth, flags);
  }
  int MdRoot(const DomainId &domain_id, u32 lane_hash, u32 depth, u32 flags) {
    LPointer<MdTask> task =
        AsyncMdRoot(domain_id, lane_hash, depth, flags);
    task->Wait();
    int ret = task->ret_[0];
    HRUN_CLIENT->DelTask(task);
    return ret;
  }
  HRUN_TASK_NODE_PUSH_ROOT(Md);

  /** Io task */
  void AsyncIoConstruct(IoTask *task, const TaskNode &task_node,
                        const DomainId &domain_id,
                        size_t io_size) {
    HRUN_CLIENT->ConstructTask<IoTask>(
        task, task_node, domain_id, id_, io_size);
  }
  int IoRoot(const DomainId &domain_id, size_t io_size) {
    LPointer<IoTask> task = AsyncIoRoot(domain_id, io_size);
    task->Wait();
    int ret = task->ret_;
    HRUN_CLIENT->DelTask(task);
    return ret;
  }
  HRUN_TASK_NODE_PUSH_ROOT(Io)
};

}  // namespace chm

#endif  // HRUN_small_message_H_
