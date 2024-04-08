//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_

#include "worker.h"
#include "chimaera/task_registry/task.h"

namespace chm {

struct CoMutexEntry {
  Task *task_;
  RunContext *rctx_;
};

/** A mutex for yielding coroutines */
class CoMutex {
 public:
  typedef std::vector<CoMutexEntry> COMUTEX_QUEUE_T;

 public:
  TaskId root_;
  size_t rep_;
  std::unordered_map<TaskId, COMUTEX_QUEUE_T> blocked_map_;

 public:
  CoMutex() {
    root_.SetNull();
    rep_ = 0;
  }

  void Lock(Task *task, RunContext &rctx) {
    TaskId task_root = task->task_node_.root_;
    if (root_ != task_root) {
      task->SetBlocked();
      if (blocked_map_.find(task_root) == blocked_map_.end()) {
        blocked_map_[task_root] = COMUTEX_QUEUE_T();
      }
      COMUTEX_QUEUE_T &blocked = blocked_map_[task_root];
      blocked.emplace_back((CoMutexEntry){task, &rctx});
      task->Yield<TASK_YIELD_CO>();
    } else {
      root_ = task_root;
      ++rep_;
    }
  }

  void Unlock() {
    if (--rep_ == 0) {
      root_.SetNull();
    }
    if (blocked_map_.size() == 0) {
      return;
    }
    COMUTEX_QUEUE_T &blocked = blocked_map_.begin()->second;
    for (size_t i = 0; i < blocked.size(); ++i) {
      RunContext &rctx = *blocked[i].rctx_;
      Worker &worker = HRUN_WORK_ORCHESTRATOR->GetWorker(
          rctx.worker_id_);
      worker.SignalUnblock(blocked[i].task_);
    }
    blocked_map_.erase(blocked_map_.begin());
  }
};

class ScopedCoMutex {
 public:
  CoMutex &mutex_;

 public:
  ScopedCoMutex(CoMutex &mutex,
                Task *task,
                RunContext &rctx)
      : mutex_(mutex) {
    mutex_.Lock(task, rctx);
  }

  ~ScopedCoMutex() {
    mutex_.Unlock();
  }
};

}  // namespace chm

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_H_
