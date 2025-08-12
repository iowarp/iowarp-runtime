#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_

#include <thread>
#include <vector>
#include <queue>
#include <chrono>
#include <boost/context/detail/fcontext.hpp>
#include "chimaera/types.h"
#include "chimaera/task.h"
#include "chimaera/chimod_spec.h"
#include "chimaera/domain_query.h"
#include "chimaera/task_queue.h"

namespace chi {

// Forward declarations
class Task;
class TaskQueue;

// Macros for accessing HSHM thread-local storage (worker thread context)
// These macros allow access to the current task, run context, and worker from any thread
// Example usage in ChiMod container code:
//   FullPtr<Task> current_task = CHI_CUR_TASK;
//   RunContext* run_ctx = CHI_CUR_RCTX;
//   Worker* worker = CHI_CUR_WORKER;
#define CHI_CUR_TASK (CHI_CUR_RCTX ? CHI_CUR_RCTX->current_task : FullPtr<Task>::GetNull())
#define CHI_CUR_RCTX (HSHM_THREAD_MODEL->GetTls<struct RunContext>(chi_cur_rctx_key_))
#define CHI_CUR_WORKER (HSHM_THREAD_MODEL->GetTls<class Worker>(chi_cur_worker_key_))
#define CHI_CUR_CONTAINER (CHI_CUR_RCTX ? static_cast<ChiContainer*>(CHI_CUR_RCTX->container) : nullptr)
#define CHI_CUR_LANE (CHI_CUR_RCTX ? static_cast<Lane*>(CHI_CUR_RCTX->lane) : nullptr)

// Helper macros for setting thread-local storage (internal use)
#define CHI_SET_CUR_TASK(task_ptr) (CHI_CUR_RCTX ? (CHI_CUR_RCTX->current_task = (task_ptr)) : FullPtr<Task>::GetNull())
#define CHI_SET_CUR_RCTX(rctx) (HSHM_THREAD_MODEL->SetTls(chi_cur_rctx_key_, static_cast<struct RunContext*>(rctx)))
#define CHI_SET_CUR_WORKER(worker) (HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<class Worker*>(worker)))

// Helper macros for clearing thread-local storage (internal use)
#define CHI_CLEAR_CUR_TASK() (CHI_CUR_RCTX ? (CHI_CUR_RCTX->current_task = FullPtr<Task>::GetNull()) : FullPtr<Task>::GetNull())
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

  /**
   * Add task to blocked queue with estimated completion time
   * @param task_ptr Full pointer to task
   * @param run_ctx_ptr Pointer to run context
   * @param estimated_time_us Estimated completion time in microseconds
   */
  void AddToBlockedQueue(const FullPtr<Task>& task_ptr, RunContext* run_ctx_ptr, double estimated_time_us);

  /**
   * Enqueue a lane to this worker's active queue for processing
   * @param lane_ptr FullPtr to lane (as returned by GetLane) that has work available
   */
  void EnqueueLane(hipc::FullPtr<TaskQueue::TaskLane> lane_ptr);

 private:
  /**
   * Pop task from active lane queue
   * @return Pointer to task or nullptr if queue empty
   */
  Task* PopActiveTask();


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
   * @param container Container for the task
   * @param lane Lane for the task (can be nullptr)
   */
  void ExecuteTask(const FullPtr<Task>& task_ptr, ChiContainer* container, Lane* lane);

  /**
   * Unified task execution function with context reuse capability
   * @param task_ptr Full pointer to task to execute
   * @param run_ctx_ptr Pointer to existing RunContext (can be null for new tasks)
   * @param is_started True if task is resuming, false for new task
   */
  void ExecuteTaskUnified(const FullPtr<Task>& task_ptr, RunContext* run_ctx_ptr, bool is_started);

  /**
   * Task execution function - calls container's Run function
   * @param task_ptr Full pointer to task to execute
   * @param run_ctx Run context
   * @return true if task completed successfully, false otherwise
   */
  bool TaskExecutionFunction(const FullPtr<Task>& task_ptr, const RunContext& run_ctx);

  /**
   * Check blocked queue for completed tasks
   * @return Number of microseconds the worker could sleep if no new work, 0 if immediate work available
   */
  u32 CheckBlockedQueue();

  /**
   * Static function for boost::fiber execution context
   * @param t Transfer context for boost::fiber
   */
  static void FiberExecutionFunction(boost::context::detail::transfer_t t);

  u32 worker_id_;
  ThreadType thread_type_;
  bool is_running_;
  bool is_initialized_;

  // Active queue of lanes for processing
  // GetLane returns a lane reference, so we store lane FullPtrs
  hipc::FullPtr<hipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>> active_queue_;  // Queue of lane FullPtrs from GetLane

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

  // Blocked queue for partially completed tasks (priority queue by completion time)
  struct BlockedTask {
    FullPtr<Task> task_ptr;
    RunContext* run_ctx_ptr;
    double estimated_completion_time_us;
    std::chrono::steady_clock::time_point block_time;
    
    BlockedTask(const FullPtr<Task>& task, RunContext* ctx, double est_time)
        : task_ptr(task), run_ctx_ptr(ctx), estimated_completion_time_us(est_time),
          block_time(std::chrono::steady_clock::now()) {}
    
    // Comparator for priority queue (earliest completion time first)
    bool operator>(const BlockedTask& other) const {
      return estimated_completion_time_us > other.estimated_completion_time_us;
    }
  };
  
  std::priority_queue<BlockedTask, std::vector<BlockedTask>, std::greater<BlockedTask>> blocked_queue_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_