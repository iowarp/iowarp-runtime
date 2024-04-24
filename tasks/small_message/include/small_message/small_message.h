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
                            const DomainQuery &dom_query,
                            const std::string &state_name,
                            const TaskStateId &id) {
    HRUN_CLIENT->ConstructTask<CreateTask>(
        task, task_node, dom_query, state_name, id);
  }
  void CreateRoot(const DomainQuery &dom_query,
                  const std::string &state_name,
                  const TaskStateId &id = TaskId::GetNull()) {
    LPointer<CreateTask> task = AsyncCreateRoot(
        dom_query, state_name, id);
    task->Wait();
    Init(task->id_);
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_PUSH_ROOT(Create);

  /** Destroy state + queue */
  HSHM_ALWAYS_INLINE
  void DestroyRoot(const DomainQuery &dom_query) {
    CHM_ADMIN->DestroyTaskStateRoot(dom_query, id_);
  }

  /** Metadata task */
  void AsyncMdConstruct(MdTask *task,
                        const TaskNode &task_node,
                        const DomainQuery &dom_query,
                        u32 lane_hash, u32 depth, u32 flags) {
    HRUN_CLIENT->ConstructTask<MdTask>(
        task, task_node, dom_query, id_, lane_hash, depth, flags);
  }
  int MdRoot(const DomainQuery &dom_query, u32 lane_hash, u32 depth, u32 flags) {
    LPointer<MdTask> task =
        AsyncMdRoot(dom_query, lane_hash, depth, flags);
    task->Wait();
    int ret = task->ret_;
    HRUN_CLIENT->DelTask(task);
    return ret;
  }
  HRUN_TASK_NODE_PUSH_ROOT(Md);

  /** Io task */
  void AsyncIoConstruct(IoTask *task, const TaskNode &task_node,
                        const DomainQuery &dom_query,
                        size_t io_size,
                        u32 io_flags) {
    HRUN_CLIENT->ConstructTask<IoTask>(
        task, task_node, dom_query, id_, io_size, io_flags);
  }
  void IoRoot(const DomainQuery &dom_query, size_t io_size,
             u32 io_flags, size_t &write_ret, size_t &read_ret) {
    LPointer<IoTask> task =
        AsyncIoRoot(dom_query, io_size, io_flags);
    task->Wait();
    write_ret = task->ret_;
    char *data = HRUN_CLIENT->GetDataPointer(task->data_);
    read_ret = 0;
    for (size_t i = 0; i < io_size; ++i) {
      read_ret += data[i];
    }
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_PUSH_ROOT(Io)
};

}  // namespace chm

#endif  // HRUN_small_message_H_
