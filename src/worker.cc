/**
 * Worker implementation
 */

#include "chimaera/worker.h"

#include <boost/context/detail/fcontext.hpp>
#include <cstdlib>
#include <limits>

#include "chimaera/pool_manager.h"
#include "chimaera/singletons.h"
#include "chimaera/task_queue.h"
#include "chimaera/work_orchestrator.h"

namespace chi {

Worker::Worker(u32 worker_id, ThreadType thread_type)
    : worker_id_(worker_id),
      thread_type_(thread_type),
      is_running_(false),
      is_initialized_(false),
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

  // Initialize stack pool (using malloc instead of hipc::StackAllocator)
  stack_pool_.clear();

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

  // Cleanup stack pool
  for (auto& stack_info : stack_pool_) {
    if (stack_info.stack_ptr) {
      free(stack_info.stack_ptr);
    }
    if (stack_info.run_ctx_ptr) {
      // Call destructor before freeing
      stack_info.run_ctx_ptr->~RunContext();
      free(stack_info.run_ctx_ptr);
    }
  }
  stack_pool_.clear();

  // Clear active queue reference (don't delete - it's in shared memory)
  active_queue_ =
      hipc::FullPtr<chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>();

  is_initialized_ = false;
}

void Worker::Run() {
  if (!is_initialized_) {
    return;
  }

  is_running_ = true;

  // Main worker loop - pop lanes from active queue and process tasks
  while (is_running_) {
    bool task_found = false;
    hipc::FullPtr<TaskQueue::TaskLane> lane_ptr;

    // Pop a lane from the active queue
    if (!active_queue_.IsNull() && !active_queue_->pop(lane_ptr).IsNull()) {
      if (!lane_ptr.IsNull()) {
        // Process up to 64 tasks from this specific lane
        const u32 MAX_TASKS_TOTAL = 64;
        u32 tasks_processed = 0;
        bool should_re_enqueue = false;

        while (tasks_processed < MAX_TASKS_TOTAL && is_running_) {
          hipc::TypedPointer<Task> task_typed_ptr;

          // Use static method to pop task from lane
          if (TaskQueue::PopTask(lane_ptr, task_typed_ptr)) {
            task_found = true;
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
                    // ExecuteTask)
                    ExecuteTask(task_full_ptr, container, nullptr);
                  }
                }
              }
            }

            // Check if lane still has tasks after processing this one
            // We can check the lane size, but we need to cast back to proper
            // type first For simplicity, assume if we processed the max we
            // should re-enqueue
            if (tasks_processed < MAX_TASKS_TOTAL) {
              should_re_enqueue = true;  // There might be more tasks
            }
          } else {
            // No more tasks in this lane
            break;
          }
        }

        // Re-enqueue the lane if it still has tasks and we processed the
        // maximum
        if (should_re_enqueue) {
          active_queue_->push(lane_ptr);
        }
      }
    }

    // Check blocked queue for completed tasks at end of each iteration
    u32 sleep_time_us = CheckBlockedQueue();

    if (!task_found) {
      // No new tasks available, use sleep time from CheckBlockedQueue
      if (sleep_time_us == 0) {
        // Immediate recheck needed, just yield briefly
        HSHM_THREAD_MODEL->Yield();
      } else if (sleep_time_us == std::numeric_limits<u32>::max()) {
        // No blocked tasks, yield briefly
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
  // First, try to find an available stack of the same size
  for (auto& stack_info : stack_pool_) {
    if (!stack_info.in_use && stack_info.stack_size == size) {
      stack_info.in_use = true;
      // Set stack pointer in RunContext
      stack_info.run_ctx_ptr->stack_ptr = stack_info.stack_ptr;
      stack_info.run_ctx_ptr->stack_size = size;
      return stack_info.run_ctx_ptr;
    }
  }

  // If no suitable stack found, allocate both stack and RunContext using malloc
  void* new_stack = malloc(size);
  RunContext* new_run_ctx =
      static_cast<RunContext*>(malloc(sizeof(RunContext)));

  if (new_stack && new_run_ctx) {
    // Initialize RunContext using placement new
    new (new_run_ctx) RunContext();
    // Set stack pointer in RunContext
    new_run_ctx->stack_ptr = new_stack;
    new_run_ctx->stack_size = size;

    stack_pool_.emplace_back(new_stack, new_run_ctx, size);
    stack_pool_.back().in_use = true;

    return new_run_ctx;
  }

  // Cleanup on failure
  if (new_stack) free(new_stack);
  if (new_run_ctx) free(new_run_ctx);

  return nullptr;
}

void Worker::DeallocateStackAndContext(RunContext* run_ctx_ptr) {
  if (!run_ctx_ptr) {
    return;
  }

  // Mark the stack and context as available for reuse instead of freeing them
  for (auto& stack_info : stack_pool_) {
    if (stack_info.stack_ptr == run_ctx_ptr->stack_ptr &&
        stack_info.run_ctx_ptr == run_ctx_ptr &&
        stack_info.stack_size == run_ctx_ptr->stack_size) {
      stack_info.in_use = false;
      return;
    }
  }

  // If stack not found in pool (shouldn't happen), just ignore
}

void Worker::ExecuteTask(const FullPtr<Task>& task_ptr, ChiContainer* container,
                         Lane* lane) {
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
  run_ctx_ptr->current_task = task_ptr;  // Store task in RunContext
  run_ctx_ptr->is_blocked = false;       // Initially not blocked
  run_ctx_ptr->container =
      static_cast<void*>(container);  // Store container for CHI_CUR_CONTAINER
  run_ctx_ptr->lane = static_cast<void*>(lane);  // Store lane for CHI_CUR_LANE
  run_ctx_ptr->waiting_for_tasks.clear();  // Clear waiting tasks for new task

  // Use unified execution function
  ExecuteTaskUnified(task_ptr, run_ctx_ptr, false);
}

void Worker::ExecuteTaskUnified(const FullPtr<Task>& task_ptr,
                                RunContext* run_ctx_ptr, bool is_started) {
  if (task_ptr.IsNull() || !run_ctx_ptr) {
    return;
  }

  // Set RunContext pointer in task
  task_ptr->run_ctx_ = run_ctx_ptr;

  // Set thread-local storage variables
  CHI_SET_CUR_WORKER(this);
  CHI_SET_CUR_RCTX(run_ctx_ptr);

  namespace bctx = boost::context::detail;

  if (is_started) {
    // Resume execution - the task's fiber context is already set up
    // Resume fiber execution using stored transfer data
    run_ctx_ptr->fiber_transfer = bctx::jump_fcontext(
        run_ctx_ptr->fiber_context, run_ctx_ptr->fiber_transfer.data);
  } else {
    // New task execution - stack is required to be available
    if (!run_ctx_ptr->stack_ptr) {
      // FATAL: Stack is required for task execution
      HELOG(kFatal,
            "Worker {}: Task execution requires stack to be allocated. Task "
            "method: {}, pool: {}",
            worker_id_, task_ptr->method_, task_ptr->pool_id_);
      std::abort();  // Fatal failure - stack is mandatory
    }

    // Create fiber context for this task and store directly in RunContext
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

  // Retrieve task completion status from RunContext
  bool task_completed = (run_ctx_ptr->runtime_data != nullptr);

  // Clear thread-local storage and task reference
  run_ctx_ptr->current_task = FullPtr<Task>::GetNull();
  CHI_CLEAR_CUR_RCTX();
  CHI_CLEAR_CUR_WORKER();

  // Clear RunContext pointer in task
  task_ptr->run_ctx_ = nullptr;

  // Only deallocate stack if task is marked complete
  if (task_completed) {
    DeallocateStackAndContext(run_ctx_ptr);
  }
  // If task is incomplete (failed or periodic), keep stack allocated for reuse
}

bool Worker::TaskExecutionFunction(const FullPtr<Task>& task_ptr,
                                   const RunContext& run_ctx) {
  if (task_ptr.IsNull()) {
    return false;
  }

  // Task execution function - calls container's Run function
  // Use container pointer from RunContext instead of CHI_POOL_MANAGER

  try {
    // Get the container from RunContext
    ChiContainer* container = static_cast<ChiContainer*>(run_ctx.container);

    if (container) {
      // Call the container's Run function with the task
      container->Run(task_ptr->method_, task_ptr,
                     const_cast<RunContext&>(run_ctx));

      // Task execution completed successfully
      return true;
    } else {
      // Container not found - this is an error condition
      HILOG(kWarning, "Container not found in RunContext for pool_id: {}",
            task_ptr->pool_id_);
      return false;
    }
  } catch (const std::exception& e) {
    // Handle execution errors
    HELOG(kError, "Task execution failed: {}", e.what());
    return false;
  }
}

void Worker::AddToBlockedQueue(const FullPtr<Task>& task_ptr,
                               RunContext* run_ctx_ptr,
                               double estimated_time_us) {
  if (task_ptr.IsNull() || !run_ctx_ptr) {
    return;
  }

  // Add task to the blocked queue (priority queue)
  blocked_queue_.emplace(task_ptr, run_ctx_ptr, estimated_time_us);
}

u32 Worker::CheckBlockedQueue() {
  // Always check tasks from the front of the queue
  while (!blocked_queue_.empty()) {
    const BlockedTask& blocked_task = blocked_queue_.top();

    if (!blocked_task.task_ptr.IsNull() && blocked_task.run_ctx_ptr) {
      // Always check if all subtasks are completed first
      if (blocked_task.run_ctx_ptr->AreSubtasksCompleted()) {
        // All subtasks are completed, resume this task immediately
        blocked_task.run_ctx_ptr->is_blocked = false;

        // Use unified execution function to resume the task
        ExecuteTaskUnified(blocked_task.task_ptr, blocked_task.run_ctx_ptr,
                           true);

        // Remove from queue and continue checking next task
        blocked_queue_.pop();
        continue;
      }

      // Subtasks not completed - calculate time only once for the first blocked
      // task
      auto current_time = std::chrono::steady_clock::now();
      auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            current_time - blocked_task.block_time)
                            .count();

      // Calculate remaining time until estimated completion
      if (elapsed_us >= blocked_task.estimated_completion_time_us) {
        // Time estimate exceeded, return 0 to indicate immediate recheck needed
        return 0;
      } else {
        // Return remaining time until estimated completion
        return static_cast<u32>(blocked_task.estimated_completion_time_us -
                                elapsed_us);
      }
    } else {
      // Invalid task, remove from queue and continue
      blocked_queue_.pop();
    }
  }

  // No blocked tasks remaining, worker can sleep indefinitely (return max
  // value)
  return std::numeric_limits<u32>::max();
}

void Worker::FiberExecutionFunction(boost::context::detail::transfer_t t) {
  namespace bctx = boost::context::detail;

  // This function runs in the fiber context
  // Use thread-local storage to get context
  Worker* worker = CHI_CUR_WORKER;
  RunContext* run_ctx = CHI_CUR_RCTX;
  FullPtr<Task> current_task_ptr = CHI_CUR_TASK;

  bool task_completed = false;
  if (!current_task_ptr.IsNull() && worker && run_ctx) {
    // Execute the task using the RunContext stored in the task
    bool execution_success =
        worker->TaskExecutionFunction(current_task_ptr, *run_ctx);

    // Mark task completion status before returning from fiber
    // Don't mark periodic tasks as complete
    if (execution_success && !current_task_ptr->IsPeriodic()) {
      task_completed = true;
    }
  }

  // Store completion status in RunContext for access after fiber return
  if (run_ctx) {
    run_ctx->runtime_data =
        task_completed ? reinterpret_cast<void*>(1) : nullptr;
  }

  // Jump back to main context when done
  bctx::jump_fcontext(t.fctx, t.data);
}

}  // namespace chi