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

  // Get current run context from worker
  Worker* worker = CHI_CUR_WORKER;
  RunContext* run_ctx = worker ? worker->GetCurrentRunContext() : nullptr;

  if (!worker || !run_ctx) {
    // No worker or run context available, fall back to client implementation
    while (is_complete.load() == 0) {
      Yield();
    }
    return;
  }

  // Use container from RunContext instead of CHI_POOL_MANAGER
  ChiContainer* container = worker ? worker->GetCurrentContainer() : nullptr;
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
  // Note: worker variable already retrieved above
  if (worker) {
    worker->AddToBlockedQueue(run_ctx, 
                              run_ctx->estimated_completion_time_us);
  }

  // Yield execution back to worker
  Yield();

#else
  // Client implementation: Wait loop using Yield()
  while (!IsComplete()) {
    Yield();
  }
#endif
}

void Task::Yield() {
#ifdef CHIMAERA_RUNTIME
  // Get current run context from worker
  Worker* worker = CHI_CUR_WORKER;
  RunContext* run_ctx = worker ? worker->GetCurrentRunContext() : nullptr;

  if (!run_ctx) {
    // No run context available, fall back to client implementation
    HSHM_THREAD_MODEL->Yield();
    return;
  }

  // Mark this task as blocked
  run_ctx->is_blocked = true;

  // Jump back to worker using boost::fiber

  // Jump back to worker - the task has been added to blocked queue
  run_ctx->fiber_transfer = bctx::jump_fcontext(run_ctx->fiber_transfer.fctx,
                                                run_ctx->fiber_transfer.data);
#else
  // Outside CHIMAERA_RUNTIME, just yield
  HSHM_THREAD_MODEL->Yield();
#endif
}

#ifndef CHIMAERA_RUNTIME
bool Task::IsComplete() const {
  // Client-side completion check
  return is_complete.load() != 0;
}
#endif

}  // namespace chi