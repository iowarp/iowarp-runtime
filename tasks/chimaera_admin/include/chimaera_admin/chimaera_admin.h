#ifndef HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_
#define HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_

#include "chimaera_admin_tasks.h"

namespace chm::Admin {

/** Create admin requests */
class Client : public TaskLibClient {
 public:
  /** Default constructor */
  Client() {
    id_ = TaskStateId(HRUN_QM_CLIENT->admin_queue_id_);
    queue_id_ = HRUN_QM_CLIENT->admin_queue_id_;
  }

  /** Destructor */
  ~Client() = default;

  /** Register a task library */
  HSHM_ALWAYS_INLINE
  void AsyncRegisterTaskLibConstruct(RegisterTaskLibTask *task,
                                     const TaskNode &task_node,
                                     const DomainId &domain_id,
                                     const std::string &lib_name) {
    HRUN_CLIENT->ConstructTask<RegisterTaskLibTask>(
        task, task_node, domain_id, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void RegisterTaskLibRoot(const DomainId &domain_id,
                           const std::string &lib_name) {
    LPointer<RegisterTaskLibTask> task = AsyncRegisterTaskLibRoot(domain_id, lib_name);
    task->Wait();
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(RegisterTaskLib)

  /** Unregister a task */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyTaskLibConstruct(DestroyTaskLibTask *task,
                                    const TaskNode &task_node,
                                    const DomainId &domain_id,
                                    const std::string &lib_name) {
    HRUN_CLIENT->ConstructTask<DestroyTaskLibTask>(
        task, task_node, domain_id, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void DestroyTaskLibRoot(const TaskNode &task_node,
                              const DomainId &domain_id,
                              const std::string &lib_name) {
    LPointer<DestroyTaskLibTask> task =
        AsyncDestroyTaskLibRoot(domain_id, lib_name);
    task->Wait();
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(DestroyTaskLib)

  /** Spawn a task state */
  template<typename CreateTaskStateT>
  HSHM_ALWAYS_INLINE
  TaskStateId CreateTaskStateComplete(LPointer<CreateTaskStateT> task) {
    return CreateTaskStateComplete(task.ptr_);
  }

  /** Get the ID of a task state */
  void AsyncGetOrCreateTaskStateIdConstruct(GetOrCreateTaskStateIdTask *task,
                                            const TaskNode &task_node,
                                            const DomainId &domain_id,
                                            const std::string &state_name) {
    HRUN_CLIENT->ConstructTask<GetOrCreateTaskStateIdTask>(
        task, task_node, domain_id, state_name);
  }
  TaskStateId GetOrCreateTaskStateIdRoot(const DomainId &domain_id,
                                         const std::string &state_name) {
    LPointer<GetOrCreateTaskStateIdTask> task =
        AsyncGetOrCreateTaskStateIdRoot(domain_id, state_name);
    task->Wait();
    TaskStateId new_id = task->id_;
    HRUN_CLIENT->DelTask(task);
    return new_id;
  }
  HRUN_TASK_NODE_ADMIN_ROOT(GetOrCreateTaskStateId)

  /** Get the ID of a task state */
  void AsyncGetTaskStateIdConstruct(GetTaskStateIdTask *task,
                                    const TaskNode &task_node,
                                    const DomainId &domain_id,
                                    const std::string &state_name) {
    HRUN_CLIENT->ConstructTask<GetTaskStateIdTask>(
        task, task_node, domain_id, state_name);
  }
  TaskStateId GetTaskStateIdRoot(const DomainId &domain_id,
                                 const std::string &state_name) {
    LPointer<GetTaskStateIdTask> task =
        AsyncGetTaskStateIdRoot(domain_id, state_name);
    task->Wait();
    TaskStateId new_id = task->id_;
    HRUN_CLIENT->DelTask(task);
    return new_id;
  }
  HRUN_TASK_NODE_ADMIN_ROOT(GetTaskStateId)

  /** Terminate a task state */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyTaskStateConstruct(DestroyTaskStateTask *task,
                                      const TaskNode &task_node,
                                      const DomainId &domain_id,
                                      const TaskStateId &id) {
    HRUN_CLIENT->ConstructTask<DestroyTaskStateTask>(
        task, task_node, domain_id, id);
  }
  HSHM_ALWAYS_INLINE
  void DestroyTaskStateRoot(const DomainId &domain_id,
                            const TaskStateId &id) {
    LPointer<DestroyTaskStateTask> task =
        AsyncDestroyTaskStateRoot(domain_id, id);
    task->Wait();
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(DestroyTaskState)

  /** Terminate the runtime */
  void AsyncStopRuntimeConstruct(StopRuntimeTask *task,
                                 const TaskNode &task_node,
                                 const DomainId &domain_id) {
    HRUN_CLIENT->ConstructTask<StopRuntimeTask>(
        task, task_node, domain_id);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(StopRuntime);
  void StopRuntimeRoot() {
    HILOG(kInfo, "Beginning to flush the runtime.\n"
                 "If you did async I/O, this may take some time.\n"
                 "All unflushed data will be written to the PFS.");
    FlushRoot(DomainId::GetGlobal());
    HILOG(kInfo, "Stopping the runtime");
    AsyncStopRuntimeRoot(DomainId::GetGlobalMinusLocal());
    AsyncStopRuntimeRoot(DomainId::GetLocal());
    HILOG(kInfo, "All done!");
  }

  /** Set work orchestrator queue policy */
  void AsyncSetWorkOrchQueuePolicyConstruct(SetWorkOrchQueuePolicyTask *task,
                                            const TaskNode &task_node,
                                            const DomainId &domain_id,
                                            const TaskStateId &policy) {
    HRUN_CLIENT->ConstructTask<SetWorkOrchQueuePolicyTask>(
        task, task_node, domain_id, policy);
  }
  void SetWorkOrchQueuePolicyRoot(const DomainId &domain_id,
                                  const TaskStateId &policy) {
    LPointer<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicyRoot(domain_id, policy);
    task->Wait();
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(SetWorkOrchQueuePolicy);

  /** Set work orchestrator process policy */
  void AsyncSetWorkOrchProcPolicyConstruct(SetWorkOrchProcPolicyTask *task,
                                           const TaskNode &task_node,
                                           const DomainId &domain_id,
                                           const TaskStateId &policy) {
    HRUN_CLIENT->ConstructTask<SetWorkOrchProcPolicyTask>(
        task, task_node, domain_id, policy);
  }
  void SetWorkOrchProcPolicyRoot(const DomainId &domain_id,
                                 const TaskStateId &policy) {
    LPointer<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicyRoot(domain_id, policy);
    task->Wait();
    HRUN_CLIENT->DelTask(task);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(SetWorkOrchProcPolicy);

  /** Flush the runtime */
  void AsyncFlushConstruct(FlushTask *task,
                           const TaskNode &task_node,
                           const DomainId &domain_id) {
    HRUN_CLIENT->ConstructTask<FlushTask>(
        task, task_node, domain_id);
  }
  void FlushRoot(const DomainId &domain_id) {
    size_t work_done = 0;
    do {
      LPointer<FlushTask> task =
          AsyncFlushRoot(domain_id);
      task->Wait();
      work_done = task->work_done_;
      HRUN_CLIENT->DelTask(task);
    } while (work_done > 0);
  }
  HRUN_TASK_NODE_ADMIN_ROOT(Flush);
};

}  // namespace chm::Admin

#define CHM_ADMIN \
  hshm::EasySingleton<chm::Admin::Client>::GetInstance()

#endif  // HRUN_TASKS_CHM_ADMIN_CHM_ADMIN_H_
