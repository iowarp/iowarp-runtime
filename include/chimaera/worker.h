#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_

#include <memory>
#include <thread>
#include "chimaera/types.h"
#include "chimaera/task.h"
#include "chimaera/chimod_spec.h"

namespace chi {

// Forward declarations
class Task;

/**
 * Worker class for executing tasks
 * 
 * Manages active and cold lane queues, executes tasks using boost::fiber,
 * and provides task execution environment with stack allocation.
 */
class Worker {
 public:
  /**
   * Constructor
   * @param worker_id Unique worker identifier
   * @param thread_type Type of worker thread
   */
  Worker(u32 worker_id, ThreadType thread_type);

  /**
   * Destructor
   */
  ~Worker();

  /**
   * Initialize worker
   * @return true if initialization successful, false otherwise
   */
  bool Init();

  /**
   * Finalize and cleanup worker resources
   */
  void Finalize();

  /**
   * Main worker loop - processes tasks from queues
   */
  void Run();

  /**
   * Stop the worker loop
   */
  void Stop();

  /**
   * Get worker ID
   * @return Worker identifier
   */
  u32 GetId() const;

  /**
   * Get worker thread type
   * @return Type of worker thread
   */
  ThreadType GetThreadType() const;

  /**
   * Check if worker is running
   * @return true if worker is active, false otherwise
   */
  bool IsRunning() const;

 private:
  /**
   * Pop task from active lane queue
   * @return Pointer to task or nullptr if queue empty
   */
  Task* PopActiveTask();

  /**
   * Pop task from cold lane queue
   * @return Pointer to task or nullptr if queue empty
   */
  Task* PopColdTask();

  /**
   * Resolve domain query for task routing
   * @param task Task to resolve domain for
   * @return true if resolution successful, false otherwise
   */
  bool ResolveDomainQuery(Task* task);

  /**
   * Create run context for task execution
   * @param task Task to create context for
   * @return RunContext for task execution
   */
  RunContext CreateRunContext(Task* task);

  /**
   * Allocate stack for task execution (64KB default)
   * @param size Stack size in bytes
   * @return Pointer to allocated stack
   */
  void* AllocateStack(size_t size = 65536);  // 64KB default

  /**
   * Deallocate task execution stack
   * @param stack_ptr Pointer to stack to deallocate
   * @param size Size of stack in bytes
   */
  void DeallocateStack(void* stack_ptr, size_t size);

  /**
   * Execute task using boost::fiber
   * @param task Task to execute
   * @param run_ctx Run context for execution
   */
  void ExecuteTask(Task* task, const RunContext& run_ctx);

  /**
   * Task execution function (empty for now)
   * @param task Task to execute
   * @param run_ctx Run context
   */
  void TaskExecutionFunction(Task* task, const RunContext& run_ctx);

  u32 worker_id_;
  ThreadType thread_type_;
  bool is_running_;
  bool is_initialized_;

  // Queue references - will be set to point to IpcManager queues
  void* active_queue_;  // Stub - would be multi_mpsc_queue pointer
  void* cold_queue_;    // Stub - would be multi_mpsc_queue pointer

  // Stack allocator for task execution using real HSHM stack allocator
  std::unique_ptr<hipc::StackAllocator> stack_allocator_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_