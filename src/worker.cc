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
      assigned_lane_(nullptr),
      stack_cache_(64), // Initial capacity for stack cache (64 entries)
      last_long_queue_check_(0) {
  // Initialize all blocked queues with capacity 1024
  for (u32 i = 0; i < NUM_BLOCKED_QUEUES; ++i) {
    blocked_queues_[i] = hshm::ext_ring_buffer<RunContext *>(1024);
  }

  // Record worker spawn time
  spawn_time_.Now();
}

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

  // Clean up cached stacks and RunContexts
  StackAndContext cached_entry;
  hshm::qtok_t token = stack_cache_.pop(cached_entry);
  while (!token.IsNull()) {
    // Free the cached stack
    if (cached_entry.stack_base_for_free) {
      free(cached_entry.stack_base_for_free);
    }

    // Free the cached RunContext
    if (cached_entry.run_ctx) {
      cached_entry.run_ctx->~RunContext();
      free(cached_entry.run_ctx);
    }

    // Get next cached entry
    token = stack_cache_.pop(cached_entry);
  }

  // Clean up all blocked queues (2 queues)
  for (u32 i = 0; i < NUM_BLOCKED_QUEUES; ++i) {
    RunContext *run_ctx;
    hshm::qtok_t queue_token = blocked_queues_[i].pop(run_ctx);
    while (!queue_token.IsNull()) {
      // RunContexts in blocked queues are still in use - don't free them
      // They will be cleaned up when the tasks complete or by stack cache
      queue_token = blocked_queues_[i].pop(run_ctx);
    }
  }

  // Clear assigned lane reference (don't delete - it's in shared memory)
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
      // Process up to 16 tasks from this worker's lane per iteration
      const u32 MAX_TASKS_PER_ITERATION = 16;
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
            // Allocate stack and RunContext before routing
            auto *pool_manager = CHI_POOL_MANAGER;
            Container *container =
                pool_manager->GetContainer(task_full_ptr->pool_id_);
            BeginTask(task_full_ptr, container, assigned_lane_);

            // Route task using consolidated routing function
            if (RouteTask(task_full_ptr, assigned_lane_, container)) {
              // Routing successful, execute the task
              RunContext *run_ctx = task_full_ptr->run_ctx_;
              ExecTask(task_full_ptr, run_ctx, false);
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

  // Initialize exec_mode to kExec by default
  RunContext *run_ctx = task_ptr->run_ctx_;
  if (run_ctx != nullptr) {
    run_ctx->exec_mode = ExecMode::kExec;
  }

  // Resolve pool query and route task to container
  // Note: ResolveDynamicQuery may override exec_mode to kDynamicSchedule
  std::vector<PoolQuery> pool_queries =
      ResolvePoolQuery(task_ptr->pool_query_, task_ptr->pool_id_, task_ptr);

  // Check if pool_queries is empty - this indicates an error in resolution
  if (pool_queries.empty()) {
    HELOG(kError,
          "Worker {}: Task routing failed - no pool queries resolved. "
          "Pool ID: {}, Method: {}",
          worker_id_, task_ptr->pool_id_, task_ptr->method_);

    // End the task with should_complete=false since RunContext is already
    // allocated
    RunContext *run_ctx = task_ptr->run_ctx_;
    EndTask(task_ptr, run_ctx, false, false);
    return false;
  }

  // Check if task should be processed locally
  if (IsTaskLocal(task_ptr, pool_queries)) {
    // Route task locally using container query and Monitor with kLocalSchedule
    return RouteLocal(task_ptr, lane, container);
  } else {
    // Route task globally using admin client's ClientSendTaskIn method
    // RouteGlobal never fails, so no need for fallback logic
    RouteGlobal(task_ptr, pool_queries);
    return false; // No local execution needed
  }
}

bool Worker::IsTaskLocal(const FullPtr<Task> &task_ptr,
                         const std::vector<PoolQuery> &pool_queries) {
  // If task has TASK_FORCE_NET flag, force it through network code
  if (task_ptr->task_flags_.Any(TASK_FORCE_NET)) {
    return false;
  }

  // If there's only one node, all tasks are local
  auto *ipc_manager = CHI_IPC;
  if (ipc_manager && ipc_manager->GetNumHosts() == 1) {
    return true;
  }

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

  case RoutingMode::Dynamic:
    // Dynamic mode routes to Monitor first, then may be resolved locally
    // Treat as local for initial routing to allow Monitor to process
    return true;

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

  // Task is local and should be executed directly
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

    // Send task using unified Send API with SerializeIn mode
    admin_client.AsyncSend(
        mctx,
        true,        // srl_mode = true (SerializeIn - sending inputs)
        task_ptr,    // Task pointer to send
        pool_queries // Pool queries vector for target nodes
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
                                                PoolId pool_id,
                                                const FullPtr<Task> &task_ptr) {
  // Basic validation
  if (pool_id.IsNull()) {
    return {}; // Invalid pool ID
  }

  RoutingMode routing_mode = query.GetRoutingMode();

  switch (routing_mode) {
  case RoutingMode::Local:
    return ResolveLocalQuery(query, task_ptr);
  case RoutingMode::Dynamic:
    return ResolveDynamicQuery(query, pool_id, task_ptr);
  case RoutingMode::DirectId:
    return ResolveDirectIdQuery(query, pool_id, task_ptr);
  case RoutingMode::DirectHash:
    return ResolveDirectHashQuery(query, pool_id, task_ptr);
  case RoutingMode::Range:
    return ResolveRangeQuery(query, pool_id, task_ptr);
  case RoutingMode::Broadcast:
    return ResolveBroadcastQuery(query, pool_id, task_ptr);
  case RoutingMode::Physical:
    return ResolvePhysicalQuery(query, pool_id, task_ptr);
  }

  return {};
}

std::vector<PoolQuery>
Worker::ResolveLocalQuery(const PoolQuery &query,
                          const FullPtr<Task> &task_ptr) {
  // Local routing - process on current node
  return {query};
}

std::vector<PoolQuery>
Worker::ResolveDynamicQuery(const PoolQuery &query, PoolId pool_id,
                            const FullPtr<Task> &task_ptr) {
  // Use the current RunContext that was allocated by BeginTask
  RunContext *run_ctx = task_ptr->run_ctx_;
  if (run_ctx == nullptr) {
    return {}; // Return empty vector if no RunContext
  }

  // Set execution mode to kDynamicSchedule
  // This tells ExecTask to call RerouteDynamicTask instead of EndTask
  run_ctx->exec_mode = ExecMode::kDynamicSchedule;

  // Return Local query for execution
  // After task completes, RerouteDynamicTask will re-route with updated
  // pool_query
  std::vector<PoolQuery> result;
  result.push_back(PoolQuery::Local());
  return result;
}

std::vector<PoolQuery>
Worker::ResolveDirectIdQuery(const PoolQuery &query, PoolId pool_id,
                             const FullPtr<Task> &task_ptr) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (pool_manager == nullptr) {
    return {query}; // Fallback to original query
  }

  // Get the container ID from the query
  ContainerId container_id = query.GetContainerId();

  // Get the physical node ID for this container
  u32 node_id = pool_manager->GetContainerNodeId(pool_id, container_id);

  // Create a Physical PoolQuery to that node
  return {PoolQuery::Physical(node_id)};
}

std::vector<PoolQuery>
Worker::ResolveDirectHashQuery(const PoolQuery &query, PoolId pool_id,
                               const FullPtr<Task> &task_ptr) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (pool_manager == nullptr) {
    return {query}; // Fallback to original query
  }

  // Get pool info to find the number of containers
  const PoolInfo *pool_info = pool_manager->GetPoolInfo(pool_id);
  if (pool_info == nullptr || pool_info->num_containers_ == 0) {
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

std::vector<PoolQuery>
Worker::ResolveRangeQuery(const PoolQuery &query, PoolId pool_id,
                          const FullPtr<Task> &task_ptr) {
  // Set execution mode to normal execution
  RunContext *run_ctx = task_ptr->run_ctx_;
  if (run_ctx != nullptr) {
    run_ctx->exec_mode = ExecMode::kExec;
  }

  auto *pool_manager = CHI_POOL_MANAGER;
  if (pool_manager == nullptr) {
    return {query}; // Fallback to original query
  }

  auto *config_manager = CHI_CONFIG_MANAGER;
  if (config_manager == nullptr) {
    return {query}; // Fallback to original query
  }

  u32 range_offset = query.GetRangeOffset();
  u32 range_count = query.GetRangeCount();

  // Validate range
  if (range_count == 0) {
    return {}; // Empty range
  }

  std::vector<PoolQuery> result_queries;

  // Get neighborhood size from configuration (maximum number of queries)
  u32 neighborhood_size = config_manager->GetNeighborhoodSize();

  // Calculate queries needed, capped at neighborhood_size
  u32 ideal_queries = (range_count + neighborhood_size - 1) / neighborhood_size;
  u32 queries_to_create = std::min(ideal_queries, neighborhood_size);

  // Create one query per container
  if (queries_to_create <= 1) {
    queries_to_create = range_count;
  }

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

std::vector<PoolQuery>
Worker::ResolveBroadcastQuery(const PoolQuery &query, PoolId pool_id,
                              const FullPtr<Task> &task_ptr) {
  auto *pool_manager = CHI_POOL_MANAGER;
  if (pool_manager == nullptr) {
    return {query}; // Fallback to original query
  }

  // Get pool info to find the total number of containers
  const PoolInfo *pool_info = pool_manager->GetPoolInfo(pool_id);
  if (pool_info == nullptr || pool_info->num_containers_ == 0) {
    return {query}; // Fallback to original query
  }

  // Create a Range query that covers all containers, then resolve it
  PoolQuery range_query = PoolQuery::Range(0, pool_info->num_containers_);
  return ResolveRangeQuery(range_query, pool_id, task_ptr);
}

std::vector<PoolQuery>
Worker::ResolvePhysicalQuery(const PoolQuery &query, PoolId pool_id,
                             const FullPtr<Task> &task_ptr) {
  // Physical routing - query is already resolved to a specific node
  return {query};
}

RunContext *Worker::AllocateStackAndContext(size_t size) {
  // Try to get from cache first
  StackAndContext cached_entry;
  hshm::qtok_t token = stack_cache_.pop(cached_entry);

  if (!token.IsNull() && cached_entry.run_ctx &&
      cached_entry.stack_base_for_free) {
    RunContext *run_ctx = cached_entry.run_ctx;
    run_ctx->Clear();
    return run_ctx;
  }

  // Normalize size to page-aligned
  const size_t page_size = 4096;
  size = ((size + page_size - 1) / page_size) * page_size;

  // Cache miss or size mismatch - allocate new stack and RunContext
  void *stack_base = nullptr;
  int ret = posix_memalign(&stack_base, page_size, size);
  RunContext *new_run_ctx = new RunContext();

  if (ret == 0 && stack_base && new_run_ctx) {
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

    return new_run_ctx;
  }

  // Cleanup on failure
  if (stack_base)
    free(stack_base);
  if (new_run_ctx)
    delete new_run_ctx;

  return nullptr;
}

void Worker::DeallocateStackAndContext(RunContext *run_ctx) {
  if (!run_ctx) {
    return;
  }

  // Add to cache for reuse instead of freeing
  // Create StackAndContext entry with the stack and RunContext
  StackAndContext cache_entry(run_ctx->stack_base_for_free, run_ctx->stack_size,
                              run_ctx);

  // Try to add to cache
  hshm::qtok_t token = stack_cache_.push(cache_entry);

  // If cache is full (null token), free the resources
  if (token.IsNull()) {
    HELOG(kError,
          "Worker {}: Failed to add RunContext to stack cache. Stack base for "
          "free: {}, Stack size: {}",
          worker_id_, run_ctx->stack_base_for_free, run_ctx->stack_size);
  }
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

  // Set current run context
  SetCurrentRunContext(run_ctx);
}

void Worker::BeginFiber(const FullPtr<Task> &task_ptr, RunContext *run_ctx,
                        void (*fiber_fn)(boost::context::detail::transfer_t)) {
  // Mark that work is being done
  did_work_ = true;

  // Set current run context
  SetCurrentRunContext(run_ctx);

  // New task execution - increment work count for non-periodic tasks
  if (run_ctx->container && !task_ptr->IsPeriodic()) {
    // Increment work remaining in the container for non-periodic tasks
    run_ctx->container->UpdateWork(task_ptr, *run_ctx, 1);
  }

  // Create fiber context for this task using provided fiber function
  // stack_ptr is already correctly positioned based on stack growth direction
  bctx::fcontext_t fiber_fctx =
      bctx::make_fcontext(run_ctx->stack_ptr, run_ctx->stack_size, fiber_fn);

  // Jump to fiber context to execute the task
  bctx::transfer_t fiber_result = bctx::jump_fcontext(fiber_fctx, nullptr);

  // Update yield_context with current worker context so task can return here
  // The fiber_result contains the worker context for the task to use
  run_ctx->yield_context = fiber_result;

  // Update resume_context only if the task actually yielded (is_blocked = true)
  if (run_ctx->is_blocked) {
    run_ctx->resume_context = fiber_result;
  }
}

void Worker::ResumeFiber(const FullPtr<Task> &task_ptr, RunContext *run_ctx) {
  // Mark that work is being done
  did_work_ = true;

  // Set current run context
  SetCurrentRunContext(run_ctx);

  // Resume execution - jump back to where the task yielded

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
  bctx::transfer_t resume_result =
      bctx::jump_fcontext(resume_fctx, resume_data);

  // Update yield_context with current worker context so task can return here
  run_ctx->yield_context = resume_result;

  // Update resume_context only if the task yielded again (is_blocked = true)
  if (run_ctx->is_blocked) {
    run_ctx->resume_context = resume_result;
  }
}

void Worker::ExecTask(const FullPtr<Task> &task_ptr, RunContext *run_ctx,
                      bool is_started) {
  if (task_ptr.IsNull() || !run_ctx) {
    return; // Consider null tasks as completed
  }

  // Call appropriate fiber function based on task state
  if (is_started) {
    ResumeFiber(task_ptr, run_ctx);
  } else {
    BeginFiber(task_ptr, run_ctx, FiberExecutionFunction);
    task_ptr->SetFlags(TASK_STARTED);
  }

  // Common cleanup logic for both fiber and direct execution
  if (run_ctx->is_blocked) {
    // Task is blocked - don't clean up, will be resumed later
    return; // Task is not completed, blocked for later resume
  }

  // Check if this is a dynamic scheduling task
  if (run_ctx->exec_mode == ExecMode::kDynamicSchedule) {
    // Dynamic scheduling - re-route task with updated pool_query
    RerouteDynamicTask(task_ptr, run_ctx);
    return;
  }

  // Handle task completion and rescheduling
  if (task_ptr->IsPeriodic()) {
    // Periodic tasks are always rescheduled regardless of execution success
    ReschedulePeriodicTask(run_ctx, task_ptr);
  } else {
    // Determine if task should be completed and cleaned up
    bool is_remote = task_ptr->IsRemote();

    // Non-periodic task completed - decrement work count
    if (run_ctx->container != nullptr) {
      // Decrement work remaining in the container for non-periodic tasks
      run_ctx->container->UpdateWork(task_ptr, *run_ctx, -1);
    }

    // End task execution and cleanup
    EndTask(task_ptr, run_ctx, true, is_remote);
  }
}

void Worker::EndTask(const FullPtr<Task> &task_ptr, RunContext *run_ctx,
                     bool should_complete, bool is_remote) {
  if (!should_complete) {
    return;
  }

  // Check if task is remote and needs to send outputs back
  if (is_remote) {
    // Get return node ID from pool_query
    chi::u32 ret_node_id = task_ptr->pool_query_.GetReturnNode();

    // Create pool query for return node
    std::vector<chi::PoolQuery> return_queries;
    return_queries.push_back(chi::PoolQuery::Physical(ret_node_id));

    // Create admin client to send task outputs back
    chimaera::admin::Client admin_client(kAdminPoolId);
    hipc::MemContext mctx;

    // Send task outputs using SerializeOut mode
    admin_client.AsyncSend(
        mctx,
        false,         // srl_mode = false (SerializeOut - sending outputs)
        task_ptr,      // Task pointer with results
        return_queries // Send back to return node
    );

    HILOG(kDebug, "Worker: Sent remote task outputs back to node {}",
          ret_node_id);
  } else {
    // Mark task as complete
    task_ptr->is_complete_.store(1);
  }

  // Deallocate stack and context
  DeallocateStackAndContext(run_ctx);
}

void Worker::RerouteDynamicTask(const FullPtr<Task> &task_ptr,
                                RunContext *run_ctx) {
  // Dynamic scheduling complete - now re-route task with updated pool_query
  // The task's pool_query_ should have been updated during execution
  // (e.g., from Dynamic to Local or Broadcast)

  Container *container = run_ctx->container;
  TaskLane *lane = run_ctx->lane;

  // Reset the TASK_STARTED flag so the task can be executed again
  task_ptr->ClearFlags(TASK_STARTED | TASK_ROUTED);

  // Re-route the task using the updated pool_query
  if (RouteTask(task_ptr, lane, container)) {
    // Avoids recursive call to RerouteDynamicTask
    if (run_ctx->exec_mode == ExecMode::kDynamicSchedule) {
      EndTask(task_ptr, run_ctx, true, false);
      return;
    }
    // Successfully re-routed - execute the task again
    // Note: ExecTask will call BeginFiber since TASK_STARTED is unset
    ExecTask(task_ptr, run_ctx, false);
  }
}

void Worker::ProcessBlockedQueue(hshm::ext_ring_buffer<RunContext *> &queue) {
  // Process only first 8 tasks in the queue
  size_t queue_size = queue.GetSize();
  size_t check_limit = std::min(queue_size, size_t(8));

  for (size_t i = 0; i < check_limit; i++) {
    RunContext *run_ctx;
    hshm::qtok_t token = queue.pop(run_ctx);

    if (token.IsNull()) {
      // Queue is empty
      break;
    }

    if (!run_ctx || run_ctx->task.IsNull()) {
      // Invalid entry, don't re-add
      continue;
    }

    // Check if enough time has passed for this task to wake up
    hshm::Timepoint current_time;
    current_time.Now();

    // Calculate elapsed time since blocking started
    double elapsed_us = run_ctx->block_start.GetUsecFromStart(current_time);

    if (elapsed_us < run_ctx->block_time_us) {
      // Not enough time has passed - re-add to queue without incrementing
      // block_count We decrement block_count before calling AddToBlockedQueue
      // since it will increment it
      run_ctx->block_count_--;
      AddToBlockedQueue(run_ctx);
      continue;
    }

    // Enough time has passed - check if all subtasks are completed
    if (run_ctx->AreSubtasksCompleted()) {
      // All subtasks completed - resume task immediately
      run_ctx->is_blocked = false;

      // Reset block count since task is now ready to proceed
      run_ctx->block_count_ = 0;

      // Determine if this is a resume (task was started before) or first
      // execution
      bool is_started = run_ctx->task->task_flags_.Any(TASK_STARTED);

      // Execute task with existing RunContext
      ExecTask(run_ctx->task, run_ctx, is_started);

      // Don't re-add to queue
      continue;
    }

    // Subtasks not completed - re-add to blocked queue
    // Block count will be incremented automatically
    AddToBlockedQueue(run_ctx);
  }
}

u32 Worker::ContinueBlockedTasks() {
  u32 return_value = 0; // Default return value for immediate recheck

  // Process queues based on block_time_us:
  // Queue 0: Short blocking times (< 10us) - check every iteration
  ProcessBlockedQueue(blocked_queues_[0]);

  // Queue 1: Long blocking times (>= 10us) - check based on elapsed time
  // Calculate current time in 10us units since worker spawn
  hshm::Timepoint current_time;
  current_time.Now();
  double elapsed_us = spawn_time_.GetUsecFromStart(current_time);
  u64 current_time_10us = static_cast<u64>(elapsed_us / 10.0);

  // Process long queue if enough time has passed since last check
  if (current_time_10us > last_long_queue_check_) {
    ProcessBlockedQueue(blocked_queues_[1]);
    last_long_queue_check_ = current_time_10us;
  }

  return return_value;
}

void Worker::AddToBlockedQueue(RunContext *run_ctx) {
  if (!run_ctx || run_ctx->task.IsNull()) {
    return;
  }

  // Capture current real time when blocking
  run_ctx->block_start.Now();

  // Increment block count
  run_ctx->block_count_++;

  // Determine queue index based on block_time_us:
  // Queue 0: Short blocking times (< 10us) - checked every iteration
  // Queue 1: Long blocking times (>= 10us) - checked every 10us of elapsed time
  constexpr double BLOCK_TIME_THRESHOLD_US = 10.0;
  u32 queue_idx = (run_ctx->block_time_us < BLOCK_TIME_THRESHOLD_US) ? 0 : 1;

  // Add to the appropriate queue
  blocked_queues_[queue_idx].push(run_ctx);
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

  // Unset TASK_STARTED flag when rescheduling periodic task
  task_ptr->ClearFlags(TASK_STARTED);

  // Add to blocked queue - block count will be incremented automatically
  AddToBlockedQueue(run_ctx);
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

    // Task completion and work count handling is done in ExecTask
    // This avoids duplicate logic and ensures proper ordering
  }

  // Jump back to worker context when task completes
  // Use temporary variables to avoid potential read/write conflicts
  bctx::fcontext_t worker_fctx = run_ctx->yield_context.fctx;
  void *worker_data = run_ctx->yield_context.data;
  bctx::jump_fcontext(worker_fctx, worker_data);
}

} // namespace chi