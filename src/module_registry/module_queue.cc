#include "chimaera/module_registry/module_queue.h"
#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/work_orchestrator/worker.h"

namespace chi {

/**
 * Forward declaration of Lane:push templates
 * This was because ROCM compiler was not able to resolve the template
 * without this.
 */
template hshm::qtok_t Lane::push<false>(const FullPtr<Task> &task);
template hshm::qtok_t Lane::push<true>(const FullPtr<Task> &task);

/** Emplace constructor */
Lane::Lane(LaneId lane_id, TaskPrio prio, LaneGroupId group_id,
           WorkerId worker_id)
    : lane_id_(lane_id), prio_(prio), group_id_(group_id),
      worker_id_(worker_id) {
  plug_count_ = 0;
  count_ = (hshm::min_u64)0;
  auto *runtime = CHI_RUNTIME;
  active_tasks_.resize(runtime->server_config_->queue_manager_.lane_depth_);
}

/** Copy constructor */
Lane::Lane(const Lane &lane) {
  lane_id_ = lane.lane_id_;
  worker_id_ = lane.worker_id_;
  load_ = lane.load_;
  plug_count_ = lane.plug_count_.load();
  prio_ = lane.prio_;
  auto *runtime = CHI_RUNTIME;
  active_tasks_.resize(runtime->server_config_->queue_manager_.lane_depth_);
}

/** Push a task  */
template <bool NO_COUNT> hshm::qtok_t Lane::push(const FullPtr<Task> &task) {
  Worker &worker = CHI_WORK_ORCHESTRATOR->GetWorker(worker_id_);
  Worker *cur_worker = CHI_CUR_WORKER;
  if (!cur_worker || worker.id_ != cur_worker->id_) {
    worker.active_.GetFail().push(task);
    return hshm::qtok_t();
  }
  if constexpr (!NO_COUNT) {
    size_t dup = count_.fetch_add(1);
    if (dup == 0) {
      HLOG(kDebug, kWorkerDebug,
           "Requesting lane {} with count {} with task {}", this, dup,
           task.ptr_);
      worker.RequestLane(this);
    } else {
      HLOG(kDebug, kWorkerDebug, "Skipping lane {} with count {} with task {}",
           this, dup, task.ptr_);
    }
  }
  hshm::qtok_t ret = active_tasks_.push(task);
  return ret;
}

/** Pop a set of tasks in sequence */
size_t Lane::pop_prep(size_t count) { return count_.fetch_sub(count) - count; }

/** Pop a task */
hshm::qtok_t Lane::pop(FullPtr<Task> &task) {
  hshm::qtok_t ret = active_tasks_.pop(task);
  if (!ret.IsNull() && !task->IsLongRunning()) {
    HLOG(kDebug, kWorkerDebug, "Popping task {} from {}", task.ptr_, this);
  }
  return ret;
}

} // namespace chi