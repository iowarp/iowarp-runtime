## Code Style

Use the Google C++ style guide for C++.

You should store the pointer returned by the singleton GetInstance method. Avoid dereferencing GetInstance method directly using either -> or *. E.g., do not do ``hshm::Singleton<T>::GetInstance()->var_``. You should do ``auto *x = hshm::Singleton<T>::GetInstance(); x->var_;``.


NEVER use a null pool query. If you don't know, always use local.

## Boost Context Stack Management

The worker fiber context system requires properly aligned stacks for boost::context::detail operations. Key requirements:

- Use `posix_memalign` for page-aligned stack allocation (not simple `malloc`)
- Ensure 16-byte alignment for x86-64 ABI compatibility 
- Zero-initialize stack memory for consistent behavior
- Proper context field separation:
  - `fiber_context`: Initial fiber entry point (created once with `make_fcontext`)
  - `yield_context`: Worker context from FiberExecutionFunction parameter - used by task to yield back to worker
  - `resume_context`: Task's yield point context - used by worker to resume the task

Stack corruption from misaligned memory causes task->Wait() failures and fiber resume issues.

### Context Jump Safety

When resuming tasks with `jump_fcontext()`, avoid read/write conflicts on the same `fiber_transfer` field:

**Problem**: 
```cpp
// UNSAFE - reading and writing same field simultaneously
run_ctx->fiber_transfer = bctx::jump_fcontext(
    run_ctx->fiber_transfer.fctx, run_ctx->fiber_transfer.data);
```

**Solution**:
```cpp
// SAFE - use separate context fields and temporary variables
bctx::fcontext_t resume_fctx = run_ctx->resume_context.fctx;
void* resume_data = run_ctx->resume_context.data;
run_ctx->resume_context = bctx::jump_fcontext(resume_fctx, resume_data);
```

**Context Flow**:
1. **FiberExecutionFunction**: Store parameter `t` as `yield_context`
2. **Task YieldBase()**: Jump to `yield_context`, store result in `resume_context`
3. **Worker ExecTask()**: Jump to `resume_context` to resume task

This prevents segfaults when resuming tasks after `Wait()` operations.

### Task Completion Race Condition

**Critical**: Never set `task_ptr->is_complete.store(1)` until ALL cleanup is finished. Setting it too early creates a race condition where client code can immediately free the task memory with `DelTask()`, causing segfaults on subsequent `task_ptr` access.

**Correct order**:
1. Perform all task cleanup (deallocate stack, clear RunContext, etc.)
2. Set `task_ptr->is_complete.store(1)` LAST
3. Never access `task_ptr` after setting `is_complete`

**Only mark non-periodic tasks as complete** - periodic tasks are rescheduled and should not be marked complete.

## ChiMod Client Requirements

### CreateTask Pool Assignment
CreateTask operations in all ChiMod clients MUST use `chi::kAdminPoolId` instead of the client's `pool_id_`. This is because CreateTask is actually a GetOrCreatePoolTask that must be processed by the admin ChiMod to create or find the target pool.

**Correct Usage:**
```cpp
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,  // Always use admin pool for CreateTask
    pool_query,
    // ... other parameters
);
```

**Incorrect Usage:**
```cpp
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    pool_id_,  // WRONG - this bypasses admin pool processing
    pool_query,
    // ... other parameters
);
```

This applies to all ChiMod clients including bdev, MOD_NAME, and any future ChiMods.

## Workflow
Use the incremental logic builder agent when making code changes.

Use the compiler subagent for making changes to cmakes and identifying places that need to be fixed in the code.

Always verify that code continue to compiles after making changes. Avoid commenting out code to fix compilation issues.

Whenever building unit tests, make sure to use the unit testing agent.

Whenever performing filesystem queries or executing programs, use the filesystem ops script agent.

NEVER DO MOCK CODE OR STUB CODE UNLESS SPECIFICALLY STATED OTHERWISE. ALWAYS IMPLEMENT REAL, WORKING CODE.
