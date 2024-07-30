//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_

#include "worker.h"
#include "corwlock_defn.h"

namespace chi {

bool CoRwLock::TryReadLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (rep_ == 0 || is_read_) {
    is_read_ = true;
    ++rep_;
    ++read_count_;
    return true;
  }
  return false;
}

void CoRwLock::ReadLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (rep_ == 0 || is_read_) {
    is_read_ = true;
    ++rep_;
    ++read_count_;
    return;
  }
  Task *task = CHI_WORK_ORCHESTRATOR->GetCurrentTask();
  TaskId task_root = task->task_node_.root_;
  task->SetBlocked(1);
  if (blocked_map_.find(task_root) == blocked_map_.end()) {
    blocked_map_[task_root] = COMUTEX_QUEUE_T();
  }
  COMUTEX_QUEUE_T &blocked = blocked_map_[task_root];
  blocked.emplace_back((CoRwLockEntry){task});
  scoped.Unlock();
  task->Yield();
}

void CoRwLock::ReadUnlock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (--rep_ == 0) {
    is_read_ = false;
    read_count_ = 0;
    root_.SetNull();
  }
  if (blocked_map_.empty()) {
    return;
  }
  COMUTEX_QUEUE_T &blocked = blocked_map_.begin()->second;
  for (size_t i = 0; i < blocked.size(); ++i) {
    Worker::SignalUnblock(blocked[i].task_);
    ++rep_;
  }
  blocked_map_.erase(blocked_map_.begin());
}

void CoRwLock::WriteLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  Task *task = CHI_WORK_ORCHESTRATOR->GetCurrentTask();
  TaskId task_root = task->task_node_.root_;
  if (root_.IsNull() || root_ == task_root) {
    root_ = task_root;
    ++rep_;
    return;
  }
  task->SetBlocked(1);
  if (blocked_map_.find(task_root) == blocked_map_.end()) {
    blocked_map_[task_root] = COMUTEX_QUEUE_T();
  }
  COMUTEX_QUEUE_T &blocked = blocked_map_[task_root];
  blocked.emplace_back((CoRwLockEntry){task});
  scoped.Unlock();
  task->Yield();
}

void CoRwLock::WriteUnlock() {
  HILOG(kInfo, "Releasing (attempt) mutex {} rep: {}", root_, rep_ - 1);
  hshm::ScopedMutex scoped(mux_, 0);
  HILOG(kInfo, "Releasing mutex {} rep: {}", root_, rep_ - 1);
  if (--rep_ == 0) {
    root_.SetNull();
  }
  if (blocked_map_.empty()) {
    return;
  }
  COMUTEX_QUEUE_T &blocked = blocked_map_.begin()->second;
  for (size_t i = 0; i < blocked.size(); ++i) {
    Worker::SignalUnblock(blocked[i].task_);
    ++rep_;
  }
  blocked_map_.erase(blocked_map_.begin());
}

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_
