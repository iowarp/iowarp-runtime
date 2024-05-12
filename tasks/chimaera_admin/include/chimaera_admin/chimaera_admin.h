#ifndef HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_
#define HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_

#include "chimaera_admin_tasks.h"

namespace chm::Admin {

/** Create admin requests */
class Client : public TaskLibClient {
 public:
  /** Default constructor */
  Client() {
    id_ = TaskStateId(CHM_QM_CLIENT->admin_queue_id_);
    queue_id_ = CHM_QM_CLIENT->admin_queue_id_;
  }

  /** Destructor */
  ~Client() = default;

  /** Register a task library */
  HSHM_ALWAYS_INLINE
  void AsyncRegisterTaskLibConstruct(RegisterTaskLibTask *task,
                                     const TaskNode &task_node,
                                     const DomainQuery &dom_query,
                                     const std::string &lib_name) {
    CHM_CLIENT->ConstructTask<RegisterTaskLibTask>(
        task, task_node, dom_query, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void RegisterTaskLibRoot(const DomainQuery &dom_query,
                           const std::string &lib_name) {
    LPointer<RegisterTaskLibTask> task = AsyncRegisterTaskLibRoot(dom_query, lib_name);
    task->Wait();
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(RegisterTaskLib)

  /** Unregister a task */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyTaskLibConstruct(DestroyTaskLibTask *task,
                                    const TaskNode &task_node,
                                    const DomainQuery &dom_query,
                                    const std::string &lib_name) {
    CHM_CLIENT->ConstructTask<DestroyTaskLibTask>(
        task, task_node, dom_query, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void DestroyTaskLibRoot(const TaskNode &task_node,
                              const DomainQuery &dom_query,
                              const std::string &lib_name) {
    LPointer<DestroyTaskLibTask> task =
        AsyncDestroyTaskLibRoot(dom_query, lib_name);
    task->Wait();
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(DestroyTaskLib)

  /** Spawn a task state */
  template<typename CreateTaskStateT>
  HSHM_ALWAYS_INLINE
  TaskStateId CreateTaskStateComplete(LPointer<CreateTaskStateT> task) {
    return CreateTaskStateComplete(task.ptr_);
  }

  /** Get the ID of a task state */
  void AsyncGetTaskStateIdConstruct(GetTaskStateIdTask *task,
                                    const TaskNode &task_node,
                                    const DomainQuery &dom_query,
                                    const std::string &state_name) {
    CHM_CLIENT->ConstructTask<GetTaskStateIdTask>(
        task, task_node, dom_query, state_name);
  }
  TaskStateId GetTaskStateIdRoot(const DomainQuery &dom_query,
                                 const std::string &state_name) {
    LPointer<GetTaskStateIdTask> task =
        AsyncGetTaskStateIdRoot(dom_query, state_name);
    task->Wait();
    TaskStateId new_id = task->id_;
    CHM_CLIENT->DelTask(task);
    return new_id;
  }
  HRUN_TASK_NODE_ADMIN_ROOT(GetTaskStateId)

  /** Terminate a task state */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyTaskStateConstruct(DestroyTaskStateTask *task,
                                      const TaskNode &task_node,
                                      const DomainQuery &dom_query,
                                      const TaskStateId &id) {
    CHM_CLIENT->ConstructTask<DestroyTaskStateTask>(
        task, task_node, dom_query, id);
  }
  HSHM_ALWAYS_INLINE
  void DestroyTaskStateRoot(const DomainQuery &dom_query,
                            const TaskStateId &id) {
    LPointer<DestroyTaskStateTask> task =
        AsyncDestroyTaskStateRoot(dom_query, id);
    task->Wait();
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(DestroyTaskState)

  /** Terminate the runtime */
  void AsyncStopRuntimeConstruct(StopRuntimeTask *task,
                                 const TaskNode &task_node,
                                 const DomainQuery &dom_query) {
    CHM_CLIENT->ConstructTask<StopRuntimeTask>(
        task, task_node, dom_query);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(StopRuntime);
  void StopRuntimeRoot() {
    HILOG(kInfo, "Beginning to flush the runtime.\n"
                 "If you did async I/O, this may take some time.\n"
                 "All unflushed data will be written to the PFS.");
    FlushRoot(DomainQuery::GetLaneGlobalBcast());
    HILOG(kInfo, "Stopping the runtime");
    AsyncStopRuntimeRoot(DomainQuery::GetGlobal(
        SubDomainId::kLaneSet, DomainQuery::kBroadcastThisLast));
    HILOG(kInfo, "All done!");
    exit(1);
  }

  /** Set work orchestrator queue policy */
  void AsyncSetWorkOrchQueuePolicyConstruct(SetWorkOrchQueuePolicyTask *task,
                                            const TaskNode &task_node,
                                            const DomainQuery &dom_query,
                                            const TaskStateId &policy) {
    CHM_CLIENT->ConstructTask<SetWorkOrchQueuePolicyTask>(
        task, task_node, dom_query, policy);
  }
  void SetWorkOrchQueuePolicyRoot(const DomainQuery &dom_query,
                                  const TaskStateId &policy) {
    LPointer<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicyRoot(dom_query, policy);
    task->Wait();
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(SetWorkOrchQueuePolicy);

  /** Set work orchestrator process policy */
  void AsyncSetWorkOrchProcPolicyConstruct(SetWorkOrchProcPolicyTask *task,
                                           const TaskNode &task_node,
                                           const DomainQuery &dom_query,
                                           const TaskStateId &policy) {
    CHM_CLIENT->ConstructTask<SetWorkOrchProcPolicyTask>(
        task, task_node, dom_query, policy);
  }
  void SetWorkOrchProcPolicyRoot(const DomainQuery &dom_query,
                                 const TaskStateId &policy) {
    LPointer<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicyRoot(dom_query, policy);
    task->Wait();
    CHM_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(SetWorkOrchProcPolicy);

  /** Flush the runtime */
  void AsyncFlushConstruct(FlushTask *task,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query) {
    CHM_CLIENT->ConstructTask<FlushTask>(
        task, task_node, dom_query);
  }
  void FlushRoot(const DomainQuery &dom_query) {
    size_t work_done = 0;
    do {
      LPointer<FlushTask> task =
          AsyncFlushRoot(dom_query);
      task->Wait();
      work_done = task->work_done_;
      CHM_CLIENT->DelTask(task);
      HILOG(kDebug, "Total flush work done: {}", work_done);
    } while (work_done > 0);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(Flush);

  /** Flush the runtime */
  void AsyncGetDomainSizeConstruct(GetDomainSizeTask *task,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query,
                           const DomainId &dom_id) {
    CHM_CLIENT->ConstructTask<GetDomainSizeTask>(
        task, task_node, dom_query, dom_id);
  }
  size_t GetDomainSizeRoot(const DomainQuery &dom_query,
                           const DomainId &dom_id) {
    LPointer<GetDomainSizeTask> task =
        AsyncGetDomainSizeRoot(dom_query, dom_id);
    task->Wait();
    size_t dom_size = task->dom_size_;
    CHM_CLIENT->DelTask(task);
    return dom_size;
  }
  HRUN_TASK_NODE_ADMIN_ROOT(GetDomainSize)
};

}  // namespace chm::Admin

#define CHM_ADMIN \
  hshm::EasySingleton<chm::Admin::Client>::GetInstance()

#endif  // HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_
