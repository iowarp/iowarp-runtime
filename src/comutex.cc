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
  
  // Set task as blocked and add it back to its lane for later processing
  if (task->run_ctx_) {
    task->run_ctx_->is_blocked = true;
  }
  AddTaskToLane(task);
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
  if (waiting_tasks_.empty()) {
    return;
  }

  // Get the first waiting TaskNode group
  auto it = waiting_tasks_.begin();
  TaskNode next_holder = it->first;
  auto& waiting_list = it->second;

  if (waiting_list.empty()) {
    waiting_tasks_.erase(it);
    UnblockNextGroup(); // Try the next group
    return;
  }

  // Set the new holder
  is_locked_ = true;
  current_holder_ = next_holder;

  // Unblock all tasks from this TaskNode group
  for (auto& waiting_task : waiting_list) {
    if (!waiting_task.IsNull() && waiting_task->run_ctx_) {
      waiting_task->run_ctx_->is_blocked = false;
      AddTaskToLane(waiting_task);
    }
  }

  // Remove this group from waiting list
  waiting_tasks_.erase(it);
}

void CoMutex::AddTaskToLane(FullPtr<Task> task) {
  if (task.IsNull() || !task->run_ctx_) {
    return;
  }

  // Get the lane from the task's run context
  auto* lane = static_cast<TaskQueue::TaskLane*>(task->run_ctx_->lane);
  if (!lane) {
    return;
  }

  // Create a FullPtr to the lane for the EmplaceTask call
  hipc::FullPtr<TaskQueue::TaskLane> lane_ptr(lane);

  // Convert task to TypedPointer for queue insertion
  hipc::TypedPointer<Task> task_typed_ptr(task.shm_);

  // Emplace the task back into its lane
  if (TaskQueue::EmplaceTask(lane_ptr, task_typed_ptr)) {
    // Notify the work orchestrator that this lane has work available
    WorkOrchestrator::NotifyWorkerLaneReady(lane_ptr);
  }
}

}  // namespace chi