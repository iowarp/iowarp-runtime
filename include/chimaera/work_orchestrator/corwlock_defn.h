//
// Created by llogan on 4/8/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_DEFN_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_DEFN_H_

#include "chimaera/module_registry/task.h"

namespace chi {

struct CoRwLockEntry {
  Task *task_;
};

/** A mutex for yielding coroutines */
class CoRwLock {
 public:
  typedef std::vector<CoRwLockEntry> COMUTEX_QUEUE_T;

 public:
  TaskId root_;
  size_t rep_;
  chi::ext_ring_buffer<TaskId> order_;
  std::unordered_map<TaskId, COMUTEX_QUEUE_T> writer_map_;
  COMUTEX_QUEUE_T reader_set_;
  bool is_read_;
  hshm::Mutex mux_;

 public:
  /** Default constructor */
  CoRwLock() : order_(CHI_LANE_SIZE) {
    root_.SetNull();
    rep_ = 0;
    mux_.Init();
    is_read_ = false;
  }

  bool TryReadLock();

  void ReadLock();

  void ReadUnlock();

  void WriteLock();

  void WriteUnlock();
};

class ScopedCoRwReadLock {
 public:
  CoRwLock &mutex_;

 public:
  ScopedCoRwReadLock(CoRwLock &mutex) : mutex_(mutex) { mutex_.ReadLock(); }

  ~ScopedCoRwReadLock() { mutex_.ReadUnlock(); }
};

class ScopedCoRwTryReadLock {
 public:
  CoRwLock &mutex_;
  bool locked_;

 public:
  ScopedCoRwTryReadLock(CoRwLock &mutex) : mutex_(mutex) {
    locked_ = mutex_.TryReadLock();
  }

  ~ScopedCoRwTryReadLock() {
    if (locked_) {
      mutex_.ReadUnlock();
    }
  }
};

class ScopedCoRwWriteLock {
 public:
  CoRwLock &mutex_;

 public:
  ScopedCoRwWriteLock(CoRwLock &mutex) : mutex_(mutex) { mutex_.WriteLock(); }

  ~ScopedCoRwWriteLock() { mutex_.WriteUnlock(); }
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORK_ORCHESTRATOR_CORWLOCK_DEFN_H_
