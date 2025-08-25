#ifndef CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_

#include <boost/context/detail/fcontext.hpp>
#include <chrono>
#include <queue>
#include <thread>
#include <vector>

#include "chimaera/chimod_spec.h"
#include "chimaera/pool_query.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"
#include "chimaera/types.h"

namespace chi {

// Forward declarations
class Task;
class TaskQueue;

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
  ChiContainer* GetCurrentContainer() const;

  /**
   * Get current lane from the current RunContext
   * @return Pointer to current lane or nullptr if no RunContext
   */
  TaskQueue::TaskLane* GetCurrentLane() const;

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
   * Enqueue a lane to this worker's active queue for processing
   * @param lane_ptr FullPtr to lane (as returned by GetLane) that has work
   * available
   */
  void EnqueueLane(hipc::FullPtr<TaskQueue::TaskLane> lane_ptr);

  /**
   * Resolve a pool query into concrete physical addresses and update RuntimeContext
   * @param task_ptr Full pointer to task with pool query to resolve
   * @return Vector of resolved pool queries with concrete addresses
   */
  std::vector<ResolvedPoolQuery> ResolvePoolQuery(const FullPtr<Task>& task_ptr);

 private:
  /**
   * Pop task from active lane queue
   * @return Pointer to task or nullptr if queue empty
   */
  Task* PopActiveTask();


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
  bool CallMonitorForLocalSchedule(ChiContainer* container,
                                   const FullPtr<Task>& task_ptr);

  /**
   * Check if task should be scheduled remotely based on domain query and load balancing
   * @param task_ptr Full pointer to task to check
   * @return true if task should be sent to remote node, false for local processing
   */
  bool ShouldScheduleRemotely(const FullPtr<Task>& task_ptr);

  /**
   * Schedule task for remote execution via admin chimod networking methods
   * @param task_ptr Full pointer to task to send remotely
   * @return true if task was successfully sent remotely, false otherwise
   */
  bool ScheduleTaskRemotely(const FullPtr<Task>& task_ptr);

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
  void BeginTask(const FullPtr<Task>& task_ptr, ChiContainer* container,
                 TaskQueue::TaskLane* lane);

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
  void ExecTask(const FullPtr<Task>& task_ptr, RunContext* run_ctx_ptr,
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

  // Active queue of lanes for processing
  // GetLane returns a lane reference, so we store lane FullPtrs
  hipc::FullPtr<chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>
      active_queue_;  // Queue of lane FullPtrs from GetLane

  // Stack management simplified - allocate/free directly with malloc

  // Blocked queue stores RunContext pointers directly (priority queue by
  // completion time)
  struct RunContextComparator {
    bool operator()(const RunContext* lhs, const RunContext* rhs) const {
      return lhs->estimated_completion_time_us >
             rhs->estimated_completion_time_us;
    }
  };

  std::priority_queue<RunContext*, std::vector<RunContext*>,
                      RunContextComparator>
      blocked_queue_;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_WORKERS_WORKER_H_