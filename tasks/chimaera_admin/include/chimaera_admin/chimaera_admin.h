#ifndef CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_
#define CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_

#include "chimaera_admin_tasks.h"

namespace chi::Admin {

/** Create admin requests */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  Client() {
    id_ = PoolId(CHI_QM_CLIENT->admin_queue_id_);
    queue_id_ = CHI_QM_CLIENT->admin_queue_id_;
  }

  /** Destructor */
  ~Client() = default;

  /** Register a module */
  HSHM_ALWAYS_INLINE
  void RegisterModule(const DomainQuery &dom_query,
                      const std::string &lib_name) {
    LPointer<RegisterModuleTask> task =
        AsyncRegisterModule(dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(RegisterModule)

  /** Unregister a module */
  HSHM_ALWAYS_INLINE
  void DestroyModule(const DomainQuery &dom_query,
                     const std::string &lib_name) {
    LPointer<DestroyModuleTask> task =
        AsyncDestroyModule(dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(DestroyModule)

  /** Register a task library */
  HSHM_ALWAYS_INLINE
  void UpgradeModule(const DomainQuery &dom_query,
                     const std::string &lib_name) {
    LPointer<UpgradeModuleTask> task =
        AsyncUpgradeModule(dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(UpgradeModule)

  /** Spawn a pool */
  template<typename CreateContainerT>
  HSHM_ALWAYS_INLINE
  PoolId CreateContainerComplete(LPointer<CreateContainerT> task) {
    return CreateContainerComplete(task.ptr_);
  }

  /** Get the ID of a pool */
  PoolId GetPoolId(const DomainQuery &dom_query,
                   const std::string &pool_name) {
    LPointer<GetPoolIdTask> task =
        AsyncGetPoolId(dom_query, pool_name);
    task->Wait();
    PoolId new_id = task->id_;
    CHI_CLIENT->DelTask(task);
    return new_id;
  }
  CHI_TASK_METHODS(GetPoolId)

  /** Terminate a pool */
  HSHM_ALWAYS_INLINE
  void DestroyContainer(const DomainQuery &dom_query,
                        const PoolId &destroy_id) {
    LPointer<DestroyContainerTask> task =
        AsyncDestroyContainer(dom_query, destroy_id);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
  CHI_TASK_METHODS(DestroyContainer)

  /** Terminate the runtime */
  void StopRuntime() {
    HILOG(kInfo, "Beginning to flush the runtime.\n"
                 "If you did async I/O, this may take some time.\n"
                 "All unflushed data will be written to the PFS.");
    Flush(DomainQuery::GetGlobalBcast());
    HILOG(kInfo, "Stopping the runtime");
    AsyncStopRuntime(DomainQuery::GetDirectHash(
        SubDomainId::kLocalContainers, 0), true);
    HILOG(kInfo, "All done!");
    exit(1);
  }
  CHI_TASK_METHODS(StopRuntime);

  /** Set work orchestrator queue policy */
  void SetWorkOrchQueuePolicy(const DomainQuery &dom_query,
                              const PoolId &policy) {
    LPointer<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicy(dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
#ifdef CHIMAERA_RUNTIME
  void SetWorkOrchQueuePolicyRN(const DomainQuery &dom_query,
                                const PoolId &policy) {
    LPointer<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicyBase(nullptr,
                                        CHI_CLIENT->MakeTaskNodeId(),
                                        dom_query, policy);
    task->SpinWait();
    CHI_CLIENT->DelTask(task);
  }
#endif
  CHI_TASK_METHODS(SetWorkOrchQueuePolicy);

  /** Set work orchestrator process policy */
  void SetWorkOrchProcPolicy(const DomainQuery &dom_query,
                             const PoolId &policy) {
    LPointer<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicy(dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(task);
  }
#ifdef CHIMAERA_RUNTIME
  void SetWorkOrchProcPolicyRN(const DomainQuery &dom_query,
                               const PoolId &policy) {
    LPointer<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicyBase(
            nullptr, CHI_CLIENT->MakeTaskNodeId(),
            dom_query, policy);
    task->SpinWait();
    CHI_CLIENT->DelTask(task);
  }
#endif
  CHI_TASK_METHODS(SetWorkOrchProcPolicy);

  /** Flush the runtime */
  void Flush(const DomainQuery &dom_query) {
    size_t work_done = 0;
    do {
      LPointer<FlushTask> task =
          AsyncFlush(dom_query);
      task->Wait();
      work_done = task->work_done_;
      CHI_CLIENT->DelTask(task);
    } while (work_done > 0);
  }
  CHI_TASK_METHODS(Flush);

  /** Get size of a domain */
  size_t GetDomainSize(const DomainQuery &dom_query,
                       const DomainId &dom_id) {
    LPointer<GetDomainSizeTask> task =
        AsyncGetDomainSize(dom_query, dom_id);
    task->Wait();
    size_t dom_size = task->dom_size_;
    CHI_CLIENT->DelTask(task);
    return dom_size;
  }
  CHI_TASK_METHODS(GetDomainSize)
};

}  // namespace chi::Admin

#define CHI_ADMIN \
  hshm::EasySingleton<chi::Admin::Client>::GetInstance()

#endif  // CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_
