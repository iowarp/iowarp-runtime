#ifndef CHI_INCLUDE_CHI_MODULE_REGISTRY_MODULE_QUEUE_H_
#define CHI_INCLUDE_CHI_MODULE_REGISTRY_MODULE_QUEUE_H_

#ifdef CHIMAERA_RUNTIME

#include "chimaera/chimaera_types.h"
#include "chimaera/queue_manager/queue.h"
#include "chimaera/work_orchestrator/comutex.h"
#include "chimaera/work_orchestrator/corwlock.h"
#include "task.h"

namespace chi {

/** Local runtime lanes. These are NOT used for interprocess communication. */
class Lane {
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
  // chi::mpsc_queue<TaskPointer> active_tasks_;
  hipc::atomic<hshm::min_u64> count_;

public:
  /** Default constructor */
  Lane() = default;

  /** Emplace constructor */
  explicit Lane(LaneId lane_id, TaskPrio prio, LaneGroupId group_id,
                WorkerId worker_id);

  /** Copy constructor */
  Lane(const Lane &lane);

#ifdef CHIMAERA_RUNTIME
  /** Push a task  */
  template <bool NO_COUNT> hshm::qtok_t push(const FullPtr<Task> &task);

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

/** A group of runtime lanes */
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

} // namespace chi

#endif // CHIMAERA_RUNTIME

#endif // CHI_INCLUDE_CHI_MODULE_REGISTRY_MODULE_QUEUE_H_