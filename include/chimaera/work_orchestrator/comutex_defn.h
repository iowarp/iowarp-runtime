//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_DEFN_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_DEFN_H_

#include <map>

#include "chimaera/module_registry/task.h"

namespace chi {

struct CoMutexEntry {
  Task *task_;
};

/** A mutex for yielding coroutines */
class CoMutex {
 public:
  typedef std::vector<CoMutexEntry> COMUTEX_QUEUE_T;

 public:
  TaskId root_;
  size_t rep_;
  chi::spsc_queue<TaskId> order_;
  std::unordered_map<TaskId, COMUTEX_QUEUE_T> blocked_map_;
  hshm::Mutex mux_;

 public:
  /** Default constructor */
  CoMutex() : order_(CHI_LANE_SIZE) {
    root_.SetNull();
    rep_ = 0;
    mux_.Init();
  }

  bool TryLock();

  void Lock();

  void Unlock();
};

class ScopedCoMutex {
 public:
  CoMutex &mutex_;

 public:
  ScopedCoMutex(CoMutex &mutex) : mutex_(mutex) { mutex_.Lock(); }

  ~ScopedCoMutex() { mutex_.Unlock(); }
};

class ScopedTryCoMutex {
 public:
  CoMutex &mutex_;
  bool is_locked_;

 public:
  ScopedTryCoMutex(CoMutex &mutex) : mutex_(mutex) {
    is_locked_ = mutex_.TryLock();
  }

  ~ScopedTryCoMutex() {
    if (is_locked_) {
      mutex_.Unlock();
    }
  }
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_DEFN_H_
