//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_

#include "corwlock_defn.h"
#include "worker.h"

namespace chi {

bool CoRwLock::TryReadLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (rep_ == 0 || is_read_) {
    is_read_ = true;
    ++rep_;
    return true;
  }
  return false;
}

void CoRwLock::ReadLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (rep_ == 0 || is_read_) {
    is_read_ = true;
    ++rep_;
    return;
  }
  Task *task = CHI_CUR_TASK;
  TaskId task_root = task->task_node_.root_;
  task->SetBlocked(1);
  reader_set_.emplace_back((CoRwLockEntry){task});
  scoped.Unlock();
  task->Yield();
}

void CoRwLock::ReadUnlock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (--rep_ == 0) {
    is_read_ = false;
    root_.SetNull();
  } else {
    return;
  }
  if (writer_map_.empty()) {
    return;
  }
  auto it = writer_map_.begin();
  COMUTEX_QUEUE_T &blocked = it->second;
  root_ = it->first;
  for (size_t i = 0; i < blocked.size(); ++i) {
    CHI_WORK_ORCHESTRATOR->SignalUnblock(blocked[i].task_);
    ++rep_;
  }
  writer_map_.erase(it);
}

void CoRwLock::WriteLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  Task *task = CHI_CUR_TASK;
  TaskId task_root = task->task_node_.root_;
  if (!is_read_) {
    if (rep_ == 0 || root_ == task_root) {
      root_ = task_root;
      ++rep_;
      return;
    }
  }
  task->SetBlocked(1);
  if (writer_map_.find(task_root) == writer_map_.end()) {
    writer_map_[task_root] = COMUTEX_QUEUE_T();
  }
  COMUTEX_QUEUE_T &blocked = writer_map_[task_root];
  blocked.emplace_back((CoRwLockEntry){task});
  scoped.Unlock();
  task->Yield();
}

void CoRwLock::WriteUnlock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (--rep_ == 0) {
    root_.SetNull();
  } else {
    return;
  }
  if (!writer_map_.empty()) {
    auto it = writer_map_.begin();
    COMUTEX_QUEUE_T &blocked = it->second;
    root_ = it->first;
    for (size_t i = 0; i < blocked.size(); ++i) {
      CHI_WORK_ORCHESTRATOR->SignalUnblock(blocked[i].task_);
      ++rep_;
    }
    writer_map_.erase(it);
  } else if (!reader_set_.empty()) {
    is_read_ = true;
    for (size_t i = 0; i < reader_set_.size(); ++i) {
      CHI_WORK_ORCHESTRATOR->SignalUnblock(reader_set_[i].task_);
      ++rep_;
    }
    reader_set_.clear();
  }
}

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_H_
