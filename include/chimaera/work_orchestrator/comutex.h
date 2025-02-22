//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_

#include "comutex_defn.h"
#include "worker.h"

namespace chi {

bool CoMutex::TryLock() {
  hshm::ScopedMutex scoped(mux_, 0);
  Task *task = CHI_CUR_TASK;
  TaskId task_root = task->task_node_.root_;
  if (root_.IsNull() || root_ == task_root) {
    root_ = task_root;
    ++rep_;
    return true;
  }
  return false;
}

void CoMutex::Lock() {
  hshm::ScopedMutex scoped(mux_, 0);
  Task *task = CHI_CUR_TASK;
  TaskId task_root = task->task_node_.root_;
  if (rep_ == 0 || root_ == task_root) {
    root_ = task_root;
    ++rep_;
    return;
  }
  task->SetBlocked(1);
  if (blocked_map_.find(task_root) == blocked_map_.end()) {
    blocked_map_[task_root] = COMUTEX_QUEUE_T();
    order_.push(task_root);
  }
  // HILOG(kInfo, "Locking task {} (id={}, pool={}, method={})",
  //       (void*)task, task->task_node_, task->pool_, task->method_);
  COMUTEX_QUEUE_T &blocked = blocked_map_[task_root];
  blocked.emplace_back((CoMutexEntry){task});
  scoped.Unlock();
  task->Yield();
}

void CoMutex::Unlock() {
  hshm::ScopedMutex scoped(mux_, 0);
  if (--rep_ == 0) {
    root_.SetNull();
  } else {
    return;
  }
  if (blocked_map_.empty()) {
    return;
  }
  order_.pop(root_);
  COMUTEX_QUEUE_T &blocked = blocked_map_[root_];
  for (size_t i = 0; i < blocked.size(); ++i) {
    Task *task = blocked[i].task_;
    // HILOG(kInfo, "Unlocking task {} (id={}, pool={}, method={})", (void
    // *)task,
    //       task->task_node_, task->pool_, task->method_);
    CHI_WORK_ORCHESTRATOR->SignalUnblock(task, task->rctx_);
    ++rep_;
  }
  blocked_map_.erase(root_);
}

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
