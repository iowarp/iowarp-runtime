#ifndef HRUN_TASKS_CHM_ADMIN_INCLUDE_CHM_ADMIN_CHM_ADMIN_TASKS_H_
#define HRUN_TASKS_CHM_ADMIN_INCLUDE_CHM_ADMIN_CHM_ADMIN_TASKS_H_

#include "chimaera/work_orchestrator/scheduler.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera/queue_manager/queue_manager_client.h"
#include "chimaera/chimaera_namespace.h"

namespace chm::Admin {

#include "chimaera_admin_methods.h"

/** A template to register or destroy a task library */
template<int method>
struct RegisterTaskLibTaskTempl : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::string lib_name_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  RegisterTaskLibTaskTempl(hipc::Allocator *alloc)
  : Task(alloc), lib_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  RegisterTaskLibTaskTempl(hipc::Allocator *alloc,
                           const TaskNode &task_node,
                           const DomainQuery &dom_query,
                           const std::string &lib_name)
  : Task(alloc), lib_name_(alloc, lib_name) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    if constexpr(method == 0) {
      method_ = Method::kRegisterTaskLib;
    } else {
      method_ = Method::kDestroyTaskLib;
    }
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
  }

  /** Destructor */
  ~RegisterTaskLibTaskTempl() {}

  /** Duplicate message */
  void CopyStart(const RegisterTaskLibTaskTempl &other, bool deep) {
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
using RegisterTaskLibTask = RegisterTaskLibTaskTempl<0>;

/** A task to destroy a Task Library */
using DestroyTaskLibTask = RegisterTaskLibTaskTempl<1>;

class CreateTaskStatePhase {
 public:
  // NOTE(llogan): kLast is intentially 0 so that the constructor
  // can seamlessly pass data to submethods
  TASK_METHOD_T kIdAllocStart = 0;
  TASK_METHOD_T kIdAllocWait = 1;
  TASK_METHOD_T kStateCreate = 2;
  TASK_METHOD_T kLast = 0;
};

/** A task to get or retrieve the ID of a task */
struct GetOrCreateTaskStateIdTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::string state_name_;
  OUT TaskStateId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  GetOrCreateTaskStateIdTask(hipc::Allocator *alloc)
  : Task(alloc), state_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  GetOrCreateTaskStateIdTask(hipc::Allocator *alloc,
                             const TaskNode &task_node,
                             const DomainQuery &dom_query,
                             const std::string &state_name)
  : Task(alloc), state_name_(alloc, state_name) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kGetOrCreateTaskStateId;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
  }

  ~GetOrCreateTaskStateIdTask() {}

  /** Duplicate message */
  void CopyStart(const GetOrCreateTaskStateIdTask &other, bool deep) {
    state_name_ = other.state_name_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(state_name_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(id_);
  }
};

/** A task to register a Task state + Create a queue */
struct CreateTaskStateTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::string lib_name_;
  IN hipc::string state_name_;
  IN size_t max_lanes_;
  INOUT TaskStateId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTaskStateTask(hipc::Allocator *alloc)
  : Task(alloc), lib_name_(alloc), state_name_(alloc) {
  }

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  CreateTaskStateTask(hipc::Allocator *alloc,
                      const TaskNode &task_node,
                      const DomainQuery &dom_query,
                      const std::string &state_name,
                      const std::string &lib_name,
                      const TaskStateId &id)
  : Task(alloc),
    state_name_(alloc, state_name),
    lib_name_(alloc, lib_name) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kCreateTaskState;
    task_flags_.SetBits(TASK_COROUTINE);
    dom_query_ = dom_query;

    // Initialize
    id_ = id;
  }

  /** Destructor */
  ~CreateTaskStateTask() {}

  /** Duplicate message */
  template<typename CreateTaskT = CreateTaskStateTask>
  void CopyStart(const CreateTaskT &other, bool deep) {
    lib_name_ = other.lib_name_;
    state_name_ = other.state_name_;
    id_ = other.id_;
    HILOG(kInfo, "Copying CreateTaskStateTask: {} {} {}",
          lib_name_.str(), state_name_.str(), id_)
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(lib_name_, state_name_, id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(id_);
  }
};

/** A task to retrieve the ID of a task */
struct GetTaskStateIdTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::string state_name_;
  OUT TaskStateId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  GetTaskStateIdTask(hipc::Allocator *alloc)
  : Task(alloc), state_name_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  GetTaskStateIdTask(hipc::Allocator *alloc,
                     const TaskNode &task_node,
                     const DomainQuery &dom_query,
                     const std::string &state_name)
  : Task(alloc), state_name_(alloc, state_name) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kGetTaskStateId;
    task_flags_.SetBits(0);
    dom_query_ = dom_query; }

  ~GetTaskStateIdTask() {}

  /** Copy message input */
  void CopyStart(const GetTaskStateIdTask &other,
                 bool deep) {
    state_name_ = other.state_name_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(state_name_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(id_);
  }
};

/** A task to destroy a Task state */
struct DestroyTaskStateTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN TaskStateId id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyTaskStateTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestroyTaskStateTask(hipc::Allocator *alloc,
                       const TaskNode &task_node,
                       const DomainQuery &dom_query,
                       const TaskStateId &id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kDestroyTaskState;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Initialize
    id_ = id;
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

/** A task to stop a runtime */
struct StopRuntimeTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  StopRuntimeTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  StopRuntimeTask(hipc::Allocator *alloc,
                  const TaskNode &task_node,
                  const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kStopRuntime;
    task_flags_.SetBits(TASK_FLUSH | TASK_FIRE_AND_FORGET);
    dom_query_ = dom_query;
  }

  /** Duplicate message */
  void CopyStart(StopRuntimeTask &other, bool deep) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/** A task to set work orchestration policy */
template<int method>
struct SetWorkOrchestratorPolicyTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN TaskStateId policy_id_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  SetWorkOrchestratorPolicyTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  SetWorkOrchestratorPolicyTask(hipc::Allocator *alloc,
                                const TaskNode &task_node,
                                const DomainQuery &dom_query,
                                const TaskStateId &policy_id) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
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
  void CopyStart(SetWorkOrchestratorPolicyTask &other, bool deep) {
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
  FlushTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  FlushTask(hipc::Allocator *alloc,
            const TaskNode &task_node,
            const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kFlush;
    task_flags_.SetBits(TASK_FLUSH | TASK_COROUTINE);
    dom_query_ = dom_query;

    // Custom
    work_done_ = 0;
  }

  /** Duplicate message */
  void CopyStart(FlushTask &other, bool deep) {
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
struct DomainSizeTask : public Task, TaskFlags<TF_LOCAL> {
  IN DomainQuery comm_;
  OUT size_t comm_size_;

  /** SHM default constructor */
  DomainSizeTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DomainSizeTask(hipc::Allocator *alloc,
                 const TaskNode &task_node,
                 const DomainQuery &dom_query) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kDomainSize;
    task_flags_.SetBits(0);
    dom_query_ = DomainQuery::GetLocal();

    // Custom
    comm_ = dom_query;
    comm_size_ = 0;
  }
};

/** A task to update the lane mapping */
struct UpdateLaneMappingTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::vector<hipc::pair<StateLaneId, DomainQuery>> mapping_;

  /** SHM default constructor */
  UpdateLaneMappingTask(hipc::Allocator *alloc)
  : Task(alloc), mapping_(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  UpdateLaneMappingTask(
      hipc::Allocator *alloc,
      const TaskNode &task_node,
      const DomainQuery &dom_query,
      const std::vector<std::pair<StateLaneId, DomainQuery>> &mapping)
  : Task(alloc), mapping_(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kUpdateLaneMapping;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Copy the mapping
    mapping_.reserve(mapping.size());
    for (size_t i = 0; i < mapping.size(); i++) {
      mapping_.emplace_back(mapping[i].first, mapping[i].second);
    }
  }

  /** Duplicate message */
  void CopyStart(UpdateLaneMappingTask &other, bool deep) {
    mapping_ = other.mapping_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(mapping_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

/** A task to update the lane mapping */
struct GetLaneMappingTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN TaskStateId state_id_;
  IN LaneId lane_id_;
  OUT DomainQuery lane_domain_;

  /** SHM default constructor */
  GetLaneMappingTask(hipc::Allocator *alloc)
  : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  GetLaneMappingTask(
      hipc::Allocator *alloc,
      const TaskNode &task_node,
      const DomainQuery &dom_query,
      const TaskStateId &state_id,
      LaneId lane_id)
  : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kGetLaneMapping;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    state_id_ = state_id;
    lane_id_ = lane_id;
  }

  /** Duplicate message */
  void CopyStart(GetLaneMappingTask &other, bool deep) {
    state_id_ = other.state_id_;
    lane_id_ = other.lane_id_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(state_id_, lane_id_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
    ar(lane_domain_);
  }
};

/** A task to update the lane mapping */
struct UpdateLaneCountTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN TaskStateId state_id_;
  IN u32 lane_count_;

  /** SHM default constructor */
  UpdateLaneCountTask(hipc::Allocator *alloc)
  : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  UpdateLaneCountTask(
      hipc::Allocator *alloc,
      const TaskNode &task_node,
      const DomainQuery &dom_query,
      const TaskStateId &state_id,
      u32 lane_count)
  : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    GetLaneHash() = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = HRUN_QM_CLIENT->admin_task_state_;
    method_ = Method::kDomainSize;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    state_id_ = state_id;
    lane_count_ = lane_count;
  }

  /** Duplicate message */
  void CopyStart(UpdateLaneCountTask &other, bool deep) {
    state_id_ = other.state_id_;
    lane_count_ = other.lane_count_;
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar(state_id_, lane_count_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};

}  // namespace chm::Admin

#endif  // HRUN_TASKS_CHM_ADMIN_INCLUDE_CHM_ADMIN_CHM_ADMIN_TASKS_H_
