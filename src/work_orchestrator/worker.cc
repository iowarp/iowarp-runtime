#include "chimaera/work_orchestrator/worker.h"

#ifdef CHIMAERA_RUNTIME

#include "chimaera/ipc/ipc_manager.h"
#include "chimaera/pool_manager/pool_manager.h"
#include <boost/context/fiber_fcontext.hpp>
#include <chrono>
#include <algorithm>

namespace chi {

Worker::Worker(WorkerType type, int worker_id, int cpu_id)
    : type_(type), worker_id_(worker_id), cpu_id_(cpu_id), should_stop_(false) {}

void Worker::Start() {
  thread_ = std::thread([this]() { this->WorkerLoop(); });
}

void Worker::Stop() {
  should_stop_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Worker::AddLane(u32 lane_id) { 
  active_lanes_.emplace_back(lane_id); 
}

void Worker::MoveLaneToCold(u32 lane_id) {
  auto it = std::find(active_lanes_.begin(), active_lanes_.end(), lane_id);
  if (it != active_lanes_.end()) {
    active_lanes_.erase(it);
    cold_lanes_.emplace_back(lane_id);
  }
}

void Worker::MoveLaneToActive(u32 lane_id) {
  auto it = std::find(cold_lanes_.begin(), cold_lanes_.end(), lane_id);
  if (it != cold_lanes_.end()) {
    cold_lanes_.erase(it);
    active_lanes_.emplace_back(lane_id);
  }
}

void Worker::WorkerLoop() {
  auto ipc_manager = CHI_IPC_MANAGER;
  auto process_queue = ipc_manager->GetProcessQueue();

  HILOG(kInfo, "Worker {} started (type: {}, cpu: {})", worker_id_,
        static_cast<int>(type_), cpu_id_);

  while (!should_stop_) {
    bool found_work = false;

    for (u32 lane_id : active_lanes_) {
      TaskPointer task;
      u32 priority = (type_ == WorkerType::kLowLatency)
                         ? QueuePriority::kLowLatency
                         : QueuePriority::kHighLatency;

      if (process_queue->DequeueTask(priority, lane_id, task)) {
        found_work = true;
        ExecuteTask(task.ptr_);
      }
    }

    if (!found_work) {
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  }

  HILOG(kInfo, "Worker {} stopped", worker_id_);
}

void Worker::ExecuteTask(Task *task) {
  try {
    if (!task) return;

    RunContext rctx;
    rctx.InitForTask(task);

    DomainQuery resolved_query = ResolveDomainQuery(task->dom_query_, task);

    if (!IsLocalTask(resolved_query)) {
      ForwardToRemoteQueue(task);
      return;
    }

    Container *container = GetContainer(task);
    if (!container) {
      HELOG(kError, "Failed to get container for task, completing task");
      rctx.SetCompleted();
      return;
    }

    Lane *target_lane = RouteTaskToLane(container, task);
    if (!target_lane) {
      HELOG(kError, "Failed to route task to lane");
      return;
    }

    ExecuteInFiber(container, task, rctx);

    // If task is not completed after fiber execution and not blocked, re-enqueue it
    if (!rctx.is_completed_ && !rctx.is_blocked_) {
      TaskPointer task_ptr;
      task_ptr.ptr_ = task;
      target_lane->EnqueueTask(task_ptr);
      return;
    }

    if (rctx.is_completed_ && task->task_flags_.Any(TASK_FIRE_AND_FORGET)) {
      container->Del(hipc::MemContext(), task->method_, task);
    }

  } catch (const std::exception &e) {
    HELOG(kError, "Exception in task execution: {}", e.what());
  }
}

DomainQuery Worker::ResolveDomainQuery(const DomainQuery &query, Task *task) {
  if (query.type_ != DomainQuery::kDynamic) {
    return query;
  }

  Container *container = GetContainer(task);
  if (container) {
    RunContext rctx;
    rctx.InitForTask(task);
    container->Monitor(MonitorMode::kGlobalSchedule, task->method_, task, rctx);
  }

  return query;
}

bool Worker::IsLocalTask(const DomainQuery &query) {
  return query.type_ == DomainQuery::kLocalId ||
         query.type_ == DomainQuery::kLocalHash;
}

void Worker::ForwardToRemoteQueue(Task *task) {
  HILOG(kInfo, "Forwarding task to remote queue (not implemented)");
}

Container* Worker::GetContainer(Task *task) {
  auto pool_manager = CHI_POOL_MANAGER;
  
  // Use container ID 0 as default for now
  ContainerId container_id = 0;
  
  Container* container = pool_manager->GetContainer(task->pool_id_, container_id);
  if (!container) {
    HELOG(kError, "Container {} not found in pool {}", container_id, task->pool_id_);
    return nullptr;
  }
  
  return container;
}

Lane *Worker::RouteTaskToLane(Container *container, Task *task) {
  RunContext rctx;
  rctx.InitForTask(task);
  container->Monitor(MonitorMode::kLocalSchedule, task->method_, task, rctx);

  // Use the suggested lane from the routing context
  return container->GetLane(rctx.suggested_queue_id_, rctx.suggested_lane_id_);
}

void Worker::ExecuteInFiber(Container *container, Task *task, RunContext &rctx) {
  const size_t STACK_SIZE = 65536;

  rctx.stack_.Allocate(STACK_SIZE);

  namespace bctx = boost::context::detail;

  auto fiber_func = [&](bctx::transfer_t t) {
    rctx.fiber_ctx_ = t.fctx;

    try {
      container->Run(task->method_, task, rctx);
      rctx.SetCompleted();
    } catch (const std::exception &e) {
      HELOG(kError, "Exception in fiber execution: {}", e.what());
      rctx.SetCompleted();
    }

    return bctx::jump_fcontext(rctx.fiber_ctx_, 0);
  };

  bctx::fcontext_t fiber_ctx = bctx::make_fcontext(
      rctx.stack_.stack_ptr_, rctx.stack_.stack_size_,
      reinterpret_cast<void (*)(bctx::transfer_t)>(+[](bctx::transfer_t t) {
        auto func = reinterpret_cast<
            std::function<bctx::transfer_t(bctx::transfer_t)> *>(t.data);
        return (*func)(t);
      }));

  auto func_ptr = &fiber_func;
  bctx::jump_fcontext(fiber_ctx, func_ptr);

  // Don't wait infinitely - the caller will handle re-enqueueing if needed
}

}  // namespace chi

#endif  // CHIMAERA_RUNTIME