/**
 * Worker implementation
 */

#include "chimaera/worker.h"

#include <boost/context/detail/fcontext.hpp>
#include <cstdlib>
#include <iostream>
#include <unordered_set>

// Include task_queue.h before other chimaera headers to ensure proper
// resolution
#include "chimaera/admin/admin_client.h"
#include "chimaera/container.h"
#include "chimaera/pool_manager.h"
#include "chimaera/singletons.h"
#include "chimaera/task.h"
#include "chimaera/task_archives.h"
#include "chimaera/task_queue.h"
#include "chimaera/work_orchestrator.h"

namespace chi {

// Stack detection is now handled by WorkOrchestrator during initialization

Worker::Worker(u32 worker_id, ThreadType thread_type)
    : worker_id_(worker_id), thread_type_(thread_type), is_running_(false),
      is_initialized_(false), did_work_(false), current_run_context_(nullptr),
      assigned_lane_(nullptr) {}

Worker::~Worker() {
  if (is_initialized_) {
    Finalize();
  }
}

bool Worker::Init() {
  if (is_initialized_) {
    return true;
  }

  // Stack management simplified - no pool needed
  // Note: assigned_lane_ will be set by WorkOrchestrator during external queue
  // initialization

  is_initialized_ = true;
  return true;
}

void Worker::Finalize() {
  if (!is_initialized_) {
    return;
  }

  Stop();

  // Stack management simplified - stacks are freed individually when tasks
  // complete Clear assigned lane reference (don't delete - it's in shared
  // memory)
  assigned_lane_ = nullptr;

  is_initialized_ = false;
}

void Worker::Run() {
  if (!is_initialized_) {
    return;
  }
  HILOG(kInfo, "Worker {}: Running", worker_id_);

  // Set current worker once for the entire thread duration
  SetAsCurrentWorker();
  is_running_ = true;

  // Main worker loop - process tasks from assigned lane
  while (is_running_) {
    did_work_ = false; // Reset work tracker at start of each loop iteration

    // Process tasks from the worker's assigned lane
    if (assigned_lane_) {
      // Process up to 64 tasks from this worker's lane per iteration
      const u32 MAX_TASKS_PER_ITERATION = 64;
      u32 tasks_processed = 0;

      while (tasks_processed < MAX_TASKS_PER_ITERATION) {
        hipc::TypedPointer<Task> task_typed_ptr;

        // Pop task from assigned lane
        hipc::FullPtr<TaskLane> lane_full_ptr(assigned_lane_);
        if (::chi::TaskQueue::PopTask(lane_full_ptr, task_typed_ptr)) {
          tasks_processed++;
          did_work_ = true;

          // Convert TypedPointer to FullPtr for consistent API usage
          hipc::FullPtr<Task> task_full_ptr(task_typed_ptr);

          if (!task_full_ptr.IsNull()) {
            // Route task using consolidated routing function
            Container *container = nullptr;
            if (RouteTask(task_full_ptr, assigned_lane_, container)) {
              // Routing successful, execute the task
              BeginTask(task_full_ptr, container, assigned_lane_);
            }
            // Note: RouteTask returning false doesn't always indicate an error
            // Real errors are handled within RouteTask itself
          }
        } else {
          // No more tasks in this lane
          break;
        }
      }
    }

    // Check blocked queue for completed tasks at end of each iteration
    u32 sleep_time_us = ContinueBlockedTasks();

    if (!did_work_) {
      // No work was done in this iteration - safe to yield/sleep
      if (sleep_time_us == 0) {
        // No blocked tasks or immediate recheck needed, just yield briefly
        HSHM_THREAD_MODEL->Yield();
      } else {
        // Sleep for the calculated time until next blocked task should be
        // checked
        HSHM_THREAD_MODEL->SleepForUs(static_cast<size_t>(sleep_time_us));
      }
    }
  }
}

void Worker::Stop() { is_running_ = false; }

void Worker::SetLane(TaskLane *lane) { assigned_lane_ = lane; }

TaskLane *Worker::GetLane() const { return assigned_lane_; }

u32 Worker::GetId() const { return worker_id_; }

ThreadType Worker::GetThreadType() const { return thread_type_; }

bool Worker::IsRunning() const { return is_running_; }

RunContext *Worker::GetCurrentRunContext() const {
  return current_run_context_;
}

RunContext *Worker::SetCurrentRunContext(RunContext *rctx) {
  current_run_context_ = rctx;
  return current_run_context_;
}

FullPtr<Task> Worker::GetCurrentTask() const {
  RunContext *run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return FullPtr<Task>::GetNull();
  }
  return run_ctx->task;
}

Container *Worker::GetCurrentContainer() const {
  RunContext *run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return nullptr;
  }
  return run_ctx->container;
}

TaskLane *Worker::GetCurrentLane() const {
  RunContext *run_ctx = GetCurrentRunContext();
  if (!run_ctx) {
    return nullptr;
  }
  return run_ctx->lane;
}

void Worker::SetAsCurrentWorker() {
  HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_,
                            static_cast<class Worker *>(this));
}

void Worker::ClearCurrentWorker() {
  HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_,
                            static_cast<class Worker *>(nullptr));
}

bool Worker::RouteTask(const FullPtr<Task> &task_ptr, TaskLane *lane,
                       Container *&container) {
  if (task_ptr.IsNull()) {
    return false;
  }

  // Check if task has already been routed - if so, return true immediately
  if (task_ptr->IsRouted()) {
    auto *pool_manager = CHI_POOL_MANAGER;
    container = pool_manager->GetContainer(task_ptr->pool_id_);
    return (container != nullptr);
  }

  // Resolve pool query and route task to container
  std::vector<PoolQuery> pool_queries =
      ResolvePoolQuery(task_ptr->pool_query_, task_ptr->pool_id_);

  // Check if pool_queries is empty - this indicates an error in resolution
  if (pool_queries.empty()) {
    HELOG(kError,
          "Worker {}: Task routing failed - no pool queries resolved. "
          "Pool ID: {}, Method: {}",
          worker_id_, task_ptr->pool_id_, task_ptr->method_);

    // Mark task as complete to end it (safe here since no cleanup needed)
    // But only mark complete for non-periodic tasks
    if (!task_ptr->IsPeriodic()) {
      task_ptr->is_complete.store(1);
    }
    return false;
  }

  // Check if task should be processed locally
  if (IsTaskLocal(pool_queries)) {
    // Route task locally using container query and Monitor with kLocalSchedule
    return RouteLocal(task_ptr, lane, container);
  } else {
    // Route task globally using admin client's ClientSendTaskIn method
    // RouteGlobal never fails, so no need for fallback logic
    RouteGlobal(task_ptr, pool_queries);
    return false; // No local execution needed
  }
}

bool Worker::IsTaskLocal(const std::vector<PoolQuery> &pool_queries) {
  // Task is local only if there is exactly one pool query
  if (pool_queries.size() != 1) {
    return false;
  }

  const PoolQuery &query = pool_queries[0];

  // Check routing mode first, then specific conditions
  RoutingMode routing_mode = query.GetRoutingMode();

  switch (routing_mode) {
  case RoutingMode::Local:
    return true; // Always local

  case RoutingMode::Physical: {
    // Physical mode is local only if targeting local node
    auto *ipc_manager = CHI_IPC;
    u64 local_node_id = ipc_manager ? ipc_manager->GetNodeId() : 0;
    return query.GetNodeId() == local_node_id;
  }

  case RoutingMode::DirectId:
  case RoutingMode::DirectHash:
  case RoutingMode::Range:
  case RoutingMode::Broadcast:
    // These modes should have been resolved to Physical queries by now
    // If we still see them here, they are not local
    return false;
  }

  return false;
}

bool Worker::RouteLocal(const FullPtr<Task> &task_ptr, TaskLane *lane,
                        Container *&container) {
  auto *pool_manager = CHI_POOL_MANAGER;
  container = pool_manager->GetContainer(task_ptr->pool_id_);
  if (!container) {
    return false;
  }

  // Task is local and should be executed directly without re-routing
  // Set TASK_ROUTED flag to indicate this task has been routed
  task_ptr->SetFlags(TASK_ROUTED);

  // Routing successful - caller should execute the task locally
  return true;
}

bool Worker::RouteGlobal(const FullPtr<Task> &task_ptr,
                         const std::vector<PoolQuery> &pool_queries) {
  try {
    // Create admin client to send task to target node
    chimaera::admin::Client admin_client(kAdminPoolId);

    // Create memory context
    hipc::MemContext mctx;

    // Send task using Client API with entire pool queries vector
    admin_client.ClientSendTaskIn(
        mctx,
        pool_queries, // Pass entire pool queries vector
        task_ptr      // Task pointer passed, serialization handled internally
    );

    // Set TASK_ROUTED flag on original task
    task_ptr->SetFlags(TASK_ROUTED);

    // Always return true (never fail)
    return true;

  } catch (const std::exception &e) {
    // Handle any exceptions - still never fail
    task_ptr->SetFlags(TASK_ROUTED);
    return true;
  } catch (...) {
    // Handle unknown exceptions - still never fail
    task_ptr->SetFlags(TASK_ROUTED);
    return true;
  }
}

std::vector<PoolQuery> Worker::ResolvePoolQuery(const PoolQuery &query,
                                                PoolId pool_id) {
  // Basic validation
  if (pool_id.IsNull()) {
    return {}; // Invalid pool ID
  }

  RoutingMode routing_mode = query.GetRoutingMode();

  switch (routing_mode) {
  case RoutingMode::Local:
    return ResolveLocalQuery(query);
  case RoutingMode::DirectId:
    return ResolveDirectIdQuery(query, pool_id);
  case RoutingMode::DirectHash:
    return ResolveDirectHashQuery(query, pool_id);
  case RoutingMode::Range:
    return ResolveRangeQuery(query, pool_id);
  case RoutingMode::Broadcast:
    return ResolveBroadcastQuery(query, pool_id);
  case RoutingMode::Physical:
    return ResolvePhysicalQuery(query, pool_id);
  }

  return {};
}

std::vector<PoolQuery> Worker::ResolveLocalQuery(const PoolQuery &query) {
  // Local routing - process on current node
  return {query};
}

std::vector<PoolQuery> Worker::ResolveDirectIdQuery(const PoolQuery &query,
                                                    PoolId pool_id) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    return {query}; // Fallback to original query
  }

  // Get the container ID from the query
  ContainerId container_id = query.GetContainerId();

  // Get the physical node ID for this container
  u32 node_id = pool_manager->GetContainerNodeId(pool_id, container_id);

  // Create a Physical PoolQuery to that node
  return {PoolQuery::Physical(node_id)};
}

std::vector<PoolQuery> Worker::ResolveDirectHashQuery(const PoolQuery &query,
                                                      PoolId pool_id) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    return {query}; // Fallback to original query
  }

  // Get pool info to find the number of containers
  const PoolInfo *pool_info = pool_manager->GetPoolInfo(pool_id);
  if (!pool_info || pool_info->num_containers_ == 0) {
    return {query}; // Fallback to original query
  }

  // Hash to get container ID
  u32 hash_value = query.GetHash();
  ContainerId container_id = hash_value % pool_info->num_containers_;

  // Get the physical node ID for this container
  u32 node_id = pool_manager->GetContainerNodeId(pool_id, container_id);

  // Create a Physical PoolQuery to that node
  return {PoolQuery::Physical(node_id)};
}

std::vector<PoolQuery> Worker::ResolveRangeQuery(const PoolQuery &query,
                                                 PoolId pool_id) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    return {query}; // Fallback to original query
  }

  u32 range_offset = query.GetRangeOffset();
  u32 range_count = query.GetRangeCount();

  // Validate range
  if (range_count == 0) {
    return {}; // Empty range
  }

  std::vector<PoolQuery> result_queries;

  // Case 1: Range is small enough - create Physical PoolQuery for each
  // container
  if (range_count <= MAX_RANGE_FOR_PHYSICAL_SPLITTING) {
    std::unordered_set<u32> unique_nodes;

    for (u32 i = 0; i < range_count; ++i) {
      ContainerId container_id = range_offset + i;
      u32 node_id = pool_manager->GetContainerNodeId(pool_id, container_id);
      unique_nodes.insert(node_id);
    }

    // Create one Physical query per unique node
    for (u32 node_id : unique_nodes) {
      result_queries.push_back(PoolQuery::Physical(node_id));
    }

    return result_queries;
  }

  // Case 2: Range is large - split into smaller Range queries
  u32 queries_to_create =
      std::min(range_count, MAX_POOL_QUERIES_PER_RESOLUTION);
  u32 containers_per_query = range_count / queries_to_create;
  u32 remaining_containers = range_count % queries_to_create;

  u32 current_offset = range_offset;
  for (u32 i = 0; i < queries_to_create; ++i) {
    u32 current_count = containers_per_query;
    if (i < remaining_containers) {
      current_count++; // Distribute remainder across first queries
    }

    if (current_count > 0) {
      result_queries.push_back(PoolQuery::Range(current_offset, current_count));
      current_offset += current_count;
    }
  }

  return result_queries;
}

std::vector<PoolQuery> Worker::ResolveBroadcastQuery(const PoolQuery &query,
                                                     PoolId pool_id) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    return {query}; // Fallback to original query
  }

  // Get pool info to find the total number of containers
  const PoolInfo *pool_info = pool_manager->GetPoolInfo(pool_id);
  if (!pool_info || pool_info->num_containers_ == 0) {
    return {query}; // Fallback to original query
  }

  // Create a Range query that covers all containers, then resolve it
  PoolQuery range_query = PoolQuery::Range(0, pool_info->num_containers_);
  return ResolveRangeQuery(range_query, pool_id);
}

std::vector<PoolQuery> Worker::ResolvePhysicalQuery(const PoolQuery &query,
                                                    PoolId pool_id) {
  // Physical routing - query is already resolved to a specific node
  return {query};
}

RunContext Worker::CreateRunContext(const FullPtr<Task> &task_ptr) {
  // This method is deprecated - use AllocateStackAndContext instead
  // Creating a temporary RunContext for compatibility
  RunContext run_ctx;
  run_ctx.thread_type = thread_type_;
  run_ctx.worker_id = worker_id_;
  run_ctx.stack_size = 65536;  // 64KB
  run_ctx.stack_ptr = nullptr; // Will be set by AllocateStackAndContext
  return run_ctx;
}

RunContext *Worker::AllocateStackAndContext(size_t size) {
  // Allocate aligned stack and RunContext
  // Use page alignment for the stack to match unit test pattern
  const size_t page_size = 4096;
  size = ((size + page_size - 1) / page_size) * page_size;

  void *stack_base = nullptr;
  int ret = posix_memalign(&stack_base, page_size, size);
  RunContext *new_run_ctx =
      static_cast<RunContext *>(malloc(sizeof(RunContext)));

  if (ret == 0 && stack_base && new_run_ctx) {
    // Initialize RunContext using placement new
    new (new_run_ctx) RunContext();

    // Zero the stack memory for consistent behavior
    std::memset(stack_base, 0, size);

    // Store the malloc base pointer for freeing later
    new_run_ctx->stack_base_for_free = stack_base;
    new_run_ctx->stack_size = size;

    // Set the correct stack pointer based on stack growth direction from work
    // orchestrator
    WorkOrchestrator *orchestrator = CHI_WORK_ORCHESTRATOR;
    bool grows_downward = orchestrator ? orchestrator->IsStackDownward()
                                       : true; // Default to downward

    if (grows_downward) {
      // Stack grows downward: point to aligned end of the malloc buffer
      // Ensure 16-byte alignment (required for x86-64 ABI)
      char *stack_top = static_cast<char *>(stack_base) + size;
      new_run_ctx->stack_ptr = reinterpret_cast<void *>(
          reinterpret_cast<uintptr_t>(stack_top) & ~static_cast<uintptr_t>(15));
    } else {
      // Stack grows upward: point to the beginning of the malloc buffer
      // Ensure 16-byte alignment
      new_run_ctx->stack_ptr = reinterpret_cast<void *>(
          (reinterpret_cast<uintptr_t>(stack_base) + 15) &
          ~static_cast<uintptr_t>(15));
    }

    // Verify alignment
    if (!new_run_ctx->stack_ptr ||
        reinterpret_cast<uintptr_t>(new_run_ctx->stack_ptr) % 16 != 0) {
      // Alignment failure - clean up and return nullptr
      free(stack_base);
      free(new_run_ctx);
      return nullptr;
    }

    return new_run_ctx;
  }

  // Cleanup on failure
  if (stack_base)
    free(stack_base);
  if (new_run_ctx)
    free(new_run_ctx);

  return nullptr;
}

void Worker::DeallocateStackAndContext(RunContext *run_ctx) {
  if (!run_ctx) {
    return;
  }

  // Free the stack using the original malloc base pointer
  if (run_ctx->stack_base_for_free) {
    free(run_ctx->stack_base_for_free);
  }

  // Call destructor explicitly before freeing
  run_ctx->~RunContext();
  free(run_ctx);
}

void Worker::BeginTask(const FullPtr<Task> &task_ptr, Container *container,
                       TaskLane *lane) {
  if (task_ptr.IsNull()) {
    return;
  }

  // Allocate stack and RunContext together for new task
  RunContext *run_ctx = AllocateStackAndContext(65536); // 64KB default

  if (!run_ctx) {
    // FATAL: Stack allocation failure - this is a critical error
    HELOG(kFatal,
          "Worker {}: Failed to allocate stack for task execution. Task "
          "method: {}, pool: {}",
          worker_id_, task_ptr->method_, task_ptr->pool_id_);
    std::abort(); // Fatal failure
  }

  // Initialize RunContext for new task
  run_ctx->thread_type = thread_type_;
  run_ctx->worker_id = worker_id_;
  run_ctx->task = task_ptr;           // Store task in RunContext
  run_ctx->is_blocked = false;        // Initially not blocked
  run_ctx->container = container;     // Store container for CHI_CUR_CONTAINER
  run_ctx->lane = lane;               // Store lane for CHI_CUR_LANE
  run_ctx->waiting_for_tasks.clear(); // Clear waiting tasks for new task
  // Set RunContext pointer in task
  task_ptr->run_ctx_ = run_ctx;

  // Use unified execution function
  ExecTask(task_ptr, run_ctx, false);
}

void Worker::EndTaskWithError(const FullPtr<Task> &task_ptr, u32 error_code) {
  if (task_ptr.IsNull()) {
    return;
  }

  // If task is fire-and-forget, delete it immediately
  // Fire-and-forget tasks don't need to be kept around for the client
  if (task_ptr->IsFireAndForget()) {
    auto *ipc_manager = CHI_IPC;
    if (ipc_manager) {
      // DelTask now takes a copy, so we can pass task_ptr directly
      ipc_manager->DelTask(task_ptr);
    }
  } else {
    // Set the error return code and mark task as complete
    task_ptr->SetReturnCode(error_code);
    task_ptr->is_complete.store(1);
  }

  // Note: Non-fire-and-forget tasks are left in memory for the client to check
  // the return code and completion status before explicitly deleting them
}

void Worker::CheckBlockedQueueDuplicates(const std::string &label) {
  std::unordered_set<RunContext *> seen_contexts;

  // Make a copy of the queue with mutex protection
  std::priority_queue<RunContext *, std::vector<RunContext *>,
                      RunContextComparator>
      check_queue;
  {
    std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
    check_queue = blocked_queue_;
  }

  while (!check_queue.empty()) {
    RunContext *run_ctx = check_queue.top();
    check_queue.pop();

    if (seen_contexts.count(run_ctx) > 0) {
      // Found a duplicate!
      HELOG(kFatal,
            "Worker {}: CRITICAL BUG - Duplicate RunContext in blocked_queue_ "
            "[{}]! "
            "Task ptr: {:#x}, Pool: {}, Method: {}, TaskNode: {}.{}.{}.{}",
            worker_id_, label, reinterpret_cast<uintptr_t>(run_ctx->task.ptr_),
            run_ctx->task->pool_id_, run_ctx->task->method_,
            run_ctx->task->task_node_.pid_, run_ctx->task->task_node_.tid_,
            run_ctx->task->task_node_.major_, run_ctx->task->task_node_.minor_);
      std::abort();
    }
    seen_contexts.insert(run_ctx);
  }
}

u32 Worker::ContinueBlockedTasks() {
  // Check for duplicates at the start
  CheckBlockedQueueDuplicates("START");

  // Create a copy of the blocked queue and clear the original with mutex
  // protection
  std::priority_queue<RunContext *, std::vector<RunContext *>,
                      RunContextComparator>
      queue_copy;
  {
    std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
    queue_copy = blocked_queue_;
    blocked_queue_ =
        std::priority_queue<RunContext *, std::vector<RunContext *>,
                            RunContextComparator>();
  }

  u32 return_value = 0;      // Default return value for immediate recheck
  bool should_break = false; // Flag to indicate we should stop processing

  // FIRST LOOP: Process ready tasks and check if we should continue
  while (!queue_copy.empty() && !should_break) {
    RunContext *run_ctx = queue_copy.top();
    queue_copy.pop();

    if (run_ctx && !run_ctx->task.IsNull()) {
      // Always check if all subtasks are completed first
      if (run_ctx->AreSubtasksCompleted()) {
        // All subtasks are completed, resume this task immediately
        run_ctx->is_blocked = false;

        // Use unified execution function to resume the task
        ExecTask(run_ctx->task, run_ctx, true);
        // Task execution handled, continue to next task
        continue;
      }

      // Subtasks not completed - calculate remaining time and re-queue
      hshm::Timepoint current_time;
      current_time.Now();
      double elapsed_us = current_time.GetUsecFromStart(run_ctx->block_time);

      // Decrement the remaining time by the current elapsed time
      run_ctx->estimated_completion_time_us -= elapsed_us;
      if (run_ctx->estimated_completion_time_us < 0) {
        run_ctx->estimated_completion_time_us = 0;
      }

      // Update the block time for next iteration
      run_ctx->block_time.Now();

      // Set return value based on remaining time
      if (run_ctx->estimated_completion_time_us <= 0) {
        return_value = 0; // Time estimate exceeded, immediate recheck needed
      } else {
        return_value = static_cast<u32>(run_ctx->estimated_completion_time_us);
      }

      // Add this task back to the blocked queue with mutex protection
      {
        std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
        blocked_queue_.push(run_ctx);
      }

      // Stop processing if this task has time remaining
      if (run_ctx->estimated_completion_time_us > 0) {
        should_break = true;
      }
    }
  }

  // SECOND LOOP: Move all remaining tasks from queue_copy back to
  // blocked_queue_
  {
    std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
    while (!queue_copy.empty()) {
      RunContext *run_ctx = queue_copy.top();
      queue_copy.pop();

      // Simply transfer back to blocked queue without processing
      blocked_queue_.push(run_ctx);
    }
  }

  // Check for duplicates at the end
  CheckBlockedQueueDuplicates("END");

  return return_value;
}

bool Worker::ExecTask(const FullPtr<Task> &task_ptr, RunContext *run_ctx,
                      bool is_started) {
  if (task_ptr.IsNull() || !run_ctx) {
    return true; // Consider null tasks as completed
  }

  // Mark that work is being done
  did_work_ = true;

  // Set current run context
  // Note: Current worker is already set for thread duration
  SetCurrentRunContext(run_ctx);

  if (is_started) {
    // Resume execution - jump back to where the task yielded (stored in
    // fiber_transfer) The fiber_transfer contains the yield point, not the
    // fiber start

    // Validate resume_context before jumping
    if (!run_ctx->resume_context.fctx) {
      HELOG(kFatal,
            "Worker {}: resume_context.fctx is null when resuming task. "
            "Stack: {} Size: {} Task method: {} Pool: {}",
            worker_id_, run_ctx->stack_ptr, run_ctx->stack_size,
            task_ptr->method_, task_ptr->pool_id_);
      std::abort();
    }

    // Check if stack pointer is still valid
    if (!run_ctx->stack_ptr || !run_ctx->stack_base_for_free) {
      HELOG(kFatal,
            "Worker {}: Stack context is invalid when resuming task. "
            "stack_ptr: {} stack_base: {} Task method: {} Pool: {}",
            worker_id_, run_ctx->stack_ptr, run_ctx->stack_base_for_free,
            task_ptr->method_, task_ptr->pool_id_);
      std::abort();
    }

    // Validate that resume_context.fctx points within the allocated stack range
    uintptr_t fctx_addr =
        reinterpret_cast<uintptr_t>(run_ctx->resume_context.fctx);
    uintptr_t stack_start =
        reinterpret_cast<uintptr_t>(run_ctx->stack_base_for_free);
    uintptr_t stack_end = stack_start + run_ctx->stack_size;

    if (fctx_addr < stack_start || fctx_addr > stack_end) {
      HELOG(kWarning,
            "Worker {}: resume_context.fctx ({:#x}) is outside stack range "
            "[{:#x}, {:#x}]. "
            "Task method: {} Pool: {}",
            worker_id_, fctx_addr, stack_start, stack_end, task_ptr->method_,
            task_ptr->pool_id_);
    }

    HILOG(kDebug,
          "Worker {}: Resuming task - fctx: {:#x}, stack: [{:#x}, {:#x}], "
          "method: {}",
          worker_id_, fctx_addr, stack_start, stack_end, task_ptr->method_);

    // Resume execution - jump back to task's yield point
    // Use temporary variables to avoid read/write conflict on resume_context
    bctx::fcontext_t resume_fctx = run_ctx->resume_context.fctx;
    void *resume_data = run_ctx->resume_context.data;

    // Jump to task's yield point and capture the result
    // This provides the current worker context to the fiber for proper return
    bctx::transfer_t resume_result =
        bctx::jump_fcontext(resume_fctx, resume_data);

    // Update yield_context with current worker context so task can return here
    // This is critical: the task needs to know where to return when it
    // completes or yields again
    run_ctx->yield_context = resume_result;

    // Update resume_context only if the task yielded again (is_blocked = true)
    if (run_ctx->is_blocked) {
      run_ctx->resume_context = resume_result;
    }
  } else {
    // New task execution
    // Increment work count for non-periodic tasks at task start
    if (run_ctx->container && !task_ptr->IsPeriodic()) {
      // Increment work remaining in the container for non-periodic tasks
      run_ctx->container->UpdateWork(task_ptr, *run_ctx, 1);
    }

    // Create fiber context for this task
    // stack_ptr is already correctly positioned based on stack growth direction
    bctx::fcontext_t fiber_fctx = bctx::make_fcontext(
        run_ctx->stack_ptr, run_ctx->stack_size, FiberExecutionFunction);

    // Jump to fiber context to execute the task
    // This will provide the worker context to FiberExecutionFunction through
    // the parameter
    bctx::transfer_t fiber_result = bctx::jump_fcontext(fiber_fctx, nullptr);

    // Update yield_context with current worker context so task can return here
    // The fiber_result contains the worker context for the task to use
    run_ctx->yield_context = fiber_result;

    // Update resume_context only if the task actually yielded (is_blocked =
    // true)
    if (run_ctx->is_blocked) {
      run_ctx->resume_context = fiber_result;
    }
  }

  // Common cleanup logic for both fiber and direct execution
  if (run_ctx->is_blocked) {
    // Task is blocked - don't clean up, will be resumed later
    return false; // Task is not completed, blocked for later resume
  }

  // Tasks should never return incomplete from ExecTask
  // Periodic tasks are rescheduled by ReschedulePeriodicTask in
  // FiberExecutionFunction Non-periodic tasks are marked complete here AFTER
  // cleanup to avoid race conditions

  // Determine if task should be completed and cleaned up
  bool should_complete_task = !task_ptr->IsPeriodic();
  bool is_fire_and_forget = task_ptr->IsFireAndForget();

  if (should_complete_task) {
    // CRITICAL BUG CHECK: Verify this task is not in the blocked queue
    // This should never happen - if it does, it's a serious bug
    std::priority_queue<RunContext *, std::vector<RunContext *>,
                        RunContextComparator>
        temp_queue;
    {
      std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
      temp_queue = blocked_queue_;
    }

    while (!temp_queue.empty()) {
      RunContext *blocked_ctx = temp_queue.top();
      temp_queue.pop();

      if (blocked_ctx == run_ctx) {
        HELOG(
            kFatal,
            "Worker {}: CRITICAL BUG - Completed task found in blocked queue! "
            "Task ptr: {:#x}, Pool: {}, Method: {}, TaskNode: {}.{}.{}.{}",
            worker_id_, reinterpret_cast<uintptr_t>(task_ptr.ptr_),
            task_ptr->pool_id_, task_ptr->method_, task_ptr->task_node_.pid_,
            task_ptr->task_node_.tid_, task_ptr->task_node_.major_,
            task_ptr->task_node_.minor_);
        std::abort();
      }
    }

    // Clear RunContext pointer first
    task_ptr->run_ctx_ = nullptr;

    // Mark task as complete FIRST - before deletion
    // This prevents race condition with client DelTask
    task_ptr->is_complete.store(1);

    // For fire-and-forget tasks, automatically delete them after completion
    if (is_fire_and_forget && run_ctx->container) {
      run_ctx->container->Del(task_ptr->method_, task_ptr);
    }

    // Deallocate stack and context AFTER task is deleted
    DeallocateStackAndContext(run_ctx);
  }
  // Return whether the task should be considered completed
  return should_complete_task;
}

void Worker::AddToBlockedQueue(RunContext *run_ctx, double estimated_time_us) {
  if (!run_ctx || run_ctx->task.IsNull()) {
    return;
  }

  auto *worker = CHI_CUR_WORKER;
  if (worker != this) {
    HELOG(
        kFatal,
        "Worker {}: AddToBlockedQueue - Task ptr: {:#x}, Pool: {}, Method: {}, "
        "TaskNode: {}.{}.{}.{}, EstimatedTime: {} us",
        worker_id_, reinterpret_cast<uintptr_t>(run_ctx->task.ptr_),
        run_ctx->task->pool_id_, run_ctx->task->method_,
        run_ctx->task->task_node_.pid_, run_ctx->task->task_node_.tid_,
        run_ctx->task->task_node_.major_, run_ctx->task->task_node_.minor_,
        estimated_time_us);
  }

  // Set timing information in RunContext
  run_ctx->estimated_completion_time_us = estimated_time_us;
  run_ctx->block_time.Now();

  // Add RunContext to the blocked queue (priority queue) with mutex protection
  {
    std::lock_guard<std::mutex> lock(blocked_queue_mutex_);
    blocked_queue_.push(run_ctx);
  }
}

void Worker::ReschedulePeriodicTask(RunContext *run_ctx,
                                    const FullPtr<Task> &task_ptr) {
  if (!run_ctx || task_ptr.IsNull() || !task_ptr->IsPeriodic()) {
    return;
  }

  // Get the lane from the run context
  TaskLane *lane = run_ctx->lane;
  if (!lane) {
    // No lane information, cannot reschedule
    return;
  }

  // Check if the lane still maps to this worker by checking the lane header
  auto &header = lane->GetHeader();
  double period_us = task_ptr->GetPeriod(kMicro);
  AddToBlockedQueue(run_ctx, period_us);
}

void Worker::FiberExecutionFunction(boost::context::detail::transfer_t t) {
  // This function runs in the fiber context
  // Use thread-local storage to get context
  Worker *worker = CHI_CUR_WORKER;
  RunContext *run_ctx = worker->GetCurrentRunContext();
  FullPtr<Task> task_ptr =
      worker ? worker->GetCurrentTask() : FullPtr<Task>::GetNull();

  if (!task_ptr.IsNull() && worker && run_ctx) {
    // Store the worker's context (from parameter t) - this is where we jump
    // back when yielding or when task completes
    run_ctx->yield_context = t;
    // Execute the task directly - merged TaskExecutionFunction logic
    try {
      // Get the container from RunContext
      Container *container = run_ctx->container;

      if (container) {
        // Call the container's Run function with the task
        container->Run(task_ptr->method_, task_ptr, *run_ctx);
      } else {
        // Container not found - this is an error condition
        HILOG(kWarning, "Container not found in RunContext for pool_id: {}",
              task_ptr->pool_id_);
      }
    } catch (const std::exception &e) {
      // Handle execution errors
      HELOG(kError, "Task execution failed: {}", e.what());
    } catch (...) {
      // Handle unknown errors
      HELOG(kError, "Task execution failed with unknown exception");
    }

    // Handle task completion and rescheduling
    if (task_ptr->IsPeriodic()) {
      // Periodic tasks are always rescheduled regardless of execution success
      worker->ReschedulePeriodicTask(run_ctx, task_ptr);
    } else {
      // Non-periodic task completed - decrement work count and mark as complete
      if (run_ctx->container) {
        // Decrement work remaining in the container for non-periodic tasks
        run_ctx->container->UpdateWork(task_ptr, *run_ctx, -1);
      }

      // Don't mark as complete yet - defer until cleanup is done in ExecTask
      // to avoid race condition with client DelTask
    }
  }

  // Jump back to worker context when task completes
  // Use temporary variables to avoid potential read/write conflicts
  bctx::fcontext_t worker_fctx = run_ctx->yield_context.fctx;
  void *worker_data = run_ctx->yield_context.data;
  bctx::jump_fcontext(worker_fctx, worker_data);
}

} // namespace chi