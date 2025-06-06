/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef CHI_INCLUDE_CHI_TASK_TASK_H_
#define CHI_INCLUDE_CHI_TASK_TASK_H_

#include "chimaera/chimaera_types.h"
#include "chimaera/network/serialize_defn.h"
#include "module_queue.h"
#include "task.h"

namespace chi {

/** Forward declaration of Module */
class Module;

/** Represents a Module in action */
typedef Module Container;

/**
 * Represents a custom operation to perform.
 * Tasks are independent of Hermes.
 * */
#ifdef CHIMAERA_RUNTIME
class Module {
public:
  PoolId pool_id_;           /**< The unique name of a pool */
  std::string name_;         /**< The unique semantic name of a pool */
  ContainerId container_id_; /**< The logical id of a container */
  std::vector<std::shared_ptr<LaneGroup>>
      lane_groups_; /**< The lanes of a pool */
  bool is_created_ = false;

  /** Default constructor */
  Module() : pool_id_(PoolId::GetNull()) {}

  /** Copy constructor */
  Module(const Module &other) : pool_id_(other.pool_id_) {
    name_ = other.name_;
    container_id_ = other.container_id_;
    is_created_ = other.is_created_;
    lane_groups_ = other.lane_groups_;
  }

  /** Move constructor */
  Module(Module &&other) noexcept : pool_id_(std::move(other.pool_id_)) {
    name_ = other.name_;
    container_id_ = other.container_id_;
    is_created_ = other.is_created_;
    lane_groups_ = other.lane_groups_;
  }

  /** Copy assignment operator */
  Module &operator=(const Module &other) {
    if (this != &other) {
      pool_id_ = other.pool_id_;
      name_ = other.name_;
      container_id_ = other.container_id_;
      is_created_ = other.is_created_;
      lane_groups_ = other.lane_groups_;
    }
    return *this;
  }

  /** Move assignment operator */
  Module &operator=(Module &&other) noexcept {
    if (this != &other) {
      pool_id_ = std::move(other.pool_id_);
      name_ = other.name_;
      container_id_ = other.container_id_;
      is_created_ = other.is_created_;
      lane_groups_ = other.lane_groups_;
    }
    return *this;
  }

  /** Emplace Constructor */
  void Init(const PoolId &id, const std::string &name) {
    pool_id_ = id;
    name_ = name;
  }

  /** Create a lane group */
  void CreateLaneGroup(LaneGroupId group_id, u32 count, chi::IntFlag flags);

  /** Get lane */
  Lane *GetLane(LaneGroupId group_id, TaskPrio prio, u32 idx) {
    LaneGroup &lane_group = *lane_groups_[group_id];
    return lane_group.get(prio, idx);
  }

  /** Get lane */
  Lane *GetLaneByHash(LaneGroupId group_id, TaskPrio prio, u32 hash) {
    LaneGroup &lane_group = *lane_groups_[group_id];
    return GetLane(group_id, prio, hash % lane_group.size());
  }

  /** Get lane with the least load */
  template <typename F>
  Lane *GetLeastLoadedLane(LaneGroupId group_id, TaskPrio prio, F &&func) {
    LaneGroup &lane_group = *lane_groups_[group_id];
    Lane *least_loaded = lane_group.get(prio, 0);
    for (Lane *lane : lane_group.lanes_[prio]) {
      if (func(lane->load_, least_loaded->load_)) {
        least_loaded = lane;
      }
    }
    return least_loaded;
  }

  /** Plug all lanes */
  void PlugAllLanes() {
    for (auto &lane_group : lane_groups_) {
      for (Lane &lane : lane_group->all_lanes_) {
        lane.UnsetPlugged();
      }
    }
  }

  /** Unplug all lanes */
  void UnplugAllLanes() {
    for (auto &lane_group : lane_groups_) {
      for (Lane &lane : lane_group->all_lanes_) {
        lane.UnsetPlugged();
      }
    }
  }

  /** Get number of active tasks */
  size_t GetNumActiveTasks() {
    size_t num_active = 0;
    for (auto &lane_group : lane_groups_) {
      for (Lane &lane : lane_group->all_lanes_) {
        num_active += lane.size();
      }
    }
    return num_active;
  }

  /** Virtual destructor */
  HSHM_DLL virtual ~Module() = default;

  /** Route to a virtual lane */
  HSHM_DLL virtual Lane *MapTaskToLane(const Task *task) = 0;

  /** Run a method of the task */
  HSHM_DLL virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;

  /** Monitor a method of the task */
  HSHM_DLL virtual void Monitor(MonitorModeId mode, u32 method, Task *task,
                                RunContext &rctx) = 0;

  /** Delete a task */
  HSHM_DLL virtual void Del(const hipc::MemContext &ctx, u32 method,
                            Task *task) = 0;

  /** Duplicate a task into an existing task */
  HSHM_DLL virtual void CopyStart(u32 method, const Task *orig_task,
                                  Task *dup_task, bool deep) = 0;

  /** Duplicate a task into a new task */
  HSHM_DLL virtual void NewCopyStart(u32 method, const Task *orig_task,
                                     FullPtr<Task> &dup_task, bool deep) = 0;

  /** Serialize a task when initially pushing into remote */
  HSHM_DLL virtual void SaveStart(u32 method, BinaryOutputArchive<true> &ar,
                                  Task *task) = 0;

  /** Deserialize a task when popping from remote queue */
  HSHM_DLL virtual TaskPointer LoadStart(u32 method,
                                         BinaryInputArchive<true> &ar) = 0;

  /** Serialize a task when returning from remote queue */
  HSHM_DLL virtual void SaveEnd(u32 method, BinaryOutputArchive<false> &ar,
                                Task *task) = 0;

  /** Deserialize a task when returning from remote queue */
  HSHM_DLL virtual void LoadEnd(u32 method, BinaryInputArchive<false> &ar,
                                Task *task) = 0;
};
#endif // CHIMAERA_RUNTIME

/** Represents the Module client-side */
class ModuleClient {
public:
  PoolId pool_id_;

public:
  /** Init from existing ID */
  HSHM_CROSS_FUN
  void Init(const PoolId &id) {
    if (id.IsNull()) {
      HELOG(kWarning, "Failed to create pool");
    }
    pool_id_ = id;
  }

  /** Default constructor */
  HSHM_CROSS_FUN
  ModuleClient() : pool_id_(PoolId::GetNull()) {}

  /** Copy constructor */
  HSHM_CROSS_FUN
  ModuleClient(const ModuleClient &other) : pool_id_(other.pool_id_) {}

  /** Move constructor */
  HSHM_CROSS_FUN
  ModuleClient(ModuleClient &&other) noexcept
      : pool_id_(std::move(other.pool_id_)) {}

  /** Copy assignment operator */
  HSHM_CROSS_FUN
  ModuleClient &operator=(const ModuleClient &other) {
    if (this != &other) {
      pool_id_ = other.pool_id_;
    }
    return *this;
  }

  /** Move assignment operator */
  HSHM_CROSS_FUN
  ModuleClient &operator=(ModuleClient &&other) noexcept {
    if (this != &other) {
      pool_id_ = std::move(other.pool_id_);
    }
    return *this;
  }

  template <typename Ar> void serialize(Ar &ar) { ar(pool_id_); }
};

extern "C" {
/** Allocate a state (no construction) */
typedef Container *(*alloc_state_t)();
/** New state (with construction) */
typedef Container *(*new_state_t)(const chi::PoolId *pool_id,
                                  const char *pool_name);
/** Get the name of a task */
typedef const char *(*get_module_name_t)(void);
} // extern c

/** Used internally by task source file */
#define CHI_TASK_CC(TRAIT_CLASS, MOD_NAME)                                     \
  extern "C" {                                                                 \
  HSHM_DLL void *alloc_state(const chi::PoolId *pool_id,                       \
                             const char *pool_name) {                          \
    chi::Container *exec =                                                     \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)());    \
    return exec;                                                               \
  }                                                                            \
  HSHM_DLL void *new_state(const chi::PoolId *pool_id,                         \
                           const char *pool_name) {                            \
    chi::Container *exec =                                                     \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)());    \
    exec->Init(*pool_id, pool_name);                                           \
    return exec;                                                               \
  }                                                                            \
  HSHM_DLL const char *get_module_name(void) { return MOD_NAME; }              \
  HSHM_DLL bool is_chimaera_task_ = true;                                      \
  }

} // namespace chi

#endif // CHI_INCLUDE_CHI_TASK_TASK_H_
