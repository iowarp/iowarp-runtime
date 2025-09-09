#ifndef CHIMAERA_INCLUDE_CHIMAERA_COMUTEX_H_
#define CHIMAERA_INCLUDE_CHIMAERA_COMUTEX_H_

#include <unordered_map>
#include <list>
#include <mutex>
#include <hermes_shm/hermes_shm.h>

#include "chimaera/types.h"

namespace chi {

// Forward declarations
class Task;
template<typename T> using FullPtr = hipc::FullPtr<T>;

/**
 * Hash functor for TaskNode that ignores minor number for grouping tasks by TaskNode
 */
struct TaskNodeGroupHash {
  std::size_t operator()(const TaskNode& node) const {
    // Hash everything except minor number to group tasks by TaskNode
    return std::hash<u32>()(node.pid_) ^ 
           (std::hash<u32>()(node.tid_) << 1) ^ 
           (std::hash<u32>()(node.major_) << 2);
  }
};

/**
 * Equality functor for TaskNode that ignores minor number for grouping tasks by TaskNode
 */
struct TaskNodeGroupEqual {
  bool operator()(const TaskNode& lhs, const TaskNode& rhs) const {
    return lhs.pid_ == rhs.pid_ && 
           lhs.tid_ == rhs.tid_ && 
           lhs.major_ == rhs.major_;
    // minor_ is intentionally ignored for grouping
  }
};

/**
 * Coroutine mutex that allows multiple tasks from the same TaskNode to proceed
 * while blocking tasks from different TaskNodes. This prevents deadlocks by 
 * allowing related tasks to execute together.
 */
class CoMutex {
public:
  CoMutex() : current_holder_(), is_locked_(false) {}

  /**
   * Acquire the mutex for the given task
   * If the mutex is free, the task's TaskNode group acquires it
   * If the mutex is held by the same TaskNode group, the task proceeds
   * If the mutex is held by a different TaskNode group, the task is blocked
   * @param task Task attempting to acquire the mutex
   */
  void Lock(FullPtr<Task> task);

  /**
   * Release the mutex and unblock the next waiting TaskNode group
   * @param task Task releasing the mutex (must be from current holder group)
   */
  void Unlock(FullPtr<Task> task);

  /**
   * Try to acquire the mutex without blocking
   * @param task Task attempting to acquire the mutex
   * @return true if acquired successfully, false otherwise
   */
  bool TryLock(FullPtr<Task> task);

private:
  std::mutex internal_mutex_;  // Guards the internal state
  TaskNode current_holder_;    // TaskNode group currently holding the lock
  bool is_locked_;             // Whether the mutex is currently locked
  
  // Map from TaskNode group to list of waiting tasks
  std::unordered_map<TaskNode, std::list<FullPtr<Task>>, TaskNodeGroupHash, TaskNodeGroupEqual> waiting_tasks_;

  /**
   * Check if a task belongs to the current holder group
   * @param task_node TaskNode of the task to check
   * @return true if task belongs to current holder group
   */
  bool BelongsToCurrentHolder(const TaskNode& task_node) const;

  /**
   * Unblock the next waiting TaskNode group
   */
  void UnblockNextGroup();

  /**
   * Add a task to its corresponding lane for execution
   * @param task Task to add back to its lane
   */
  void AddTaskToLane(FullPtr<Task> task);
};

/**
 * RAII-style scoped mutex lock for CoMutex
 */
class ScopedCoMutex {
public:
  /**
   * Constructor that acquires the mutex
   * @param mutex CoMutex to acquire
   * @param task Task that will hold the mutex
   */
  explicit ScopedCoMutex(CoMutex& mutex, FullPtr<Task> task) 
      : mutex_(mutex), task_(task) {
    mutex_.Lock(task_);
  }

  /**
   * Destructor that releases the mutex
   */
  ~ScopedCoMutex() {
    mutex_.Unlock(task_);
  }

  // Non-copyable
  ScopedCoMutex(const ScopedCoMutex&) = delete;
  ScopedCoMutex& operator=(const ScopedCoMutex&) = delete;

  // Non-movable
  ScopedCoMutex(ScopedCoMutex&&) = delete;
  ScopedCoMutex& operator=(ScopedCoMutex&&) = delete;

private:
  CoMutex& mutex_;
  FullPtr<Task> task_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_COMUTEX_H_