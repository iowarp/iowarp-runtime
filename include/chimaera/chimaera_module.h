#ifndef CHI_CHIMAERA_MODULE_H_
#define CHI_CHIMAERA_MODULE_H_

#include <hermes_shm/data_structures/all.h>
#include "chimaera_types.h"
#include "chimaera_task.h"

namespace chi {

class Lane {
public:
  LaneId lane_id_;
  QueueId queue_id_;
  chi::ipc::mpsc_queue<TaskPointer> task_queue_;
  hshm::atomic<u64> load_;
  bool is_active_;

  Lane() : load_(0), is_active_(false) {}

  Lane(LaneId lane_id, QueueId queue_id, u32 depth)
      : lane_id_(lane_id), queue_id_(queue_id), task_queue_(depth), load_(0), is_active_(false) {}

  void EnqueueTask(const TaskPointer &task) {
    task_queue_.emplace(task);
    load_.fetch_add(1);
  }

  bool DequeueTask(TaskPointer &task) {
    auto qtok = task_queue_.pop(task);
    if (!qtok.IsNull()) {
      load_.fetch_sub(1);
      return true;
    }
    return false;
  }

  u64 GetLoad() const {
    return load_.load();
  }

  void SetActive(bool active) {
    is_active_ = active;
  }

  bool IsActive() const {
    return is_active_;
  }
};

class Module;
typedef Module Container;

#ifdef CHIMAERA_RUNTIME
class Module {
public:
  PoolId pool_id_;
  std::string pool_name_;
  ContainerId container_id_;
  chi::ipc::unordered_map<QueueId, chi::ipc::vector<Lane*>> queues_;

  Module() = default;
  virtual ~Module() = default;

  void Init(const PoolId &pool_id, const std::string &pool_name) {
    pool_id_ = pool_id;
    pool_name_ = pool_name;
    container_id_ = 0;
  }

  void CreateQueue(QueueId queue_id, u32 num_lanes, IntFlag flags) {
    auto &lanes = queues_[queue_id];
    lanes.resize(num_lanes);
    
    for (u32 i = 0; i < num_lanes; ++i) {
      lanes[i] = new Lane(i, queue_id, 1024);
    }
  }

  Lane* GetLane(QueueId queue_id, LaneId lane_id) {
    auto it = queues_.find(queue_id);
    if (it != queues_.end() && lane_id < it->second.size()) {
      return it->second[lane_id];
    }
    return nullptr;
  }

  Lane* GetLaneByHash(QueueId queue_id, u32 hash) {
    auto it = queues_.find(queue_id);
    if (it != queues_.end()) {
      LaneId lane_id = hash % it->second.size();
      return it->second[lane_id];
    }
    return nullptr;
  }

  template <typename F>
  Lane* GetLeastLoadedLane(QueueId queue_id, F &&func) {
    auto it = queues_.find(queue_id);
    if (it == queues_.end() || it->second.empty()) {
      return nullptr;
    }

    Lane* best_lane = it->second[0];
    u64 best_load = best_lane->GetLoad();

    for (size_t i = 1; i < it->second.size(); ++i) {
      u64 current_load = it->second[i]->GetLoad();
      if (func(current_load, best_load)) {
        best_lane = it->second[i];
        best_load = current_load;
      }
    }

    return best_lane;
  }

  virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;
  virtual void Monitor(MonitorModeId mode, u32 method, Task *task, RunContext &rctx) = 0;
  virtual void Del(const hipc::MemContext &ctx, u32 method, Task *task) = 0;
  virtual void Copy(u32 method, const Task *orig_task, Task *dup_task, bool deep) = 0;
  virtual void NewCopy(u32 method, const Task *orig_task, hshm::ipc::FullPtr<Task> &dup_task, bool deep) = 0;
  virtual void SaveIn(u32 method, hshm::BinaryOutputArchive<true> &ar, Task *task) = 0;
  virtual TaskPointer LoadIn(u32 method, hshm::BinaryInputArchive<true> &ar) = 0;
  virtual void SaveOut(u32 method, hshm::BinaryOutputArchive<false> &ar, Task *task) = 0;
  virtual void LoadOut(u32 method, hshm::BinaryInputArchive<false> &ar, Task *task) = 0;
};
#endif  // CHIMAERA_RUNTIME

class ModuleClient {
public:
  PoolId pool_id_;

  ModuleClient() = default;
  ModuleClient(PoolId pool_id) : pool_id_(pool_id) {}

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(pool_id_);
  }
};

extern "C" {
typedef Container *(*alloc_state_t)();
typedef Container *(*new_state_t)(const PoolId *pool_id, const char *pool_name);
typedef const char *(*get_module_name_t)(void);
}

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

}  // namespace chi

#endif  // CHI_CHIMAERA_MODULE_H_