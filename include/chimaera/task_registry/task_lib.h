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

#ifndef HRUN_INCLUDE_HRUN_TASK_TASK_H_
#define HRUN_INCLUDE_HRUN_TASK_TASK_H_

#include <dlfcn.h>
#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue_factory.h"
#include "task.h"
#include "chimaera/network/serialize_defn.h"

namespace chi {

typedef LPointer<Task> TaskPointer;

/** Monitoring modes */
class MonitorMode {
 public:
  TASK_METHOD_T kBeginTrainTime = 0;
  TASK_METHOD_T kEndTrainTime = 1;
  TASK_METHOD_T kEstTime = 2;
  TASK_METHOD_T kFlushStat = 3;
  TASK_METHOD_T kReplicaStart = 4;
  TASK_METHOD_T kReplicaAgg = 5;
  TASK_METHOD_T kOrder = 6;
};

/** The information of a lane */
struct Lane {
  QueueId lane_id_;
  QueueId ingress_id_;
  i32 worker_id_;
  size_t load_ = 0;
};

struct LaneGroup {
  std::vector<Lane> lanes_;
};

/**
 * Represents a custom operation to perform.
 * Tasks are independent of Hermes.
 * */
class TaskLib {
 public:
  PoolId id_;    /**< The unique name of a task state */
  QueueId queue_id_;  /**< The queue id of a task state */
  std::string name_;  /**< The unique semantic name of a task state */
  ContainerId container_id_;       /**< The logical id of a container */
  LaneId lane_counter_ = 0;
  std::unordered_map<LaneGroupId, LaneGroup>
      lane_groups_;  /**< The lanes of a task state */

  /** Default constructor */
  TaskLib() : id_(PoolId::GetNull()) {}

  /** Emplace Constructor */
  void Init(const PoolId &id, const QueueId &queue_id,
            const std::string &name) {
    id_ = id;
    queue_id_ = queue_id;
    name_ = name;
  }

  /** Create a lane group */
  void CreateLaneGroup(const LaneGroupId &id, u32 count, u32 flags = 0);

  /** Get lane */
  Lane* GetLaneByHash(const LaneGroupId &group, u32 hash) {
    LaneGroup &lane_group = lane_groups_[group];
    return &lane_group.lanes_[hash % lane_group.lanes_.size()];
  }

  /** Get lane */
  Lane* GetLane(const QueueId &lane_id) {
    return GetLaneByHash(lane_id.node_id_, lane_id.unique_);
  }

  /** Get number of lanes */
  size_t GetMaxContainersPn() {
    return lane_counter_;
  }

  /** Virtual destructor */
  virtual ~TaskLib() = default;

  /** Route to a virtual lane */
  virtual Lane* Route(const Task *task) = 0;

  /** Run a method of the task */
  virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;

  /** Monitor a method of the task */
  virtual void Monitor(u32 mode, Task *task, RunContext &rctx) = 0;

  /** Delete a task */
  virtual void Del(u32 method, Task *task) = 0;

  /** Duplicate a task into an existing task */
  virtual void CopyStart(u32 method,
                         const Task *orig_task,
                         Task *dup_task,
                         bool deep) = 0;

  /** Duplicate a task into a new task */
  virtual void NewCopyStart(u32 method,
                            const Task *orig_task,
                            LPointer<Task> &dup_task,
                            bool deep) = 0;

  /** Serialize a task when initially pushing into remote */
  virtual void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) = 0;

  /** Deserialize a task when popping from remote queue */
  virtual TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) = 0;

  /** Serialize a task when returning from remote queue */
  virtual void SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) = 0;

  /** Deserialize a task when returning from remote queue */
  virtual void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) = 0;
};

/** Represents a TaskLib in action */
typedef TaskLib Container;

/** Represents the TaskLib client-side */
class TaskLibClient {
 public:
  PoolId id_;
  QueueId queue_id_;

 public:
  /** Init from existing ID */
  void Init(const PoolId &id,
            const QueueId &queue_id) {
    if (id.IsNull()) {
      HELOG(kWarning, "Failed to create task state");
    }
    id_ = id;
    // queue_id_ = QueueId(id_);
    queue_id_ = queue_id;
  }

  /** Init from existing ID */
  void Init(const PoolId &id) {
    if (id.IsNull()) {
      HELOG(kWarning, "Failed to create task state");
    }
    id_ = id;
    // queue_id_ = QueueId(id_);
    queue_id_ = id;
  }
};

extern "C" {
/** Allocate a state (no construction) */
typedef Container* (*alloc_state_t)();
/** New state (with construction) */
typedef Container* (*new_state_t)(
    const chi::PoolId *pool_id, const char *pool_name);
/** Get the name of a task */
typedef const char* (*get_task_lib_name_t)(void);
}  // extern c

/** Used internally by task source file */
#define CHI_TASK_CC(TRAIT_CLASS, TASK_NAME)\
  extern "C" {\
  void* alloc_state(const chi::PoolId *pool_id, const char *pool_name) {\
    chi::Container *exec = reinterpret_cast<chi::Container*>(\
        new TYPE_UNWRAP(TRAIT_CLASS)());\
    return exec;\
  }\
  void* new_state(const chi::PoolId *pool_id, const char *pool_name) {\
    chi::Container *exec = reinterpret_cast<chi::Container*>(\
        new TYPE_UNWRAP(TRAIT_CLASS)());\
    exec->Init(*pool_id, CHI_CLIENT->GetQueueId(*pool_id), pool_name);\
    return exec;\
  }\
  const char* get_task_lib_name(void) { return TASK_NAME; }\
  bool is_chimaera_task_ = true;\
  }

}   // namespace chi

#endif  // HRUN_INCLUDE_HRUN_TASK_TASK_H_
