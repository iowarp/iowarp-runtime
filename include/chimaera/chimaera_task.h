#ifndef CHI_CHIMAERA_TASK_H_
#define CHI_CHIMAERA_TASK_H_

#include <hermes_shm/data_structures/all.h>
#include <boost/context/fiber_fcontext.hpp>
#include "chimaera_types.h"

namespace chi {

#define IN
#define OUT
#define INOUT
#define TEMP

class Task;
class RunContext;

typedef hshm::ipc::FullPtr<Task> TaskPointer;

struct Task : public hipc::ShmContainer {
public:
  IN PoolId pool_id_;
  IN TaskNode task_node_;
  IN DomainQuery dom_query_;
  IN MethodId method_;
  IN IntFlag task_flags_;
  IN double period_ns_;

  Task() = default;

  void Wait();
  void Wait(Task *subtask);
  
  template <typename TaskT>
  void Wait(std::vector<hshm::ipc::FullPtr<TaskT>> &subtasks);

  template <typename Ar>
  void serialize(Ar &ar) {
    ar(pool_id_, task_node_, dom_query_, method_, task_flags_, period_ns_);
  }
};

namespace bctx = boost::context::detail;

struct FiberStack {
  void *stack_ptr_;
  size_t stack_size_;
  
  FiberStack() : stack_ptr_(nullptr), stack_size_(0) {}
  
  ~FiberStack() {
    if (stack_ptr_) {
      free(stack_ptr_);
    }
  }
  
  void Allocate(size_t size = 65536) {
    stack_size_ = size;
    stack_ptr_ = malloc(stack_size_);
  }
};

struct RunContext {
  bctx::fcontext_t fiber_ctx_;
  FiberStack stack_;
  bool is_blocked_;
  bool is_completed_;
  hshm::Timer timer_;
  Task *current_task_;
  QueueId suggested_queue_id_;
  LaneId suggested_lane_id_;
  
  RunContext() : is_blocked_(false), is_completed_(false), current_task_(nullptr), 
                 suggested_queue_id_(0), suggested_lane_id_(0) {}
  
  void InitForTask(Task *task) {
    current_task_ = task;
    is_blocked_ = false;
    is_completed_ = false;
    
    if (task->task_flags_.Any(TASK_PERIODIC)) {
      timer_.Resume();
    }
  }
  
  void SetBlocked(bool blocked) {
    is_blocked_ = blocked;
  }
  
  void SetCompleted() {
    is_completed_ = true;
    if (current_task_ && current_task_->task_flags_.Any(TASK_PERIODIC)) {
      timer_.Pause();
    }
  }
  
  bool ShouldExecutePeriodic() {
    if (!current_task_->task_flags_.Any(TASK_PERIODIC)) {
      return false;
    }
    return timer_.GetNsecFromStart() >= current_task_->period_ns_;
  }
  
  void ResetPeriodic() {
    if (current_task_->task_flags_.Any(TASK_PERIODIC)) {
      timer_.Reset();
    }
  }
};

class CoroutineLock {
private:
  chi::ipc::ext_ring_buffer<Task*> blocked_tasks_;
  std::mutex mutex_;
  bool is_locked_;

public:
  CoroutineLock() : is_locked_(false) {}

  void Lock(Task *task, RunContext &ctx) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!is_locked_) {
      is_locked_ = true;
      return;
    }
    
    blocked_tasks_.emplace(task);
    ctx.SetBlocked(true);
  }

  void Unlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    is_locked_ = false;
    
    Task *next_task;
    if (blocked_tasks_.pop(next_task)) {
    }
  }
};

class CoroutineReaderWriterLock {
private:
  chi::ipc::ext_ring_buffer<Task*> blocked_readers_;
  chi::ipc::ext_ring_buffer<Task*> blocked_writers_;
  std::mutex mutex_;
  int readers_count_;
  bool writer_active_;

public:
  CoroutineReaderWriterLock() : readers_count_(0), writer_active_(false) {}

  void ReaderLock(Task *task, RunContext &ctx) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!writer_active_) {
      readers_count_++;
      return;
    }
    
    blocked_readers_.emplace(task);
    ctx.SetBlocked(true);
  }

  void ReaderUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    readers_count_--;
    
    if (readers_count_ == 0) {
      Task *writer_task;
      if (blocked_writers_.pop(writer_task)) {
        writer_active_ = true;
      }
    }
  }

  void WriterLock(Task *task, RunContext &ctx) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!writer_active_ && readers_count_ == 0) {
      writer_active_ = true;
      return;
    }
    
    blocked_writers_.emplace(task);
    ctx.SetBlocked(true);
  }

  void WriterUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    writer_active_ = false;
    
    Task *reader_task;
    while (blocked_readers_.pop(reader_task)) {
      readers_count_++;
    }
    
    if (readers_count_ == 0) {
      Task *writer_task;
      if (blocked_writers_.pop(writer_task)) {
        writer_active_ = true;
      }
    }
  }
};

}  // namespace chi

#endif  // CHI_CHIMAERA_TASK_H_