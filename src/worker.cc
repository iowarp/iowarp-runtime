/**
 * Worker implementation
 */

#include "chimaera/worker.h"
#include "chimaera/singletons.h"
#include "chimaera/work_orchestrator.h"
#include "chimaera/pool_manager.h"
#include <cstdlib>

namespace chi {

Worker::Worker(u32 worker_id, ThreadType thread_type) 
    : worker_id_(worker_id), thread_type_(thread_type), 
      is_running_(false), is_initialized_(false),
      active_queue_(nullptr), cold_queue_(nullptr) {
}

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

  // Set queue references based on thread type
  IpcManager* ipc = CHI_IPC;
  if (thread_type_ == kLowLatencyWorker) {
    active_queue_ = ipc->GetProcessQueue(kLowLatency);
    cold_queue_ = ipc->GetProcessQueue(kHighLatency);
  } else {
    active_queue_ = ipc->GetProcessQueue(kHighLatency);
    cold_queue_ = ipc->GetProcessQueue(kLowLatency);
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

  // Clear queue references
  active_queue_ = nullptr;
  cold_queue_ = nullptr;

  is_initialized_ = false;
}

void Worker::Run() {
  if (!is_initialized_) {
    return;
  }

  is_running_ = true;

  // Main worker loop - iterate over active lanes and pop tasks from them
  while (is_running_) {
    bool task_found = false;
    
    // Get the current set of active lanes assigned to this worker
    std::vector<LaneId> active_lanes = GetActiveLanes();
    
    // Iterate over assigned lanes
    for (LaneId lane_id : active_lanes) {
      WorkOrchestrator* orchestrator = CHI_WORK_ORCHESTRATOR;
      WorkerLane* lane = orchestrator->GetLane(lane_id);
      
      if (lane) {
        hipc::Pointer task_ptr;
        
        // Try to dequeue a task from this lane
        if (lane->Dequeue(task_ptr)) {
          task_found = true;
          
          // Convert pointer to FullPtr for consistent API usage
          hipc::FullPtr<Task> task_full_ptr(task_ptr);
          
          if (!task_full_ptr.IsNull()) {
            // Resolve domain query and route task to container
            if (ResolveDomainQuery(task_full_ptr)) {
              ChiContainer* container = QueryContainerFromPoolManager(task_full_ptr);
              if (container) {
                // Call monitor function with kLocalSchedule to map task to lane
                if (CallMonitorForLocalSchedule(container, task_full_ptr)) {
                  // Execute the task (RunContext allocated within ExecuteTask)
                  ExecuteTask(task_full_ptr);
                }
              }
            }
          }
          
          // Process one task per lane iteration to ensure fairness
          break;
        }
      }
    }
    
    if (!task_found) {
      // No tasks available, yield briefly
      HSHM_THREAD_MODEL->Yield();
    }
  }
}

void Worker::Stop() {
  is_running_ = false;
}

u32 Worker::GetId() const {
  return worker_id_;
}

ThreadType Worker::GetThreadType() const {
  return thread_type_;
}

bool Worker::IsRunning() const {
  return is_running_;
}

Task* Worker::PopActiveTask() {
  // Stub implementation - would pop from active_queue_
  return nullptr;
}

Task* Worker::PopColdTask() {
  // Stub implementation - would pop from cold_queue_
  return nullptr;
}

std::vector<LaneId> Worker::GetActiveLanes() {
  WorkOrchestrator* orchestrator = CHI_WORK_ORCHESTRATOR;
  return orchestrator->GetActiveLanesForWorker(worker_id_);
}

bool Worker::ResolveDomainQuery(const FullPtr<Task>& task_ptr) {
  if (task_ptr.IsNull()) {
    return false;
  }

  // Resolve DomainQuery stored in the task to route to a specific container
  // For now, this routes the task to a container on this node based on PoolId and DomainQuery
  
  // Basic validation of domain query
  // For now, we assume the domain query is valid if the task has a valid pool_id
  if (task_ptr->pool_id_ == 0) {
    return false; // Invalid pool ID
  }

  // Here we would implement the actual domain resolution logic
  // For now, we assume the task is valid for local processing
  return true;
}

ChiContainer* Worker::QueryContainerFromPoolManager(const FullPtr<Task>& task_ptr) {
  if (task_ptr.IsNull()) {
    return nullptr;
  }

  // Query container from the PoolManager based on task's PoolId
  // Using singleton access pattern similar to other components
  return CHI_POOL_MANAGER->GetContainer(task_ptr->pool_id_);
}

bool Worker::CallMonitorForLocalSchedule(ChiContainer* container, const FullPtr<Task>& task_ptr) {
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
  run_ctx.stack_size = 65536; // 64KB
  run_ctx.stack_ptr = nullptr; // Will be set by AllocateStackAndContext
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
  RunContext* new_run_ctx = static_cast<RunContext*>(malloc(sizeof(RunContext)));
  
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

void Worker::ExecuteTask(const FullPtr<Task>& task_ptr) {
  if (task_ptr.IsNull()) {
    return;
  }

  // Allocate stack and RunContext together
  RunContext* run_ctx_ptr = AllocateStackAndContext(65536); // 64KB default
  
  if (!run_ctx_ptr) {
    // FATAL: Stack allocation failure - this is a critical error
    HELOG(kFatal, "Worker {}: Failed to allocate stack for task execution. Task method: {}, pool: {}", 
          worker_id_, task_ptr->method_, task_ptr->pool_id_);
    std::abort(); // Fatal failure
  }
  
  // Initialize RunContext
  run_ctx_ptr->thread_type = thread_type_;
  run_ctx_ptr->worker_id = worker_id_;
  run_ctx_ptr->current_task = task_ptr;  // Store task in RunContext
  
  // Set RunContext pointer in task
  task_ptr->run_ctx_ = run_ctx_ptr;
  
  // Set thread-local storage variables  
  CHI_SET_CUR_RCTX(run_ctx_ptr);
  CHI_SET_CUR_WORKER(this);
  
  namespace bctx = boost::context::detail;

  if (run_ctx_ptr->stack_ptr) {
    // Create fiber context for this task
    fiber_context_ = bctx::make_fcontext(run_ctx_ptr->stack_ptr, run_ctx_ptr->stack_size, 
      [](bctx::transfer_t t) {
        // This lambda runs in the fiber context
        // Use thread-local storage instead of transfer data
        FullPtr<Task> current_task_ptr = CHI_CUR_TASK;
        Worker* worker = CHI_CUR_WORKER;
        RunContext* run_ctx = CHI_CUR_RCTX;
        
        bool task_completed = false;
        if (!current_task_ptr.IsNull() && worker && run_ctx) {
          // Execute the task using the RunContext stored in the task
          bool execution_success = worker->TaskExecutionFunction(current_task_ptr, *run_ctx);
          
          // Mark task completion status before returning from fiber
          // Don't mark periodic tasks as complete
          if (execution_success && !current_task_ptr->IsPeriodic()) {
            task_completed = true;
          }
        }
        
        // Store completion status in RunContext for access after fiber return
        run_ctx->runtime_data = task_completed ? reinterpret_cast<void*>(1) : nullptr;
        
        // Jump back to main context when done
        worker->shared_transfer_ = bctx::jump_fcontext(t.fctx, nullptr);
      });

    // Jump to fiber context to execute the task (no data needed, using thread-local)
    shared_transfer_ = bctx::jump_fcontext(fiber_context_, nullptr);
    
    // Retrieve task completion status from RunContext
    bool task_completed = (run_ctx_ptr->runtime_data != nullptr);
    
    // Clear thread-local storage and task reference
    run_ctx_ptr->current_task = FullPtr<Task>(); // Clear task from RunContext
    CHI_CLEAR_CUR_RCTX();
    CHI_CLEAR_CUR_WORKER();
    
    // Clear RunContext pointer in task
    task_ptr->run_ctx_ = nullptr;
    
    // Only deallocate stack if task is marked complete
    if (task_completed) {
      DeallocateStackAndContext(run_ctx_ptr);
    }
    // If task is incomplete (failed or periodic), keep stack allocated for reuse
  } else {
    // Fallback to direct execution if no stack available
    bool execution_success = TaskExecutionFunction(task_ptr, *run_ctx_ptr);
    
    // Clear thread-local storage and task reference
    run_ctx_ptr->current_task = FullPtr<Task>(); // Clear task from RunContext
    CHI_CLEAR_CUR_RCTX();
    CHI_CLEAR_CUR_WORKER();
    
    // Clear RunContext pointer in task
    task_ptr->run_ctx_ = nullptr;
    
    // Mark task completion for direct execution and deallocate if complete
    if (execution_success && !task_ptr->IsPeriodic()) {
      DeallocateStackAndContext(run_ctx_ptr);
    }
    // If task is incomplete (failed or periodic), keep stack allocated for reuse
  }
}

bool Worker::TaskExecutionFunction(const FullPtr<Task>& task_ptr, const RunContext& run_ctx) {
  if (task_ptr.IsNull()) {
    return false;
  }

  // Task execution function - calls container's Run function
  // This is where the worker will eventually poll the lane and call the container's Run function
  
  try {
    // Get the container for this task
    ChiContainer* container = CHI_POOL_MANAGER->GetContainer(task_ptr->pool_id_);
    
    if (container) {
      // Call the container's Run function with the task
      RunContext mutable_run_ctx = run_ctx; // Make a mutable copy
      container->Run(task_ptr->method_, task_ptr, mutable_run_ctx);
      
      // Task execution completed successfully
      return true;
    } else {
      // Container not found - this is an error condition
      HILOG(kWarning, "Container not found for pool_id: {}", task_ptr->pool_id_);
      return false;
    }
  } catch (const std::exception& e) {
    // Handle execution errors
    HELOG(kError, "Task execution failed: {}", e.what());
    return false;
  }
}

}  // namespace chi