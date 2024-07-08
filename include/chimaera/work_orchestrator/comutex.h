//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_

#include "worker.h"
#include "comutex_defn.h"

namespace chi {

void CoMutex::Lock() {
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
  blocked.emplace_back((CoMutexEntry){task});
  task->Yield();
}

void CoMutex::Unlock() {
  if (--rep_ == 0) {
    root_.SetNull();
  }
  if (blocked_map_.size() == 0) {
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

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
