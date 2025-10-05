#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_

#include <boost/context/detail/fcontext.hpp>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "chimaera/container.h"
#include "chimaera/pool_query.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"
#include "chimaera/types.h"

namespace chi {

// Forward declaration to avoid circular dependency
using WorkQueue = chi::ipc::mpsc_queue<hipc::TypedPointer<TaskLane>>;

// Forward declarations  
class Task;


// Macro for accessing HSHM thread-local storage (worker thread context)
// This macro allows access to the current worker from any thread
// Example usage in ChiMod container code:
//   Worker* worker = CHI_CUR_WORKER;
//   FullPtr<Task> current_task = worker->GetCurrentTask();
//   RunContext* run_ctx = worker->GetCurrentRunContext();
#define CHI_CUR_WORKER \
  (HSHM_THREAD_MODEL->GetTls<class Worker>(chi_cur_worker_key_))


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
   * Get current RunContext for this worker thread
   * @return Pointer to current RunContext or nullptr
   */
  RunContext* GetCurrentRunContext() const;

  /**
   * Set current RunContext for this worker thread
   * @param rctx Pointer to RunContext to set as current
   * @return Pointer to the set RunContext
   */
  RunContext* SetCurrentRunContext(RunContext* rctx);

  /**
   * Get current task from the current RunContext
   * @return FullPtr to current task or null if no RunContext
   */
  FullPtr<Task> GetCurrentTask() const;

  /**
   * Get current container from the current RunContext
   * @return Pointer to current container or nullptr if no RunContext
   */
  Container* GetCurrentContainer() const;

  /**
   * Get current lane from the current RunContext
   * @return Pointer to current lane or nullptr if no RunContext
   */
  TaskLane* GetCurrentLane() const;

  /**
   * Set this worker as the current worker in thread-local storage
   */
  void SetAsCurrentWorker();

  /**
   * Clear the current worker from thread-local storage
   */
  static void ClearCurrentWorker();

  /**
   * Add run context to blocked queue with estimated completion time
   * @param run_ctx_ptr Pointer to run context (task accessible via
   * run_ctx_ptr->task)
   * @param estimated_time_us Estimated completion time in microseconds
   */
  void AddToBlockedQueue(RunContext* run_ctx_ptr, double estimated_time_us);

  /**
   * Reschedule a periodic task for next execution
   * Checks if lane still maps to this worker - if so, adds to blocked queue
   * Otherwise, reschedules task back to the lane
   * @param run_ctx_ptr Pointer to run context 
   * @param task_ptr Full pointer to the periodic task
   */
  void ReschedulePeriodicTask(RunContext* run_ctx_ptr, const FullPtr<Task>& task_ptr);

  /**
   * Set the worker's assigned lane
   * @param lane Pointer to the TaskLane assigned to this worker
   */
  void SetLane(TaskLane* lane);

  /**
   * Get the worker's assigned lane
   * @return Pointer to the TaskLane assigned to this worker
   */
  TaskLane* GetLane() const;

  /**
   * Route a task by calling ResolvePoolQuery and determining local vs global scheduling
   * @param task_ptr Full pointer to task to route
   * @param lane Pointer to the task lane for execution context
   * @param container Output parameter for the container to use for task execution
   * @return true if task was successfully routed, false otherwise
   */
  bool RouteTask(const FullPtr<Task>& task_ptr, TaskLane* lane, Container*& container);

  /**
   * Resolve a pool query into concrete physical addresses
   * @param query Pool query to resolve
   * @param pool_id Pool ID for the query
   * @return Vector of pool queries for routing
   */
  std::vector<PoolQuery> ResolvePoolQuery(const PoolQuery& query, PoolId pool_id);

private:
  // Pool query resolution helper functions
  std::vector<PoolQuery> ResolveLocalQuery(const PoolQuery& query);
  std::vector<PoolQuery> ResolveDirectIdQuery(const PoolQuery& query, PoolId pool_id);
  std::vector<PoolQuery> ResolveDirectHashQuery(const PoolQuery& query, PoolId pool_id);
  std::vector<PoolQuery> ResolveRangeQuery(const PoolQuery& query, PoolId pool_id);
  std::vector<PoolQuery> ResolveBroadcastQuery(const PoolQuery& query, PoolId pool_id);
  std::vector<PoolQuery> ResolvePhysicalQuery(const PoolQuery& query, PoolId pool_id);

public:

  /**
   * Check if task should be processed locally based on pool queries
   * @param pool_queries Vector of pool queries from ResolvePoolQuery
   * @return true if task should be processed locally, false for global routing
   */
  bool IsTaskLocal(const std::vector<PoolQuery>& pool_queries);

  /**
   * Route task locally using container query and Monitor with kLocalSchedule
   * @param task_ptr Full pointer to task to route locally
   * @param lane Pointer to the task lane for execution context
   * @param container Output parameter for the container to use for task execution
   * @return true if local routing successful, false otherwise
   */
  bool RouteLocal(const FullPtr<Task>& task_ptr, TaskLane* lane, Container*& container);

  /**
   * Route task globally using admin client's ClientSendTaskIn method
   * @param task_ptr Full pointer to task to route globally
   * @param pool_queries Vector of pool queries for global routing
   * @return true if global routing successful, false otherwise
   */
  bool RouteGlobal(const FullPtr<Task>& task_ptr, const std::vector<PoolQuery>& pool_queries);

 private:
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
   * @param run_ctx_ptr Pointer to RunContext containing stack info to
   * deallocate
   */
  void DeallocateStackAndContext(RunContext* run_ctx_ptr);

  /**
   * Begin task execution using boost::fiber for context switching
   * @param task_ptr Full pointer to task to execute (RunContext will be
   * allocated and set in task)
   * @param container Container for the task
   * @param lane Lane for the task (can be nullptr)
   */
  void BeginTask(const FullPtr<Task>& task_ptr, Container* container,
                 TaskLane* lane);

  /**
   * End task with error due to routing failure
   * Sets the task return code, marks it as complete, and deletes fire-and-forget tasks
   * @param task_ptr Full pointer to task that failed routing
   * @param error_code Error code to set (default: 1 for general error)
   */
  void EndTaskWithError(const FullPtr<Task>& task_ptr, u32 error_code = 1);

  /**
   * Continue processing resumed tasks from priority 1 lane (CoMutex/CoRwLock unblocked tasks)
   * Called before ContinueBlockedTasks to give priority to lock-resumed tasks
   */
  void ContinueResumedTasks();

  /**
   * Continue processing blocked tasks that are ready to resume
   * @return Number of microseconds the worker could sleep if no new work, 0 if
   * immediate work available
   */
  u32 ContinueBlockedTasks();

  /**
   * Execute task with context switching capability
   * @param task_ptr Full pointer to task to execute
   * @param run_ctx_ptr Pointer to existing RunContext
   * @param is_started True if task is resuming, false for new task
   */
  bool ExecTask(const FullPtr<Task>& task_ptr, RunContext* run_ctx_ptr,
                bool is_started);

  /**
   * Static function for boost::fiber execution context
   * @param t Transfer context for boost::fiber
   */
  static void FiberExecutionFunction(boost::context::detail::transfer_t t);

  u32 worker_id_;
  ThreadType thread_type_;
  bool is_running_;
  bool is_initialized_;
  bool did_work_;  // Tracks if any work was done in current loop iteration

  // Current RunContext for this worker thread
  RunContext* current_run_context_;

  // Single lane assigned to this worker (one lane per worker)
  TaskLane* assigned_lane_;

  // Stack management simplified - allocate/free directly with malloc

  // Blocked queue stores RunContext pointers directly (priority queue by
  // completion time)
  struct RunContextComparator {
    bool operator()(const RunContext* lhs, const RunContext* rhs) const {
      return lhs->estimated_completion_time_us >
             rhs->estimated_completion_time_us;
    }
  };

  // Two blocked queues - we alternate between them using a bit
  // This allows lock-free operation: one queue is being processed while
  // new blocked tasks go to the other queue
  std::priority_queue<RunContext*, std::vector<RunContext*>,
                      RunContextComparator>
      blocked_queue_[2];

  // Bit to select which blocked queue to use for new additions
  // Flipped in ContinueBlockedTasks to alternate between queues
  bool block_queue_bit_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_