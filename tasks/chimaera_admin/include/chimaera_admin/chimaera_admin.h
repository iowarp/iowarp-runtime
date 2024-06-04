#ifndef HRUN_TASKS_CHI_ADMIN_CHI_ADMIN_H_
#define HRUN_TASKS_CHI_ADMIN_CHI_ADMIN_H_

#include "chimaera_admin_tasks.h"

namespace chi::Admin {

/** Create admin requests */
class Client : public TaskLibClient {
 public:
  /** Default constructor */
  Client() {
    id_ = PoolId(CHI_QM_CLIENT->admin_queue_id_);
    queue_id_ = CHI_QM_CLIENT->admin_queue_id_;
  }

  /** Destructor */
  ~Client() = default;

  /** Register a task library */
  HSHM_ALWAYS_INLINE
  void AsyncRegisterTaskLibConstruct(RegisterTaskLibTask *task,
                                     const TaskNode &task_node,
                                     const DomainQuery &dom_query,
                                     const std::string &lib_name) {
    CHI_CLIENT->ConstructTask<RegisterTaskLibTask>(
        task, task_node, dom_query, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void RegisterTaskLibRoot(const DomainQuery &dom_query,
                           const std::string &lib_name) {
    LPointer<RegisterTaskLibTask> task = AsyncRegisterTaskLibRoot(dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHIMAERA_TASK_NODE_ROOT(RegisterTaskLib)

  /** Unregister a task */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyTaskLibConstruct(DestroyTaskLibTask *task,
                                    const TaskNode &task_node,
                                    const DomainQuery &dom_query,
                                    const std::string &lib_name) {
    CHI_CLIENT->ConstructTask<DestroyTaskLibTask>(
        task, task_node, dom_query, lib_name);
  }
  HSHM_ALWAYS_INLINE
  void DestroyTaskLibRoot(const TaskNode &task_node,
                              const DomainQuery &dom_query,
                              const std::string &lib_name) {
    LPointer<DestroyTaskLibTask> task =
        AsyncDestroyTaskLibRoot(dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHIMAERA_TASK_NODE_ROOT(DestroyTaskLib)

  /** Spawn a task state */
  template<typename CreateContainerT>
  HSHM_ALWAYS_INLINE
  PoolId CreateContainerComplete(LPointer<CreateContainerT> task) {
    return CreateContainerComplete(task.ptr_);
  }

  /** Get the ID of a task state */
  void AsyncGetPoolIdConstruct(GetPoolIdTask *task,
                                    const TaskNode &task_node,
                                    const DomainQuery &dom_query,
                                    const std::string &pool_name) {
    CHI_CLIENT->ConstructTask<GetPoolIdTask>(
        task, task_node, dom_query, pool_name);
  }
  PoolId GetPoolIdRoot(const DomainQuery &dom_query,
                                 const std::string &pool_name) {
    LPointer<GetPoolIdTask> task =
        AsyncGetPoolIdRoot(dom_query, pool_name);
    task->Wait();
    PoolId new_id = task->id_;
    CHI_CLIENT->DelTask(task);
    return new_id;
  }
  CHIMAERA_TASK_NODE_ROOT(GetPoolId)

  /** Terminate a task state */
  HSHM_ALWAYS_INLINE
  void AsyncDestroyContainerConstruct(DestroyContainerTask *task,
                                      const TaskNode &task_node,
                                      const DomainQuery &dom_query,
                                      const PoolId &id) {
    CHI_CLIENT->ConstructTask<DestroyContainerTask>(
        task, task_node, dom_query, id);
  }
  HSHM_ALWAYS_INLINE
  void DestroyContainerRoot(const DomainQuery &dom_query,
                            const PoolId &id) {
    LPointer<DestroyContainerTask> task =
        AsyncDestroyContainerRoot(dom_query, id);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHIMAERA_TASK_NODE_ROOT(DestroyContainer)

  /** Terminate the runtime */
  void AsyncStopRuntimeConstruct(StopRuntimeTask *task,
                                 const TaskNode &task_node,
                                 const DomainQuery &dom_query,
                                 bool root) {
    CHI_CLIENT->ConstructTask<StopRuntimeTask>(
        task, task_node, dom_query, root);
  }
  void StopRuntimeRoot() {
    HILOG(kInfo, "Beginning to flush the runtime.\n"
                 "If you did async I/O, this may take some time.\n"
                 "All unflushed data will be written to the PFS.");
    FlushRoot(DomainQuery::GetLaneGlobalBcast());
    HILOG(kInfo, "Stopping the runtime");
    AsyncStopRuntimeRoot(DomainQuery::GetDirectHash(
        SubDomainId::kLocalContainers, 0), true);
    HILOG(kInfo, "All done!");
    exit(1);
  }
  CHIMAERA_TASK_NODE_ROOT(StopRuntime);

  /** Set work orchestrator queue policy */
  void AsyncSetWorkOrchQueuePolicyConstruct(SetWorkOrchQueuePolicyTask *task,
                                            const TaskNode &task_node,
                                            const DomainQuery &dom_query,
                                            const PoolId &policy) {
    CHI_CLIENT->ConstructTask<SetWorkOrchQueuePolicyTask>(
        task, task_node, dom_query, policy);
  }
  void SetWorkOrchQueuePolicyRoot(const DomainQuery &dom_query,
                                  const PoolId &policy) {
    LPointer<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicyRoot(dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHIMAERA_TASK_NODE_ROOT(SetWorkOrchQueuePolicy);

  /** Set work orchestrator process policy */
  void AsyncSetWorkOrchProcPolicyConstruct(SetWorkOrchProcPolicyTask *task,
                                           const TaskNode &task_node,
                                           const DomainQuery &dom_query,
                                           const PoolId &policy) {
    CHI_CLIENT->ConstructTask<SetWorkOrchProcPolicyTask>(
        task, task_node, dom_query, policy);
  }
  void SetWorkOrchProcPolicyRoot(const DomainQuery &dom_query,
                                 const PoolId &policy) {
    LPointer<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicyRoot(dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHIMAERA_TASK_NODE_ROOT(SetWorkOrchProcPolicy);

  /** Flush the runtime */
  void AsyncFlushConstruct(FlushTask *task,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query) {
    CHI_CLIENT->ConstructTask<FlushTask>(
        task, task_node, dom_query);
  }
  void FlushRoot(const DomainQuery &dom_query) {
    size_t work_done = 0;
    do {
      LPointer<FlushTask> task =
          AsyncFlushRoot(dom_query);
      task->Wait();
      work_done = task->work_done_;
      CHI_CLIENT->DelTask(task);
      HILOG(kDebug, "Total flush work done: {}", work_done);
    } while (work_done > 0);
  }
  CHIMAERA_TASK_NODE_ROOT(Flush);

  /** Flush the runtime */
  void AsyncGetDomainSizeConstruct(GetDomainSizeTask *task,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query,
                           const DomainId &dom_id) {
    CHI_CLIENT->ConstructTask<GetDomainSizeTask>(
        task, task_node, dom_query, dom_id);
  }
  size_t GetDomainSizeRoot(const DomainQuery &dom_query,
                           const DomainId &dom_id) {
    LPointer<GetDomainSizeTask> task =
        AsyncGetDomainSizeRoot(dom_query, dom_id);
    task->Wait();
    size_t dom_size = task->dom_size_;
    CHI_CLIENT->DelTask(task);
    return dom_size;
  }
  CHIMAERA_TASK_NODE_ROOT(GetDomainSize)
};

}  // namespace chi::Admin

#define CHI_ADMIN \
  hshm::EasySingleton<chi::Admin::Client>::GetInstance()

#endif  // HRUN_TASKS_CHI_ADMIN_CHI_ADMIN_H_
