/**
 * Task implementation
 */

#include "chimaera/task.h"

#include "chimaera/chimod_spec.h"

#ifdef CHIMAERA_RUNTIME
#include "chimaera/singletons.h"
#include "chimaera/worker.h"
#endif

namespace chi {

void Task::Wait() {
#ifdef CHIMAERA_RUNTIME
  // Runtime implementation: Estimate load and yield execution

  // Get current run context
  RunContext* run_ctx = CHI_CUR_RCTX;

  if (!run_ctx) {
    return;  // Cannot wait without run context
  }

  // Use container from RunContext instead of CHI_POOL_MANAGER
  ChiContainer* container = CHI_CUR_CONTAINER;
  if (container) {
    // Estimate completion time using Monitor with kEstLoad
    RunContext est_ctx = *run_ctx;  // Copy run context for estimation
    try {
      container->Monitor(MonitorModeId::kEstLoad, method_, run_ctx->task,
                         est_ctx);

      // The estimated time should be stored in
      // est_ctx.estimated_completion_time_us
      run_ctx->estimated_completion_time_us =
          est_ctx.estimated_completion_time_us;

    } catch (const std::exception& e) {
      // Estimation failed, fall back to default estimate
      run_ctx->estimated_completion_time_us = 1000.0;  // Default 1ms estimate
    }
  } else {
    // No container available, use default estimate
    run_ctx->estimated_completion_time_us = 1000.0;  // Default 1ms estimate
  }

  // Add task to worker's blocked queue and yield
  Worker* worker = CHI_CUR_WORKER;
  if (worker) {
    worker->AddToBlockedQueue(run_ctx, 
                              run_ctx->estimated_completion_time_us);
  }

  // Yield execution back to worker
  Yield();

#else
  // Client implementation: Spinwait with 10 microsecond sleep
  while (!IsComplete()) {
    HSHM_THREAD_MODEL->SleepForUs(10);  // Sleep for 10 microseconds
  }
#endif
}

#ifdef CHIMAERA_RUNTIME
void Task::Yield() {
  // Get current run context
  RunContext* run_ctx = CHI_CUR_RCTX;

  if (!run_ctx) {
    return;  // Cannot yield without run context
  }

  // Mark this task as blocked
  run_ctx->is_blocked = true;

  // Jump back to worker using boost::fiber
  namespace bctx = boost::context::detail;

  // Jump back to worker - the task has been added to blocked queue
  run_ctx->fiber_transfer = bctx::jump_fcontext(run_ctx->fiber_transfer.fctx,
                                                run_ctx->fiber_transfer.data);
}
#endif

#ifndef CHIMAERA_RUNTIME
bool Task::IsComplete() const {
  // Client-side completion check
  // This would check task completion status
  // For now, stub implementation
  return false;  // TODO: Implement completion checking
}
#endif

}  // namespace chi