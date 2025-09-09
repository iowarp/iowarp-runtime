#ifndef CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_

#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>
#include <hermes_shm/hermes_shm.h>

#include "chimaera/types.h"
#include "chimaera/comutex.h"  // For TaskNodeGroupHash and TaskNodeGroupEqual

namespace chi {

/**
 * Lock state for the reader-writer lock
 */
enum class CoRwLockState {
  kUnlocked,     // No tasks hold the lock
  kReadLocked,   // One or more TaskNode groups hold read locks
  kWriteLocked   // One TaskNode group holds the write lock
};

/**
 * Coroutine reader-writer lock that allows multiple readers from any TaskNode group
 * or a single writer TaskNode group. Like CoMutex, tasks from the same TaskNode group
 * can proceed together to prevent deadlocks.
 */
class CoRwLock {
public:
  CoRwLock() : state_(CoRwLockState::kUnlocked), write_holder_() {}

  /**
   * Acquire a read lock for the current task (retrieved from CHI_CUR_WORKER)
   * If unlocked or read-locked: task's TaskNode group can proceed
   * If write-locked by different TaskNode group: task is blocked
   * If write-locked by same TaskNode group: task can proceed (upgrade scenario)
   */
  void ReadLock();

  /**
   * Release a read lock (uses current task from CHI_CUR_WORKER)
   */
  void ReadUnlock();

  /**
   * Acquire a write lock for the current task (retrieved from CHI_CUR_WORKER)
   * If unlocked: task's TaskNode group acquires write lock
   * If locked by same TaskNode group: task can proceed
   * If locked by different TaskNode group: task is blocked
   */
  void WriteLock();

  /**
   * Release a write lock and unblock the next waiting TaskNode group
   * (uses current task from CHI_CUR_WORKER)
   */
  void WriteUnlock();

  /**
   * Try to acquire a read lock without blocking
   * (uses current task from CHI_CUR_WORKER)
   * @return true if acquired successfully, false otherwise
   */
  bool TryReadLock();

  /**
   * Try to acquire a write lock without blocking
   * (uses current task from CHI_CUR_WORKER)
   * @return true if acquired successfully, false otherwise
   */
  bool TryWriteLock();

private:
  std::mutex internal_mutex_;  // Guards the internal state
  CoRwLockState state_;        // Current lock state
  
  // For read locks: set of TaskNode groups holding read locks
  std::unordered_set<TaskNode, TaskNodeGroupHash, TaskNodeGroupEqual> read_holders_;
  
  // For write locks: TaskNode group holding the write lock
  TaskNode write_holder_;
  
  // Map from TaskNode group to list of waiting read tasks
  std::unordered_map<TaskNode, std::list<FullPtr<Task>>, TaskNodeGroupHash, TaskNodeGroupEqual> waiting_readers_;
  
  // Map from TaskNode group to list of waiting write tasks
  std::unordered_map<TaskNode, std::list<FullPtr<Task>>, TaskNodeGroupHash, TaskNodeGroupEqual> waiting_writers_;

  /**
   * Check if a task belongs to the current write holder group
   * @param task_node TaskNode of the task to check
   * @return true if task belongs to current write holder group
   */
  bool BelongsToWriteHolder(const TaskNode& task_node) const;

  /**
   * Check if a task belongs to any current read holder group
   * @param task_node TaskNode of the task to check
   * @return true if task belongs to any current read holder group
   */
  bool BelongsToReadHolders(const TaskNode& task_node) const;

  /**
   * Unblock the next waiting TaskNode group (prefer readers over writers when possible)
   */
  void UnblockNextGroup();

  /**
   * Add a task to its corresponding lane for execution
   * @param task Task to add back to its lane
   */
  void AddTaskToLane(FullPtr<Task> task);

  /**
   * Unblock all waiting readers (used when write lock is released)
   */
  void UnblockWaitingReaders();

  /**
   * Unblock one waiting writer (used when all read locks are released)
   */
  void UnblockOneWaitingWriter();
};

/**
 * RAII-style scoped read lock for CoRwLock
 */
class ScopedCoRwReadLock {
public:
  /**
   * Constructor that acquires the read lock
   * @param rwlock CoRwLock to acquire read lock on
   */
  explicit ScopedCoRwReadLock(CoRwLock& rwlock)
      : rwlock_(rwlock) {
    rwlock_.ReadLock();
  }

  /**
   * Destructor that releases the read lock
   */
  ~ScopedCoRwReadLock() {
    rwlock_.ReadUnlock();
  }

  // Non-copyable
  ScopedCoRwReadLock(const ScopedCoRwReadLock&) = delete;
  ScopedCoRwReadLock& operator=(const ScopedCoRwReadLock&) = delete;

  // Non-movable
  ScopedCoRwReadLock(ScopedCoRwReadLock&&) = delete;
  ScopedCoRwReadLock& operator=(ScopedCoRwReadLock&&) = delete;

private:
  CoRwLock& rwlock_;
};

/**
 * RAII-style scoped write lock for CoRwLock
 */
class ScopedCoRwWriteLock {
public:
  /**
   * Constructor that acquires the write lock
   * @param rwlock CoRwLock to acquire write lock on
   */
  explicit ScopedCoRwWriteLock(CoRwLock& rwlock)
      : rwlock_(rwlock) {
    rwlock_.WriteLock();
  }

  /**
   * Destructor that releases the write lock
   */
  ~ScopedCoRwWriteLock() {
    rwlock_.WriteUnlock();
  }

  // Non-copyable
  ScopedCoRwWriteLock(const ScopedCoRwWriteLock&) = delete;
  ScopedCoRwWriteLock& operator=(const ScopedCoRwWriteLock&) = delete;

  // Non-movable
  ScopedCoRwWriteLock(ScopedCoRwWriteLock&&) = delete;
  ScopedCoRwWriteLock& operator=(ScopedCoRwWriteLock&&) = delete;

private:
  CoRwLock& rwlock_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_