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

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "chimaera/chimaera_types.h"
#include "chimaera/network/serialize_defn.h"
#include "chimaera/queue_manager/queue.h"
#include "chimaera/work_orchestrator/comutex_defn.h"
#include "chimaera/work_orchestrator/corwlock_defn.h"
#include "task.h"

namespace chi {

typedef FullPtr<Task> TaskPointer;

/** The information of a lane */
class Lane : public hipc::list_queue_entry {
 public:
  LaneId lane_id_;
  TaskPrio prio_;
  LaneGroupId group_id_;
  WorkerId worker_id_;
  Load load_;
  CoMutex comux_;
  hipc::atomic<hshm::min_u64> plug_count_;
  size_t lane_req_;
  chi::ext_ring_buffer<TaskPointer> active_tasks_;
  hipc::atomic<hshm::min_u64> count_;

 public:
  /** Default constructor */
  Lane() = default;

  /** Emplace constructor */
  explicit Lane(LaneId lane_id, TaskPrio prio, LaneGroupId group_id,
                WorkerId worker_id)
      : lane_id_(lane_id),
        prio_(prio),
        group_id_(group_id),
        worker_id_(worker_id) {
    plug_count_ = 0;
    count_ = (hshm::min_u64)0;
    // TODO(llogan): Don't hardcode size
    active_tasks_.resize(CHI_LANE_SIZE);
  }

  /** Copy constructor */
  Lane(const Lane &lane) {
    lane_id_ = lane.lane_id_;
    worker_id_ = lane.worker_id_;
    load_ = lane.load_;
    plug_count_ = lane.plug_count_.load();
    prio_ = lane.prio_;
    // TODO(llogan): Don't hardcode size
    active_tasks_.resize(CHI_LANE_SIZE);
  }

#ifdef CHIMAERA_RUNTIME
  /** Push a task  */
  template <bool NO_COUNT>
  hshm::qtok_t push(const FullPtr<Task> &task);

  /** Say we are about to pop a set of tasks */
  size_t pop_prep(size_t count);

  /** Pop a task */
  hshm::qtok_t pop(FullPtr<Task> &task);
#endif

  size_t size() { return count_.load(); }

  bool IsPlugged() { return plug_count_.load() > 0; }

  void SetPlugged() { plug_count_ += 1; }

  void UnsetPlugged() { plug_count_ -= 1; }
};

/** A group of lanes */
struct LaneGroup {
  chi::IntFlag flags_;
  std::vector<Lane> all_lanes_;
  std::vector<Lane *> lanes_[TaskPrioOpt::kNumPrio];

  LaneGroup(chi::IntFlag flags) : flags_(flags) {}

  Lane *get(TaskPrio prio, u32 idx) { return lanes_[prio][idx]; }

  void reserve(u32 count) {
    all_lanes_.reserve(2 * count);
    for (u32 i = 0; i < TaskPrioOpt::kNumPrio; ++i) {
      lanes_[i].reserve(count);
    }
  }

  void emplace_back(LaneId lane_id, TaskPrio prio, LaneGroupId group_id,
                    WorkerId worker_id) {
    all_lanes_.emplace_back(lane_id, prio, group_id, worker_id);
    lanes_[prio].emplace_back(&all_lanes_.back());
  }

  size_t size() { return lanes_[0].size(); }
};

/**
 * Represents a custom operation to perform.
 * Tasks are independent of Hermes.
 * */
class Module {
 public:
  PoolId id_;                /**< The unique name of a pool */
  QueueId queue_id_;         /**< The queue id of a pool */
  std::string name_;         /**< The unique semantic name of a pool */
  ContainerId container_id_; /**< The logical id of a container */
  std::vector<std::shared_ptr<LaneGroup>>
      lane_groups_; /**< The lanes of a pool */
  bool is_created_ = false;

  /** Default constructor */
  Module() : id_(PoolId::GetNull()) {}

  /** Emplace Constructor */
  void Init(const PoolId &id, const QueueId &queue_id,
            const std::string &name) {
    id_ = id;
    queue_id_ = queue_id;
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
  virtual ~Module() = default;

  /** Route to a virtual lane */
  virtual Lane *MapTaskToLane(const Task *task) = 0;

  /** Run a method of the task */
  virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;

  /** Monitor a method of the task */
  virtual void Monitor(MonitorModeId mode, u32 method, Task *task,
                       RunContext &rctx) = 0;

  /** Delete a task */
  virtual void Del(const hipc::MemContext &ctx, u32 method, Task *task) = 0;

  /** Duplicate a task into an existing task */
  virtual void CopyStart(u32 method, const Task *orig_task, Task *dup_task,
                         bool deep) = 0;

  /** Duplicate a task into a new task */
  virtual void NewCopyStart(u32 method, const Task *orig_task,
                            FullPtr<Task> &dup_task, bool deep) = 0;

  /** Serialize a task when initially pushing into remote */
  virtual void SaveStart(u32 method, BinaryOutputArchive<true> &ar,
                         Task *task) = 0;

  /** Deserialize a task when popping from remote queue */
  virtual TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) = 0;

  /** Serialize a task when returning from remote queue */
  virtual void SaveEnd(u32 method, BinaryOutputArchive<false> &ar,
                       Task *task) = 0;

  /** Deserialize a task when returning from remote queue */
  virtual void LoadEnd(u32 method, BinaryInputArchive<false> &ar,
                       Task *task) = 0;
};

/** Represents a Module in action */
typedef Module Container;

/** Represents the Module client-side */
class ModuleClient {
 public:
  PoolId id_;
  QueueId queue_id_;

 public:
  /** Init from existing ID */
  void Init(const PoolId &id, const QueueId &queue_id) {
    if (id.IsNull()) {
      HELOG(kWarning, "Failed to create pool");
    }
    id_ = id;
    // queue_id_ = QueueId(id_);
    queue_id_ = queue_id;
  }

  /** Init from existing ID */
  void Init(const PoolId &id) {
    if (id.IsNull()) {
      HELOG(kWarning, "Failed to create pool");
    }
    id_ = id;
    // queue_id_ = QueueId(id_);
    queue_id_ = id;
  }
};

extern "C" {
/** Allocate a state (no construction) */
typedef Container *(*alloc_state_t)();
/** New state (with construction) */
typedef Container *(*new_state_t)(const chi::PoolId *pool_id,
                                  const char *pool_name);
/** Get the name of a task */
typedef const char *(*get_module_name_t)(void);
}  // extern c

/** Used internally by task source file */
#define CHI_TASK_CC(TRAIT_CLASS, TASK_NAME)                                 \
  extern "C" {                                                              \
  void *alloc_state(const chi::PoolId *pool_id, const char *pool_name) {    \
    chi::Container *exec =                                                  \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)()); \
    return exec;                                                            \
  }                                                                         \
  void *new_state(const chi::PoolId *pool_id, const char *pool_name) {      \
    chi::Container *exec =                                                  \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)()); \
    exec->Init(*pool_id, CHI_CLIENT->GetQueueId(*pool_id), pool_name);      \
    return exec;                                                            \
  }                                                                         \
  const char *get_module_name(void) { return TASK_NAME; }                   \
  bool is_chimaera_task_ = true;                                            \
  }

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_TASK_TASK_H_
