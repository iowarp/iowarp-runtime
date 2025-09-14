#include "chimaera/comutex.h"
#include "chimaera/task.h"
#include "chimaera/work_orchestrator.h"
#include "chimaera/task_queue.h"
#include "chimaera/worker.h"

namespace chi {

void CoMutex::Lock() {
  // Get current task from the current worker
  auto* worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }
  
  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);
  
  const TaskNode& task_node = task->task_node_;

  // If mutex is not locked, acquire it for this TaskNode group
  if (!is_locked_) {
    is_locked_ = true;
    current_holder_ = task_node;
    return;
  }

  // If already held by the same TaskNode group, allow the task to proceed
  if (BelongsToCurrentHolder(task_node)) {
    return;
  }

  // Different TaskNode group - must block this task  
  // Add task to the waiting list for its TaskNode group
  waiting_tasks_[task_node].push_back(task);
  
  // Release the lock while blocking to allow other operations
  internal_mutex_.unlock();
  
  // Mark task as blocked and yield control back to the scheduler
  task->YieldBase();
  
  // When we return, the unblock method has already delegated the lock to us
  // No need to check - we can assume the lock is now ours
}

void CoMutex::Unlock() {
  // Get current task from the current worker
  auto* worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }
  
  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);
  
  const TaskNode& task_node = task->task_node_;

  // Only the current holder group can unlock
  if (!is_locked_ || !BelongsToCurrentHolder(task_node)) {
    return;
  }

  // Release the lock and unblock the next waiting group
  is_locked_ = false;
  UnblockNextGroup();
}

bool CoMutex::TryLock() {
  // Get current task from the current worker
  auto* worker = CHI_CUR_WORKER;
  if (!worker) {
    return false; // No worker context
  }
  
  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);
  
  const TaskNode& task_node = task->task_node_;

  // If mutex is not locked, acquire it for this TaskNode group
  if (!is_locked_) {
    is_locked_ = true;
    current_holder_ = task_node;
    return true;
  }

  // If already held by the same TaskNode group, allow the task to proceed
  if (BelongsToCurrentHolder(task_node)) {
    return true;
  }

  // Different TaskNode group - cannot acquire without blocking
  return false;
}

bool CoMutex::BelongsToCurrentHolder(const TaskNode& task_node) const {
  // Use the same comparison logic as TaskNodeGroupEqual
  return task_node.pid_ == current_holder_.pid_ && 
         task_node.tid_ == current_holder_.tid_ && 
         task_node.major_ == current_holder_.major_;
  // minor_ is intentionally ignored for grouping
}

void CoMutex::UnblockNextGroup() {
  // Delegate the lock to the first waiting TaskNode group
  if (!waiting_tasks_.empty()) {
    auto it = waiting_tasks_.begin();
    const TaskNode& next_holder = it->first;
    
    // Transfer lock to the next group
    is_locked_ = true;
    current_holder_ = next_holder;
    
    // Notify the work orchestrator that tasks from this group can now proceed
    auto* worker = CHI_CUR_WORKER;
    if (worker) {
      for (auto& waiting_task : it->second) {
        if (!waiting_task.IsNull() && waiting_task->run_ctx_) {
          worker->AddToBlockedQueue(waiting_task->run_ctx_, 0.0);
        }
      }
    }
  }
}


}  // namespace chi