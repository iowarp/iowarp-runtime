/**
 * Worker implementation
 */

#include "chimaera/worker.h"

#include <boost/context/detail/fcontext.hpp>
#include <cstdlib>
#include <iostream>

#include "chimaera/pool_manager.h"
#include "chimaera/singletons.h"
#include "chimaera/task_queue.h"
#include "chimaera/work_orchestrator.h"

namespace chi {

// Stack detection is now handled by WorkOrchestrator during initialization

Worker::Worker(u32 worker_id, ThreadType thread_type)
    : worker_id_(worker_id),
      thread_type_(thread_type),
      is_running_(false),
      is_initialized_(false),
      did_work_(false),
      current_run_context_(nullptr) {}

Worker::~Worker() {
  if (is_initialized_) {
    Finalize();
  }
}

bool Worker::Init() {
  if (is_initialized_) {
    return true;
  }

  // Stack management simplified - no pool needed

  // Get active queue from shared memory via IPC Manager
  IpcManager* ipc = CHI_IPC;
  if (ipc) {
    active_queue_ = ipc->GetWorkerQueue(worker_id_);
  }

  is_initialized_ = true;
  return true;
}

void Worker::Finalize() {
  if (!is_initialized_) {
    return;
  }

  Stop();

  // Stack management simplified - stacks are freed individually when tasks
  // complete

  // Clear active queue reference (don't delete - it's in shared memory)
  active_queue_ =
      hipc::FullPtr<chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>();

  is_initialized_ = false;
}

void Worker::Run() {
  if (!is_initialized_) {
    return;
  }

  // Set current worker once for the entire thread duration
  SetAsCurrentWorker();
  is_running_ = true;

  // Main worker loop - pop lanes from active queue and process tasks
  while (is_running_) {
    did_work_ = false;  // Reset work tracker at start of each loop iteration
    hipc::FullPtr<TaskQueue::TaskLane> lane_ptr;

    // Pop a lane from the active queue
    if (!active_queue_.IsNull() && !active_queue_->pop(lane_ptr).IsNull()) {
      if (!lane_ptr.IsNull()) {
        // Process up to 64 tasks from this specific lane
        const u32 MAX_TASKS_TOTAL = 64;
        u32 tasks_processed = 0;

        while (tasks_processed < MAX_TASKS_TOTAL && is_running_) {
          hipc::TypedPointer<Task> task_typed_ptr;

          // Use static method to pop task from lane
          if (TaskQueue::PopTask(lane_ptr, task_typed_ptr)) {
            tasks_processed++;

            // Convert TypedPointer to FullPtr for consistent API usage
            hipc::FullPtr<Task> task_full_ptr(task_typed_ptr);

            if (!task_full_ptr.IsNull()) {
              // Resolve domain query and route task to container
              if (ResolveDomainQuery(task_full_ptr)) {
                ChiContainer* container =
                    QueryContainerFromPoolManager(task_full_ptr);
                if (container) {
                  // Call monitor function with kLocalSchedule to map task to
                  // lane
                  if (CallMonitorForLocalSchedule(container, task_full_ptr)) {
                    // Execute the task (RunContext allocated within
                    // BeginTask)
                    BeginTask(task_full_ptr, container, lane_ptr.ptr_);
                  }
                }
              }
            }
          } else {
            // No more tasks in this lane
            break;
          }
        }

        // Re-enqueue the lane if it still has tasks remaining
        if (lane_ptr->GetSize() > 0) {
          active_queue_->push(lane_ptr);
        }
      }
    }

    // Check blocked queue for completed tasks at end of each iteration
    u32 sleep_time_us = ContinueBlockedTasks();

    if (!did_work_) {
      // No work was done in this iteration - safe to yield/sleep
      if (sleep_time_us == 0) {
        // No blocked tasks or immediate recheck needed, just yield briefly
        HSHM_THREAD_MODEL->Yield();
      } else {
        // Sleep for the calculated time until next blocked task should be
        // checked
        HSHM_THREAD_MODEL->SleepForUs(static_cast<size_t>(sleep_time_us));
      }
    }
  }
}

void Worker::Stop() { is_running_ = false; }

void Worker::EnqueueLane(hipc::FullPtr<TaskQueue::TaskLane> lane_ptr) {
  if (lane_ptr.IsNull() || active_queue_.IsNull()) {
    return;
  }

  // Enqueue lane FullPtr to active queue (lock-free)
  active_queue_->push(lane_ptr);
}

u32 Worker::GetId() const { return worker_id_; }

ThreadType Worker::GetThreadType() const { return thread_type_; }

bool Worker::IsRunning() const { return is_running_; }

RunContext* Worker::GetCurrentRunContext() const {
  return current_run_context_;
}

RunContext* Worker::SetCurrentRunContext(RunContext* rctx) {
  current_run_context_ = rctx;
  return current_run_context_;
}

FullPtr<Task> Worker::GetCurrentTask() const {
  RunContext* run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return FullPtr<Task>::GetNull();
  }
  return run_ctx->task;
}

ChiContainer* Worker::GetCurrentContainer() const {
  RunContext* run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return nullptr;
  }
  return static_cast<ChiContainer*>(run_ctx->container);
}

TaskQueue::TaskLane* Worker::GetCurrentLane() const {
  RunContext* run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return nullptr;
  }
  return static_cast<TaskQueue::TaskLane*>(run_ctx->lane);
}

void Worker::SetAsCurrentWorker() {
  HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<class Worker*>(this));
}

void Worker::ClearCurrentWorker() {
  HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<class Worker*>(nullptr));
}

Task* Worker::PopActiveTask() {
  // Stub implementation - this method is now deprecated
  // Task processing is handled directly in Worker::Run() through lane-based
  // processing
  return nullptr;
}

bool Worker::ResolveDomainQuery(const FullPtr<Task>& task_ptr) {
  if (task_ptr.IsNull()) {
    return false;
  }

  // Resolve DomainQuery stored in the task to route to a specific container
  // For now, this routes the task to a container on this node based on PoolId
  // and DomainQuery

  // Basic validation of domain query
  // For now, we assume the domain query is valid if the task has a valid
  // pool_id
  if (task_ptr->pool_id_ == 0) {
    return false;  // Invalid pool ID
  }

  // Here we would implement the actual domain resolution logic
  // For now, we assume the task is valid for local processing
  return true;
}

ChiContainer* Worker::QueryContainerFromPoolManager(
    const FullPtr<Task>& task_ptr) {
  if (task_ptr.IsNull()) {
    return nullptr;
  }

  // Query container from the PoolManager based on task's PoolId
  // Using singleton access pattern similar to other components
  return CHI_POOL_MANAGER->GetContainer(task_ptr->pool_id_);
}

bool Worker::CallMonitorForLocalSchedule(ChiContainer* container,
                                         const FullPtr<Task>& task_ptr) {
  if (container == nullptr || task_ptr.IsNull()) {
    return false;
  }

  // Call the monitor function with kLocalSchedule to map the task to a lane
  RunContext run_ctx = CreateRunContext(task_ptr);

  try {
    container->Monitor(MonitorModeId::kLocalSchedule, task_ptr->method_,
                       task_ptr, run_ctx);
    return true;
  } catch (const std::exception& e) {
    // Monitor function failed
    return false;
  }
}

RunContext Worker::CreateRunContext(const FullPtr<Task>& task_ptr) {
  // This method is deprecated - use AllocateStackAndContext instead
  // Creating a temporary RunContext for compatibility
  RunContext run_ctx;
  run_ctx.thread_type = thread_type_;
  run_ctx.worker_id = worker_id_;
  run_ctx.stack_size = 65536;   // 64KB
  run_ctx.stack_ptr = nullptr;  // Will be set by AllocateStackAndContext
  return run_ctx;
}

RunContext* Worker::AllocateStackAndContext(size_t size) {
  // Allocate both stack and RunContext using malloc
  void* stack_base = malloc(size);
  RunContext* new_run_ctx =
      static_cast<RunContext*>(malloc(sizeof(RunContext)));

  if (stack_base && new_run_ctx) {
    // Initialize RunContext using placement new
    new (new_run_ctx) RunContext();

    // Store the malloc base pointer for freeing later
    new_run_ctx->stack_base_for_free = stack_base;
    new_run_ctx->stack_size = size;

    // Set the correct stack pointer based on stack growth direction from work
    // orchestrator
    WorkOrchestrator* orchestrator = CHI_WORK_ORCHESTRATOR;
    bool grows_downward = orchestrator ? orchestrator->IsStackDownward()
                                       : true;  // Default to downward

    if (grows_downward) {
      // Stack grows downward: point to the end of the malloc buffer
      new_run_ctx->stack_ptr = static_cast<char*>(stack_base) + size;
    } else {
      // Stack grows upward: point to the beginning of the malloc buffer
      new_run_ctx->stack_ptr = stack_base;
    }

    return new_run_ctx;
  }

  // Cleanup on failure
  if (stack_base) free(stack_base);
  if (new_run_ctx) free(new_run_ctx);

  return nullptr;
}

void Worker::DeallocateStackAndContext(RunContext* run_ctx_ptr) {
  if (!run_ctx_ptr) {
    return;
  }

  // Free the stack using the original malloc base pointer
  if (run_ctx_ptr->stack_base_for_free) {
    free(run_ctx_ptr->stack_base_for_free);
  }

  // Call destructor explicitly before freeing
  run_ctx_ptr->~RunContext();
  free(run_ctx_ptr);
}

void Worker::BeginTask(const FullPtr<Task>& task_ptr, ChiContainer* container,
                       TaskQueue::TaskLane* lane) {
  if (task_ptr.IsNull()) {
    return;
  }

  // Allocate stack and RunContext together for new task
  RunContext* run_ctx_ptr = AllocateStackAndContext(65536);  // 64KB default

  if (!run_ctx_ptr) {
    // FATAL: Stack allocation failure - this is a critical error
    HELOG(kFatal,
          "Worker {}: Failed to allocate stack for task execution. Task "
          "method: {}, pool: {}",
          worker_id_, task_ptr->method_, task_ptr->pool_id_);
    std::abort();  // Fatal failure
  }

  // Initialize RunContext for new task
  run_ctx_ptr->thread_type = thread_type_;
  run_ctx_ptr->worker_id = worker_id_;
  run_ctx_ptr->task = task_ptr;     // Store task in RunContext
  run_ctx_ptr->is_blocked = false;  // Initially not blocked
  run_ctx_ptr->container =
      static_cast<void*>(container);  // Store container for CHI_CUR_CONTAINER
  run_ctx_ptr->lane = static_cast<void*>(lane);  // Store lane for CHI_CUR_LANE
  run_ctx_ptr->waiting_for_tasks.clear();  // Clear waiting tasks for new task
  // Set RunContext pointer in task
  task_ptr->run_ctx_ = run_ctx_ptr;

  // Use unified execution function
  ExecTask(task_ptr, run_ctx_ptr, false);
}

u32 Worker::ContinueBlockedTasks() {
  // Always check tasks from the front of the queue
  while (!blocked_queue_.empty()) {
    RunContext* run_ctx_ptr = blocked_queue_.top();

    if (run_ctx_ptr && !run_ctx_ptr->task.IsNull()) {
      // Always check if all subtasks are completed first
      if (run_ctx_ptr->AreSubtasksCompleted()) {
        // All subtasks are completed, resume this task immediately
        run_ctx_ptr->is_blocked = false;

        // Remove from queue BEFORE calling ExecTask to prevent duplicate
        // entries if ExecTask calls AddToBlockedQueue
        blocked_queue_.pop();

        // Use unified execution function to resume the task
        ExecTask(run_ctx_ptr->task, run_ctx_ptr, true);

        // Continue checking next task
        continue;
      }

      // Subtasks not completed - calculate time using HSHM timepoint
      hshm::Timepoint current_time;
      current_time.Now();
      double elapsed_us =
          current_time.GetUsecFromStart(run_ctx_ptr->block_time);

      // Calculate remaining time until estimated completion
      if (elapsed_us >= run_ctx_ptr->estimated_completion_time_us) {
        // Time estimate exceeded, return 0 to indicate immediate recheck needed
        return 0;
      } else {
        // Return remaining time until estimated completion
        return static_cast<u32>(run_ctx_ptr->estimated_completion_time_us -
                                elapsed_us);
      }
    } else {
      // Invalid run context, remove from queue and continue
      blocked_queue_.pop();
    }
  }

  // No blocked tasks remaining, return 0 for immediate recheck
  return 0;
}

void Worker::ExecTask(const FullPtr<Task>& task_ptr, RunContext* run_ctx_ptr,
                      bool is_started) {
  if (task_ptr.IsNull() || !run_ctx_ptr) {
    return;
  }

  // Mark that work is being done
  did_work_ = true;

  // Set current run context
  // Note: Current worker is already set for thread duration
  SetCurrentRunContext(run_ctx_ptr);

  namespace bctx = boost::context::detail;

  if (is_started) {
    // Resume execution - the task's fiber context is already set up
    // Resume fiber execution using stored transfer data
    run_ctx_ptr->fiber_transfer = bctx::jump_fcontext(
        run_ctx_ptr->fiber_context, run_ctx_ptr->fiber_transfer.data);
  } else {
    // New task execution
    // Create fiber context for this task and store directly in RunContext
    // stack_ptr is already correctly positioned based on stack growth direction
    run_ctx_ptr->fiber_context =
        bctx::make_fcontext(run_ctx_ptr->stack_ptr, run_ctx_ptr->stack_size,
                            FiberExecutionFunction);

    // Jump to fiber context to execute the task
    run_ctx_ptr->fiber_transfer =
        bctx::jump_fcontext(run_ctx_ptr->fiber_context, nullptr);
  }

  // Common cleanup logic for both fiber and direct execution
  if (run_ctx_ptr->is_blocked) {
    // Task is blocked - don't clean up, will be resumed later
    return;
  }

  // Tasks should never return incomplete from ExecTask
  // Periodic tasks are rescheduled by ReschedulePeriodicTask in FiberExecutionFunction
  // Non-periodic tasks are marked complete in FiberExecutionFunction
  
  // Only clean up completed non-periodic tasks
  bool task_completed = (task_ptr->is_complete.load() != 0);
  if (task_completed) {
    // Clear RunContext pointer and deallocate stack for completed tasks
    task_ptr->run_ctx_ = nullptr;
    DeallocateStackAndContext(run_ctx_ptr);
  }
  // Periodic tasks (whether staying or sent elsewhere) keep their resources
}

void Worker::AddToBlockedQueue(RunContext* run_ctx_ptr,
                               double estimated_time_us) {
  if (!run_ctx_ptr || run_ctx_ptr->task.IsNull()) {
    return;
  }

  // Set timing information in RunContext
  run_ctx_ptr->estimated_completion_time_us = estimated_time_us;
  run_ctx_ptr->block_time.Now();

  // Add RunContext to the blocked queue (priority queue)
  blocked_queue_.push(run_ctx_ptr);
}

void Worker::ReschedulePeriodicTask(RunContext* run_ctx_ptr, const FullPtr<Task>& task_ptr) {
  if (!run_ctx_ptr || task_ptr.IsNull() || !task_ptr->IsPeriodic()) {
    return;
  }

  // Get the lane from the run context
  TaskQueue::TaskLane* lane = static_cast<TaskQueue::TaskLane*>(run_ctx_ptr->lane);
  if (!lane) {
    // No lane information, cannot reschedule
    return;
  }

  // Check if the lane still maps to this worker by checking the lane header
  auto& header = lane->GetHeader();
  if (header.assigned_worker_id == worker_id_) {
    // Lane still maps to this worker - add to blocked queue for timed execution
    double period_us = task_ptr->GetPeriod(kMicro);
    AddToBlockedQueue(run_ctx_ptr, period_us);
  } else {
    // Lane has been reassigned to a different worker - reschedule task in the lane
    // Convert task FullPtr to TypedPointer for lane enqueueing
    hipc::TypedPointer<Task> task_typed_ptr(task_ptr.shm_);
    hipc::FullPtr<TaskQueue::TaskLane> lane_full_ptr(lane);
    TaskQueue::EmplaceTask(lane_full_ptr, task_typed_ptr);
  }
}

void Worker::FiberExecutionFunction(boost::context::detail::transfer_t t) {
  namespace bctx = boost::context::detail;

  // This function runs in the fiber context
  // Use thread-local storage to get context
  Worker* worker = CHI_CUR_WORKER;
  RunContext* run_ctx = worker->GetCurrentRunContext();
  FullPtr<Task> task_ptr = worker ? worker->GetCurrentTask() : FullPtr<Task>::GetNull();

  if (!task_ptr.IsNull() && worker && run_ctx) {
    // Execute the task directly - merged TaskExecutionFunction logic
    try {
      // Get the container from RunContext
      ChiContainer* container = static_cast<ChiContainer*>(run_ctx->container);

      if (container) {
        // Call the container's Run function with the task
        container->Run(task_ptr->method_, task_ptr, *run_ctx);
      } else {
        // Container not found - this is an error condition
        HILOG(kWarning, "Container not found in RunContext for pool_id: {}",
              task_ptr->pool_id_);
      }
    } catch (const std::exception& e) {
      // Handle execution errors
      HELOG(kError, "Task execution failed: {}", e.what());
    } catch (...) {
      // Handle unknown errors
      HELOG(kError, "Task execution failed with unknown exception");
    }

    // Handle task completion and rescheduling
    if (task_ptr->IsPeriodic()) {
      // Periodic tasks are always rescheduled regardless of execution success
      worker->ReschedulePeriodicTask(run_ctx, task_ptr);
    } else {
      // Non-periodic task completed - mark as complete regardless of success/failure
      task_ptr->is_complete.store(1);
    }
  }

  // Jump back to main context when done
  bctx::jump_fcontext(t.fctx, t.data);
}

}  // namespace chi