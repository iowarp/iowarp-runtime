#include "chimaera/work_orchestrator/corwlock.h"
#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/work_orchestrator/worker.h"

namespace chi {

CoRwLock::CoRwLock()
    : order_(CHI_RUNTIME->server_config_->queue_manager_.comux_depth_) {
  root_.SetNull();
  rep_ = 0;
  mux_.Init();
  is_read_ = false;
}

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
  if (!is_read_ && root_ == task_root) {
    HELOG(kFatal, "Recursively acquiring read lock during write!");
  }
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
  order_.pop(root_);
  COMUTEX_QUEUE_T &blocked = writer_map_[root_];
  for (size_t i = 0; i < blocked.size(); ++i) {
    Task *task = blocked[i].task_;
    CHI_WORK_ORCHESTRATOR->SignalUnblock(task, task->rctx_);
    ++rep_;
  }
  writer_map_.erase(root_);
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
  } else if (is_read_ && root_ == task_root) {
    HELOG(kFatal, "Recursively acquiring write lock during read!");
  }
  task->SetBlocked(1);
  if (writer_map_.find(task_root) == writer_map_.end()) {
    writer_map_[task_root] = COMUTEX_QUEUE_T();
    order_.push(task_root);
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
    order_.pop(root_);
    COMUTEX_QUEUE_T &blocked = writer_map_[root_];
    for (size_t i = 0; i < blocked.size(); ++i) {
      Task *task = blocked[i].task_;
      CHI_WORK_ORCHESTRATOR->SignalUnblock(task, task->rctx_);
      ++rep_;
    }
    writer_map_.erase(root_);
  } else if (!reader_set_.empty()) {
    is_read_ = true;
    for (size_t i = 0; i < reader_set_.size(); ++i) {
      Task *task = reader_set_[i].task_;
      CHI_WORK_ORCHESTRATOR->SignalUnblock(task, task->rctx_);
      ++rep_;
    }
    reader_set_.clear();
  }
}

} // namespace chi