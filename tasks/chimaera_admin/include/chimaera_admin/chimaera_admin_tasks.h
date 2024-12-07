#ifndef CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_
#define CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_

#include "chimaera/work_orchestrator/scheduler.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera/queue_manager/queue_manager_client.h"
#include "chimaera/module_registry/module.h"

namespace chi::Admin {

#include "chimaera_admin_methods.h"

/** A template to register or destroy a task library */
template<int method>
struct RegisterModuleTaskTempl : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::string lib_name_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  RegisterModuleTaskTempl(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc), lib_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  RegisterModuleTaskTempl(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                          const TaskNode &task_node,
                          const PoolId &pool_id,
                          const DomainQuery &dom_query,
                          const std::string &lib_name)
      : Task(alloc), lib_name_(alloc, lib_name) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    if constexpr(method == 0) {
      method_ = Method::kRegisterModule;
    } else {
      method_ = Method::kDestroyModule;
    }
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
  }

  /** Destructor */
  ~RegisterModuleTaskTempl() {}

  /** Duplicate message */
  void CopyStart(const RegisterModuleTaskTempl &other, bool deep) {
    lib_name_ = other.lib_name_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(lib_name_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/** A task to register a Task Library */
using RegisterModuleTask = RegisterModuleTaskTempl<0>;

/** A task to destroy a Task Library */
using DestroyModuleTask = RegisterModuleTaskTempl<1>;

/** A template to register or destroy a task library */
struct UpgradeModuleTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::string lib_name_;
  TEMP Container *old_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  UpgradeModuleTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc), lib_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  UpgradeModuleTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                    const TaskNode &task_node,
                    const PoolId &pool_id,
                    const DomainQuery &dom_query,
                    const std::string &lib_name)
      : Task(alloc), lib_name_(alloc, lib_name) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kUpgradeModule;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
  }

  /** Destructor */
  ~UpgradeModuleTask() {}

  /** Duplicate message */
  void CopyStart(const UpgradeModuleTask &other, bool deep) {
    lib_name_ = other.lib_name_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(lib_name_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }

  template<typename T>
  T* Get() {
    return reinterpret_cast<T *>(old_);
  }
};

/** A task to register a pool + Create a queue */
struct CreateContainerTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::string lib_name_;
  IN chi::ipc::string pool_name_;
  IN DomainQuery affinity_;
  IN bool root_ = true;
  INOUT CreateContext ctx_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateContainerTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc), lib_name_(alloc), pool_name_(alloc) {
  }

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateContainerTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                      const TaskNode &task_node,
                      const PoolId &pool_id,
                      const DomainQuery &dom_query,
                      const DomainQuery &affinity,
                      const std::string &pool_name,
                      const std::string &lib_name,
                      const CreateContext &ctx)
      : Task(alloc),
        pool_name_(alloc, pool_name),
        lib_name_(alloc, lib_name) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kCreateContainer;
    task_flags_.SetBits(TASK_COROUTINE);
    dom_query_ = dom_query;

    // Initialize
    affinity_ = affinity;
    ctx_ = ctx;
  }

  /** Destructor */
  ~CreateContainerTask() {}

  /** Duplicate message */
  void CopyStart(const CreateContainerTask &other, bool deep) {
    // Should never be called...
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }

  /** Duplicate message */
  template<typename CreateTaskT = CreateContainerTask>
  void BaseCopyStart(const CreateTaskT &other, bool deep) {
    lib_name_ = other.lib_name_;
    pool_name_ = other.pool_name_;
    ctx_ = other.ctx_;
    root_ = other.root_;
    affinity_ = other.affinity_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void BaseSerializeStart(Ar &ar) {
    ar(lib_name_, pool_name_, ctx_, root_, affinity_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void BaseSerializeEnd(Ar &ar) {
    ar(ctx_.id_);
  }
};

/** A task to register a pool + Create a queue */
struct CreateTask : public CreateContainerTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : CreateContainerTask(alloc) {
    method_ = Method::kCreate;
  }
};

/** A task to retrieve the ID of a task */
struct GetPoolIdTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::string pool_name_;
  OUT PoolId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  GetPoolIdTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc), pool_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  GetPoolIdTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                const TaskNode &task_node,
                const PoolId &pool_id,
                const DomainQuery &dom_query,
                const std::string &pool_name)
      : Task(alloc), pool_name_(alloc, pool_name) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kGetPoolId;
    task_flags_.SetBits(0);
    dom_query_ = dom_query; }

  ~GetPoolIdTask() {}

  /** Copy message input */
  void CopyStart(const GetPoolIdTask &other,
                 bool deep) {
    pool_name_ = other.pool_name_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(pool_name_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(id_);
  }
};

/** A task to destroy a pool */
struct DestroyContainerTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN PoolId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyContainerTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyContainerTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                       const TaskNode &task_node,
                       const PoolId &pool_id,
                       const DomainQuery &dom_query,
                       const PoolId &destroy_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kDestroyContainer;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    id_ = destroy_id;
  }

  /** Copy message input */
  template<typename DestroyTaskT>
  void CopyStart(const DestroyTaskT &other,
                 bool deep) {
    id_ = other.id_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar & id_;
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/** A task to register a pool + Create a queue */
typedef chi::Admin::DestroyContainerTask DestroyTask;

/** A task to stop a runtime */
struct StopRuntimeTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN bool root_;

  /** SHM default constructor */
  StopRuntimeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  StopRuntimeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                  const TaskNode &task_node,
                  const PoolId &pool_id,
                  const DomainQuery &dom_query,
                  bool root) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kStopRuntime;
    task_flags_.SetBits(TASK_FLUSH | TASK_FIRE_AND_FORGET);
    dom_query_ = dom_query;

    root_ = root;
  }

  /** Duplicate message */
  void CopyStart(const StopRuntimeTask &other, bool deep) {
    root_ = other.root_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(root_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/** A task to set work orchestration policy */
template<int method>
struct SetWorkOrchestratorPolicyTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN PoolId policy_id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  SetWorkOrchestratorPolicyTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  SetWorkOrchestratorPolicyTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                                const TaskNode &task_node,
                                const PoolId &pool_id,
                                const DomainQuery &dom_query,
                                const PoolId &policy_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    if constexpr(method == 0) {
      method_ = Method::kSetWorkOrchQueuePolicy;
    } else {
      method_ = Method::kSetWorkOrchProcPolicy;
    }
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    policy_id_ = policy_id;
  }

  /** Duplicate message */
  void CopyStart(const SetWorkOrchestratorPolicyTask &other, bool deep) {
    policy_id_ = other.policy_id_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(policy_id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};
using SetWorkOrchQueuePolicyTask = SetWorkOrchestratorPolicyTask<0>;
using SetWorkOrchProcPolicyTask = SetWorkOrchestratorPolicyTask<1>;

/** A task to flush the runtime */
struct FlushTask : public Task, TaskFlags<TF_SRL_SYM> {
  INOUT size_t work_done_;

  /** SHM default constructor */
  FlushTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  FlushTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
            const TaskNode &task_node,
            const PoolId &pool_id,
            const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kFlush;
    task_flags_.SetBits(TASK_FLUSH);
    dom_query_ = dom_query;

    // Custom
    work_done_ = 0;
  }

  /** Duplicate message */
  void CopyStart(const FlushTask &other, bool deep) {
    work_done_ = other.work_done_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(work_done_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(work_done_);
  }
};

/** A task to get the domain size */
struct GetDomainSizeTask : public Task, TaskFlags<TF_LOCAL> {
  IN DomainId dom_id_;
  OUT size_t dom_size_;

  /** SHM default constructor */
  GetDomainSizeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  GetDomainSizeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                    const TaskNode &task_node,
                    const PoolId &pool_id,
                    const DomainQuery &dom_query,
                    const DomainId &dom_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kGetDomainSize;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
    dom_id_ = dom_id;
    dom_size_ = 0;
  }
};

/** A task to update the lane mapping */
struct UpdateDomainTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::vector<UpdateDomainInfo> ops_;

  /** SHM default constructor */
  UpdateDomainTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
  : Task(alloc), ops_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  UpdateDomainTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
      const TaskNode &task_node,
      const PoolId &pool_id,
      const DomainQuery &dom_query,
      const std::vector<UpdateDomainInfo> &ops)
      : Task(alloc), ops_(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = CHI_QM_CLIENT->admin_pool_id_;
    method_ = Method::kUpdateDomain;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
    ops_ = ops;
  }

  /** Duplicate message */
  void CopyStart(const UpdateDomainTask &other, bool deep) {
    ops_ = other.ops_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(ops_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

}  // namespace chi::Admin

#endif  // CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_
