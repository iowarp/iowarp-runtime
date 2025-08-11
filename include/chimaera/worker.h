#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_

#include <memory>
#include <thread>
#include <vector>
#include <shared_mutex>
#include "chimaera/types.h"
#include "chimaera/task.h"
#include "chimaera/chimod_spec.h"
#include "chimaera/domain_query.h"

namespace chi {

// Forward declarations
class Task;

// Macros for accessing HSHM thread-local storage (worker thread context)
// These macros allow access to the current task, run context, and worker from any thread
// Example usage in ChiMod container code:
//   FullPtr<Task> current_task = CHI_CUR_TASK;
//   RunContext* run_ctx = CHI_CUR_RCTX;
//   Worker* worker = CHI_CUR_WORKER;
#define CHI_CUR_TASK (CHI_CUR_RCTX ? CHI_CUR_RCTX->current_task : FullPtr<Task>())
#define CHI_CUR_RCTX (HSHM_THREAD_MODEL->GetTls<struct RunContext>(chi_cur_rctx_key_))
#define CHI_CUR_WORKER (HSHM_THREAD_MODEL->GetTls<class Worker>(chi_cur_worker_key_))

// Helper macros for setting thread-local storage (internal use)
#define CHI_SET_CUR_TASK(task_ptr) (CHI_CUR_RCTX ? (CHI_CUR_RCTX->current_task = (task_ptr)) : FullPtr<Task>())
#define CHI_SET_CUR_RCTX(rctx) (HSHM_THREAD_MODEL->SetTls(chi_cur_rctx_key_, static_cast<struct RunContext*>(rctx)))
#define CHI_SET_CUR_WORKER(worker) (HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<class Worker*>(worker)))

// Helper macros for clearing thread-local storage (internal use)
#define CHI_CLEAR_CUR_TASK() (CHI_CUR_RCTX ? (CHI_CUR_RCTX->current_task = FullPtr<Task>()) : FullPtr<Task>())
#define CHI_CLEAR_CUR_RCTX() (HSHM_THREAD_MODEL->SetTls(chi_cur_rctx_key_, static_cast<struct RunContext*>(nullptr)))
#define CHI_CLEAR_CUR_WORKER() (HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<class Worker*>(nullptr)))

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
   * Iterate over active lanes assigned to this worker
   * @return Vector of lane IDs that this worker should process
   */
  std::vector<LaneId> GetActiveLanes();

  /**
   * Resolve domain query for task routing
   * Routes tasks to containers on this node based on PoolId and DomainQuery
   * @param task_ptr Full pointer to task to resolve domain for
   * @return true if resolution successful, false otherwise
   */
  bool ResolveDomainQuery(const FullPtr<Task>& task_ptr);

  /**
   * Query container from PoolManager based on task requirements
   * @param task_ptr Full pointer to task requiring container lookup
   * @return Pointer to container or nullptr if not found
   */
  ChiContainer* QueryContainerFromPoolManager(const FullPtr<Task>& task_ptr);

  /**
   * Call monitor function with kLocalSchedule to map task to lane
   * @param container Target container for the task
   * @param task_ptr Full pointer to task to be mapped to a lane
   * @return true if mapping successful, false otherwise
   */
  bool CallMonitorForLocalSchedule(ChiContainer* container, const FullPtr<Task>& task_ptr);

  /**
   * Create run context for task execution
   * @param task_ptr Full pointer to task to create context for
   * @return RunContext for task execution
   */
  RunContext CreateRunContext(const FullPtr<Task>& task_ptr);

  /**
   * Allocate stack and RunContext for task execution (64KB default)
   * @param size Stack size in bytes
   * @return RunContext pointer with stack_ptr set
   */
  RunContext* AllocateStackAndContext(size_t size = 65536);  // 64KB default

  /**
   * Deallocate task execution stack and RunContext
   * @param run_ctx_ptr Pointer to RunContext containing stack info to deallocate
   */
  void DeallocateStackAndContext(RunContext* run_ctx_ptr);

  /**
   * Execute task using boost::fiber for context switching
   * @param task_ptr Full pointer to task to execute (RunContext will be allocated and set in task)
   */
  void ExecuteTask(const FullPtr<Task>& task_ptr);

  /**
   * Task execution function - calls container's Run function
   * @param task_ptr Full pointer to task to execute
   * @param run_ctx Run context
   * @return true if task completed successfully, false otherwise
   */
  bool TaskExecutionFunction(const FullPtr<Task>& task_ptr, const RunContext& run_ctx);

  u32 worker_id_;
  ThreadType thread_type_;
  bool is_running_;
  bool is_initialized_;

  // Queue references - will be set to point to IpcManager queues
  void* active_queue_;  // Stub - would be multi_mpsc_queue pointer
  void* cold_queue_;    // Stub - would be multi_mpsc_queue pointer

  // Assigned lanes for this worker
  std::vector<LaneId> assigned_lanes_;
  mutable std::shared_mutex assigned_lanes_mutex_;

  // Stack management using malloc and std::vector (replaces hipc::StackAllocator)
  struct StackInfo {
    void* stack_ptr;
    RunContext* run_ctx_ptr;
    size_t stack_size;
    bool in_use;
    
    StackInfo(void* stack_p, RunContext* ctx_p, size_t s) 
        : stack_ptr(stack_p), run_ctx_ptr(ctx_p), stack_size(s), in_use(false) {}
  };
  
  std::vector<StackInfo> stack_pool_;

  // Boost fiber context for task switching
  boost::context::detail::fcontext_t fiber_context_;
  boost::context::detail::transfer_t shared_transfer_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_