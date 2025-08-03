#ifndef CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_
#define CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/module_registry/module.h"
#include "chimaera/queue_manager/queue_manager.h"
#include "chimaera/work_orchestrator/scheduler.h"

namespace chi::Admin {

#include "chimaera_admin_methods.h"

/** A task to register a pool + Create a queue */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "chimaera_chimaera_admin";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar> HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
template <typename TaskParamsT>
struct CreatePoolBaseTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN chi::ipc::string lib_name_;
  IN chi::ipc::string pool_name_;
  IN DomainQuery affinity_;
  IN bool root_ = true;
  INOUT chi::ipc::string params_;
  INOUT CreateContext ctx_;

  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit CreatePoolBaseTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc), lib_name_(alloc), pool_name_(alloc), params_(alloc) {}

  /** Emplace constructor */
  template <typename... Args>
  HSHM_INLINE_CROSS_FUN explicit CreatePoolBaseTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const DomainQuery &affinity, const chi::string &pool_name,
      const CreateContext &ctx, Args &&...args)
      : Task(alloc), pool_name_(alloc, pool_name),
        lib_name_(alloc, TaskParamsT::lib_name_), params_(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = chi::ADMIN_POOL_ID;
    method_ = Method::kCreatePool;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    affinity_ = affinity;
    ctx_ = ctx;

    TaskParamsT params{alloc, std::forward<Args>(args)...};
    SetParams(params);
  }

  /** Broadcast constructor */
  template <typename... Args>
  HSHM_INLINE_CROSS_FUN explicit CreatePoolBaseTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const CreatePoolBaseTask &other)
      : Task(alloc), pool_name_(alloc, other.pool_name_),
        lib_name_(alloc, other.lib_name_) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = chi::ADMIN_POOL_ID;
    method_ = Method::kCreatePool;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    root_ = false;
    affinity_ = dom_query;
    ctx_ = other.ctx_;
    params_ = other.params_;
  }

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~CreatePoolBaseTask() {}

  /** Duplicate message */
  HSHM_INLINE_CROSS_FUN
  void CopyStart(const CreatePoolBaseTask &other, bool deep) {
    lib_name_ = other.lib_name_;
    pool_name_ = other.pool_name_;
    ctx_ = other.ctx_;
    root_ = other.root_;
    affinity_ = other.affinity_;
    params_ = other.params_;
  }

  /** (De)serialize message call */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeStart(Ar &ar) {
    ar(lib_name_, pool_name_, ctx_, root_, affinity_, params_);
  }

  /** (De)serialize message return */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeEnd(Ar &ar) {
    ar(ctx_.id_, params_);
  }

  /** Get the parameters */
  HSHM_INLINE_CROSS_FUN
  TaskParamsT GetParams() {
    std::stringstream ss(params_.str());
    cereal::BinaryInputArchive ar(ss);
    TaskParamsT params;
    ar(params);
    return params;
  }

  HSHM_INLINE_CROSS_FUN void SetParams(const TaskParamsT &params) {
    std::stringstream ss;
    cereal::BinaryOutputArchive ar(ss);
    ar(params);
    params_ = ss.str();
  }
};
typedef CreatePoolBaseTask<CreateTaskParams> CreatePoolTask;

CHI_BEGIN(Create)
/** A task to register a pool + Create a queue */
struct CreateTask : public CreatePoolTask {
  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit CreateTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : CreatePoolTask(alloc) {
    method_ = Method::kCreate;
  }
};
CHI_END(Create)

CHI_BEGIN(DestroyPool)
/** A task to destroy a pool */
struct DestroyPoolTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN PoolId id_;

  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit DestroyPoolTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit DestroyPoolTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                           const TaskNode &task_node, const PoolId &pool_id,
                           const DomainQuery &dom_query,
                           const PoolId &destroy_id)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = chi::ADMIN_POOL_ID;
    method_ = Method::kDestroyPool;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    id_ = destroy_id;
  }

  /** Copy message input */
  template <typename DestroyTaskT>
  HSHM_INLINE_CROSS_FUN void CopyStart(const DestroyTaskT &other, bool deep) {
    id_ = other.id_;
  }

  /** (De)serialize message call */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeStart(Ar &ar) {
    ar & id_;
  }

  /** (De)serialize message return */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeEnd(Ar &ar) {}
};
CHI_END(DestroyPool)

CHI_BEGIN(StopRuntime)
/** A task to stop a runtime */
struct StopRuntimeTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN bool root_;

  /** SHM default constructor */
  StopRuntimeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit StopRuntimeTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                           const TaskNode &task_node, const PoolId &pool_id,
                           const DomainQuery &dom_query)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = chi::ADMIN_POOL_ID;
    method_ = Method::kStopRuntime;
    task_flags_.SetBits(TASK_FIRE_AND_FORGET);
    dom_query_ = dom_query;
  }

  /** Duplicate message */
  HSHM_INLINE_CROSS_FUN
  void CopyStart(const StopRuntimeTask &other, bool deep) {}

  /** (De)serialize message call */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeStart(Ar &ar) {}

  /** (De)serialize message return */
  template <typename Ar> HSHM_INLINE_CROSS_FUN void SerializeEnd(Ar &ar) {}
};
CHI_END(StopRuntime)

} // namespace chi::Admin

#endif // CHI_TASKS_CHI_ADMIN_INCLUDE_CHI_ADMIN_CHI_ADMIN_TASKS_H_
