/**
 * Worker implementation
 */

#include "chimaera/worker.h"
#include "chimaera/singletons.h"
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

  // Initialize stack allocator using HSHM memory manager
  auto mem_manager = HSHM_MEMORY_MANAGER;
  hipc::AllocatorId alloc_id(2, worker_id_); // Use worker-specific allocator
  
  try {
    // Create a dedicated allocator for this worker's stack
    mem_manager->CreateAllocator<hipc::StackAllocator>(
        hipc::MemoryBackendId::Get(0), alloc_id, 0
    );
    stack_allocator_ = std::make_unique<hipc::StackAllocator>(
        *mem_manager->GetAllocator<hipc::StackAllocator>(alloc_id)
    );
  } catch (const std::exception& e) {
    return false;
  }

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

  // Cleanup stack allocator
  stack_allocator_.reset();

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

  // Main worker loop
  while (is_running_) {
    Task* task = nullptr;

    // Try to pop from active queue first
    task = PopActiveTask();
    if (task == nullptr) {
      // Try cold queue if active queue is empty
      task = PopColdTask();
    }

    if (task != nullptr) {
      // Process the task
      if (ResolveDomainQuery(task)) {
        RunContext run_ctx = CreateRunContext(task);
        ExecuteTask(task, run_ctx);
      }
    } else {
      // No tasks available, yield briefly
      // In real implementation, might use condition variable or sleep
      std::this_thread::yield();
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

bool Worker::ResolveDomainQuery(Task* task) {
  if (task == nullptr) {
    return false;
  }

  // Stub implementation - would resolve domain query for task routing
  return true;
}

RunContext Worker::CreateRunContext(Task* task) {
  RunContext run_ctx;
  run_ctx.thread_type = thread_type_;
  run_ctx.worker_id = worker_id_;
  run_ctx.stack_size = 65536; // 64KB
  run_ctx.stack_ptr = AllocateStack(run_ctx.stack_size);
  return run_ctx;
}

void* Worker::AllocateStack(size_t size) {
  if (!stack_allocator_) {
    return nullptr;
  }
  
  auto stack_mem = stack_allocator_->Allocate(HSHM_MCTX, size);
  return stack_mem.ptr_;
}

void Worker::DeallocateStack(void* stack_ptr, size_t size) {
  if (!stack_allocator_ || !stack_ptr) {
    return;
  }
  
  hipc::FullPtr<void> stack_full_ptr(stack_ptr);
  stack_allocator_->Free(HSHM_MCTX, stack_full_ptr);
}

void Worker::ExecuteTask(Task* task, const RunContext& run_ctx) {
  if (task == nullptr) {
    return;
  }

  // Execute task directly (stub implementation)
  TaskExecutionFunction(task, run_ctx);

  // Cleanup stack
  if (run_ctx.stack_ptr) {
    DeallocateStack(run_ctx.stack_ptr, run_ctx.stack_size);
  }
}

void Worker::TaskExecutionFunction(Task* task, const RunContext& run_ctx) {
  // Empty task execution function for now
  // In real implementation, this would:
  // 1. Look up the task method
  // 2. Execute the corresponding function
  // 3. Handle task completion and cleanup
}

}  // namespace chi