/**
 * Task implementation
 */

#include "chimaera/task.h"

#include "chimaera/container.h"
#include "chimaera/singletons.h"
#include "chimaera/worker.h"

// Namespace alias for boost::context::detail
namespace bctx = boost::context::detail;

namespace chi {

void Task::Wait() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager && chimaera_manager->IsRuntime()) {
    // Runtime implementation: Estimate load and yield execution

    // Get current run context from worker
    Worker* worker = CHI_CUR_WORKER;
    RunContext* run_ctx = worker ? worker->GetCurrentRunContext() : nullptr;

    if (!worker || !run_ctx) {
      // No worker or run context available, fall back to client implementation
      while (!IsComplete()) {
        YieldBase();
      }
      return;
    }

    // Use container from RunContext instead of CHI_POOL_MANAGER
    Container* container = worker ? worker->GetCurrentContainer() : nullptr;
    if (container) {
      // Estimate completion time using Monitor with kEstLoad
      RunContext est_ctx = *run_ctx;  // Copy run context for estimation
      container->Monitor(MonitorModeId::kEstLoad, method_, run_ctx->task,
                         est_ctx);

      // The estimated time should be stored in
      // est_ctx.estimated_completion_time_us
      run_ctx->estimated_completion_time_us =
          est_ctx.estimated_completion_time_us;
    } else {
      // No container available, use default estimate
      run_ctx->estimated_completion_time_us = 1000.0;  // Default 1ms estimate
    }

    // Yield execution back to worker in loop until task completes
    // Add to blocked queue before each yield
    while (!IsComplete()) {
      worker->AddToBlockedQueue(run_ctx, run_ctx->estimated_completion_time_us);
      YieldBase();
    }

  } else {
    // Client implementation: Wait loop using Yield()
    while (!IsComplete()) {
      YieldBase();
    }
  }
}

void Task::YieldBase() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager && chimaera_manager->IsRuntime()) {
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
    // Store the result (task's yield point) in resume_context for later
    // resumption Use temporary variables to store the yield context before
    // jumping
    bctx::fcontext_t yield_fctx = run_ctx->yield_context.fctx;
    void* yield_data = run_ctx->yield_context.data;

    // Jump back to worker and capture the result
    bctx::transfer_t yield_result = bctx::jump_fcontext(yield_fctx, yield_data);

    // CRITICAL: Update yield_context with the new worker context from the
    // resume operation This ensures that subsequent yields or completion
    // returns to the correct worker location
    run_ctx->yield_context = yield_result;

    // Store where we can resume from for the next yield cycle
    run_ctx->resume_context = yield_result;
  } else {
    // Outside runtime mode, just yield
    HSHM_THREAD_MODEL->Yield();
  }
}

void Task::Yield() {
  // New public Yield function that simply calls Wait
  Wait();
}

bool Task::IsComplete() const {
  // Completion check (works for both client and runtime modes)
  return is_complete.load() != 0;
}

}  // namespace chi