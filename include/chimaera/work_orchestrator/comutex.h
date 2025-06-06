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
  chi::ext_ring_buffer<TaskId> order_;
  std::unordered_map<TaskId, COMUTEX_QUEUE_T> blocked_map_;
  hshm::Mutex mux_;

public:
  CoMutex();

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

} // namespace chi

#endif // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_COMUTEX_DEFN_H_
