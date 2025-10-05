#include "chimaera/corwlock.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"
#include "chimaera/work_orchestrator.h"
#include "chimaera/worker.h"

namespace chi {

void CoRwLock::ReadLock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  switch (state_) {
  case CoRwLockState::kUnlocked:
    // Lock is free - acquire read lock for this TaskNode group
    state_ = CoRwLockState::kReadLocked;
    read_holders_.insert(task_node);
    return;

  case CoRwLockState::kReadLocked:
    // Already read-locked - this TaskNode group can also read
    read_holders_.insert(task_node);
    return;

  case CoRwLockState::kWriteLocked:
    // Write-locked - check if it's by the same TaskNode group
    if (BelongsToWriteHolder(task_node)) {
      // Same group that has write lock can also read (upgrade scenario)
      return;
    }
    // Different group - must block
    waiting_readers_[task_node].push_back(task);

    // Release the lock while blocking to allow other operations
    internal_mutex_.unlock();

    // Mark task as blocked and yield control back to the scheduler
    task->YieldBase();

    // When we return, the unblock method has already delegated the lock to us
    // No need to check - we can assume the lock is now ours
    return;
  }
}

void CoRwLock::ReadUnlock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  // Only remove from read holders if we're in read-locked state
  if (state_ == CoRwLockState::kReadLocked) {
    read_holders_.erase(task_node);

    // If no more readers, transition to unlocked and unblock waiting writers
    if (read_holders_.empty()) {
      state_ = CoRwLockState::kUnlocked;
      UnblockOneWaitingWriter();
    }
  }
  // If write-locked, read unlock doesn't change state (upgrade scenario)
}

void CoRwLock::WriteLock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  switch (state_) {
  case CoRwLockState::kUnlocked:
    // Lock is free - acquire write lock for this TaskNode group
    state_ = CoRwLockState::kWriteLocked;
    write_holder_ = task_node;
    return;

  case CoRwLockState::kReadLocked:
    // Read-locked - check if this TaskNode group is among the readers
    if (BelongsToReadHolders(task_node)) {
      // This group has read lock, can upgrade to write lock
      state_ = CoRwLockState::kWriteLocked;
      write_holder_ = task_node;
      // Keep the read_holders_ as they can still read
      return;
    }
    // Different group - must block
    waiting_writers_[task_node].push_back(task);

    // Release the lock while blocking to allow other operations
    internal_mutex_.unlock();

    // Mark task as blocked and yield control back to the scheduler
    task->YieldBase();

    // When we return, the unblock method has already delegated the lock to us
    // No need to check - we can assume the lock is now ours
    return;

  case CoRwLockState::kWriteLocked:
    // Write-locked - check if it's by the same TaskNode group
    if (BelongsToWriteHolder(task_node)) {
      // Same group that has write lock can proceed (reentrant)
      return;
    }
    // Different group - must block
    waiting_writers_[task_node].push_back(task);

    // Release the lock while blocking to allow other operations
    internal_mutex_.unlock();

    // Mark task as blocked and yield control back to the scheduler
    task->YieldBase();

    // When we return, the unblock method has already delegated the lock to us
    // No need to check - we can assume the lock is now ours
    return;
  }
}

void CoRwLock::WriteUnlock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  // Only the current write holder group can unlock
  if (state_ != CoRwLockState::kWriteLocked ||
      !BelongsToWriteHolder(task_node)) {
    return;
  }

  // Check if this group also has read locks (downgrade scenario)
  if (BelongsToReadHolders(task_node)) {
    // Downgrade to read-only for this group
    state_ = CoRwLockState::kReadLocked;
    // Unblock waiting readers (they can read concurrently)
    UnblockWaitingReaders();
  } else {
    // No read locks held, transition to unlocked
    state_ = CoRwLockState::kUnlocked;
    read_holders_.clear();
    // Prefer readers over writers when unblocking
    if (!waiting_readers_.empty()) {
      UnblockWaitingReaders();
    } else {
      UnblockOneWaitingWriter();
    }
  }
}

bool CoRwLock::TryReadLock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return false; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  switch (state_) {
  case CoRwLockState::kUnlocked:
    // Lock is free - acquire read lock for this TaskNode group
    state_ = CoRwLockState::kReadLocked;
    read_holders_.insert(task_node);
    return true;

  case CoRwLockState::kReadLocked:
    // Already read-locked - this TaskNode group can also read
    read_holders_.insert(task_node);
    return true;

  case CoRwLockState::kWriteLocked:
    // Write-locked - check if it's by the same TaskNode group
    if (BelongsToWriteHolder(task_node)) {
      // Same group that has write lock can also read (upgrade scenario)
      return true;
    }
    // Different group - cannot acquire without blocking
    return false;
  }

  return false;
}

bool CoRwLock::TryWriteLock() {
  // Get current task from the current worker
  auto *worker = CHI_CUR_WORKER;
  if (!worker) {
    return false; // No worker context
  }

  FullPtr<Task> task = worker->GetCurrentTask();
  if (task.IsNull()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(internal_mutex_);

  const TaskNode &task_node = task->task_node_;

  switch (state_) {
  case CoRwLockState::kUnlocked:
    // Lock is free - acquire write lock for this TaskNode group
    state_ = CoRwLockState::kWriteLocked;
    write_holder_ = task_node;
    return true;

  case CoRwLockState::kReadLocked:
    // Read-locked - check if this TaskNode group is among the readers
    if (BelongsToReadHolders(task_node)) {
      // This group has read lock, can upgrade to write lock
      state_ = CoRwLockState::kWriteLocked;
      write_holder_ = task_node;
      return true;
    }
    // Different group - cannot acquire without blocking
    return false;

  case CoRwLockState::kWriteLocked:
    // Write-locked - check if it's by the same TaskNode group
    if (BelongsToWriteHolder(task_node)) {
      // Same group that has write lock can proceed (reentrant)
      return true;
    }
    // Different group - cannot acquire without blocking
    return false;
  }

  return false;
}

bool CoRwLock::BelongsToWriteHolder(const TaskNode &task_node) const {
  // Use the same comparison logic as TaskNodeGroupEqual
  return task_node.pid_ == write_holder_.pid_ &&
         task_node.tid_ == write_holder_.tid_ &&
         task_node.major_ == write_holder_.major_;
  // minor_ is intentionally ignored for grouping
}

bool CoRwLock::BelongsToReadHolders(const TaskNode &task_node) const {
  // Check if this TaskNode group is in the read_holders_ set
  return read_holders_.find(task_node) != read_holders_.end();
}

void CoRwLock::UnblockNextGroup() {
  // Prefer readers over writers for better concurrency
  if (!waiting_readers_.empty()) {
    UnblockWaitingReaders();
  } else {
    UnblockOneWaitingWriter();
  }
}

void CoRwLock::UnblockWaitingReaders() {
  // Delegate read access to all waiting reader groups
  if (!waiting_readers_.empty()) {
    // Transition to read-locked state and add all waiting reader groups to
    // holders
    state_ = CoRwLockState::kReadLocked;
    for (auto &[task_node, task_list] : waiting_readers_) {
      read_holders_.insert(task_node);
    }

    // Notify the work orchestrator that all waiting reader tasks can now
    // proceed
    auto *worker = CHI_CUR_WORKER;
    if (worker) {
      for (auto &[task_node, task_list] : waiting_readers_) {
        for (auto &waiting_task : task_list) {
          if (!waiting_task.IsNull() && waiting_task->run_ctx_) {
            worker->AddToBlockedQueue(waiting_task->run_ctx_, 0.0);
          }
        }
      }
    }

    // CRITICAL: Clear waiting_readers_ to prevent re-enqueueing
    waiting_readers_.clear();
  }
}

void CoRwLock::UnblockOneWaitingWriter() {
  // Delegate write access to the first waiting writer group
  if (!waiting_writers_.empty() && state_ == CoRwLockState::kUnlocked) {
    auto it = waiting_writers_.begin();
    const TaskNode &next_holder = it->first;

    // Transfer write lock to the next group
    state_ = CoRwLockState::kWriteLocked;
    write_holder_ = next_holder;

    // Notify the work orchestrator that tasks from this writer group can now
    // proceed
    auto *worker = CHI_CUR_WORKER;
    if (worker) {
      for (auto &waiting_task : it->second) {
        if (!waiting_task.IsNull() && waiting_task->run_ctx_) {
          worker->AddToBlockedQueue(waiting_task->run_ctx_, 0.0);
        }
      }
    }

    // CRITICAL: Remove this TaskNode group from waiting_writers_ to prevent re-enqueueing
    waiting_writers_.erase(it);
  }
}

} // namespace chi