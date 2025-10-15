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

void Task::Wait(double wait_time_us, bool from_yield) {
  auto *chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager && chimaera_manager->IsRuntime()) {
    // Runtime implementation: Estimate load and yield execution

    // Get current run context from worker
    Worker *worker = CHI_CUR_WORKER;
    RunContext *run_ctx = worker ? worker->GetCurrentRunContext() : nullptr;

    if (!worker || !run_ctx) {
      // No worker or run context available, fall back to client implementation
      while (!IsComplete()) {
        YieldBase();
      }
      return;
    }

    // Determine the actual wait time to use
    double actual_wait_time_us = wait_time_us;

    // If wait_time is 0, use Monitor to estimate task weight
    if (wait_time_us == 0.0) {
      // Use container from RunContext to estimate load
      Container *container = worker ? worker->GetCurrentContainer() : nullptr;
      if (container) {
        // Estimate completion time using Monitor with kEstLoad
        // Use run_ctx directly - Monitor should update
        // estimated_completion_time_us
        container->Monitor(MonitorModeId::kEstLoad, method_, run_ctx->task,
                           *run_ctx);

        // The estimated time should be stored in
        // run_ctx->estimated_completion_time_us
        actual_wait_time_us = run_ctx->estimated_completion_time_us;
      } else {
        // No container available, use default estimate
        actual_wait_time_us = 1000.0; // Default 1ms estimate
      }
    }

    // Check if task is already blocked - this should never happen
    if (run_ctx->is_blocked) {
      HELOG(kFatal,
            "Worker {}: Task is already blocked when calling Wait()! "
            "Task ptr: {:#x}, Pool: {}, Method: {}, TaskId: {}.{}.{}.{}.{}",
            worker->GetId(), reinterpret_cast<uintptr_t>(this), pool_id_,
            method_, task_id_.pid_, task_id_.tid_, task_id_.major_,
            task_id_.replica_id_, task_id_.unique_);
      std::abort();
    }

    // Add this task to the current task's waiting_for_tasks list
    // This ensures AreSubtasksCompleted() properly tracks this subtask
    // Skip if called from yield to avoid double tracking
    if (!from_yield) {
      auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
      hipc::FullPtr<Task> this_task_ptr(alloc, this);
      run_ctx->waiting_for_tasks.push_back(this_task_ptr);
    }

    // Yield execution back to worker in loop until task completes
    // Add to blocked queue before each yield with the determined wait time
    // NOTE(llogan): This will only be unblocked when all subtasks are complete
    // No need for a while loop here.
    worker->AddToBlockedQueue(run_ctx, actual_wait_time_us);
    YieldBase();
  } else {
    // Client implementation: Wait loop using Yield()
    while (!IsComplete()) {
      YieldBase();
    }
  }
}

void Task::YieldBase() {
  auto *chimaera_manager = CHI_CHIMAERA_MANAGER;
  if (chimaera_manager && chimaera_manager->IsRuntime()) {
    // Get current run context from worker
    Worker *worker = CHI_CUR_WORKER;
    RunContext *run_ctx = worker ? worker->GetCurrentRunContext() : nullptr;

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
    void *yield_data = run_ctx->yield_context.data;

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

void Task::Yield(double wait_time_us) {
  // New public Yield function that calls Wait with from_yield=true
  // to avoid adding subtasks to RunContext
  Wait(wait_time_us, true);
}

bool Task::IsComplete() const {
  // Completion check (works for both client and runtime modes)
  return is_complete_.load() != 0;
}

} // namespace chi