## Code Style

Use the Google C++ style guide for C++.

### Worker Method Return Types

The following Worker methods return `void`, not `bool`:
- `ExecTask()` - Execute task with context switching capability
- `EndTask()` - End task execution and perform cleanup
- `RerouteDynamicTask()` - End dynamic scheduling task and re-route with updated pool query

These methods handle task execution flow internally and do not return success/failure status.

### Type Aliases

Use the `WorkQueue` typedef for worker queue types:
```cpp
using WorkQueue = chi::ipc::mpsc_queue<hipc::TypedPointer<TaskLane>>;
```

This simplifies code readability and maintenance for worker queue operations.

**TaskLane Typedef:**
The `TaskLane` typedef is defined globally in the `chi` namespace:
```cpp
using TaskLane = chi::ipc::multi_mpsc_queue<hipc::TypedPointer<Task>, TaskQueueHeader>::queue_t;
```

Use `TaskLane*` for all lane pointers in RunContext and other interfaces. Avoid `void*` and explicit type casts.

You should store the pointer returned by the singleton GetInstance method. Avoid dereferencing GetInstance method directly using either -> or *. E.g., do not do ``hshm::Singleton<T>::GetInstance()->var_``. You should do ``auto *x = hshm::Singleton<T>::GetInstance(); x->var_;``.


Whenever you build a new function, always create a docstring for it. It should document what the parameters mean and the point of the function. It should be something easily parsed by doxygen.

NEVER use a null pool query. If you don't know, always use local.

Local QueueId should be named. NEVER use raw integers. This is the same for priorities. Please name them semantically.

## ChiMod Client Requirements

### PoolQuery Recommendations for Create Operations

**RECOMMENDED**: Use `PoolQuery::Dynamic()` for all Create operations to leverage automatic caching optimization.

**Dynamic Pool Query Behavior:**
- Routes to the Monitor method with `MonitorModeId::kGlobalSchedule`
- Monitor performs a two-step process:
  1. Check if pool exists locally using PoolManager
  2. If pool exists: change pool_query to Local (task executes locally using existing pool)
  3. If pool doesn't exist: change pool_query to Broadcast (task creates pool on all nodes)
- This optimization avoids unnecessary network overhead when pools already exist locally

**Correct Usage:**
```cpp
// Recommended: Use Dynamic() for automatic caching
admin_client.Create(mctx, chi::PoolQuery::Dynamic(), "admin");
bdev_client.Create(mctx, chi::PoolQuery::Dynamic(), file_path, chimaera::bdev::BdevType::kFile);
```

**Alternative Usage:**
```cpp
// Explicit routing - use when you specifically need Local or Broadcast behavior
admin_client.Create(mctx, chi::PoolQuery::Local(), "admin");  // Force local creation
bdev_client.Create(mctx, chi::PoolQuery::Broadcast(), pool_name, bdev_type);  // Force broadcast
```

**Why Dynamic() is Recommended:**
- **Performance**: Avoids redundant pool creation attempts when pool already exists
- **Network Efficiency**: Eliminates unnecessary broadcast messages for existing pools
- **Automatic**: No manual cache checking required in client code
- **Safe**: Falls back to broadcast creation when pool doesn't exist locally

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

### ChiMod Name Parameter
ChiMod clients MUST use `CreateParams::chimod_lib_name` instead of hardcoded module names in CreateTask operations.

**Correct Usage:**
```cpp
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,  // Always use admin pool for CreateTask
    pool_query,
    CreateParams::chimod_lib_name,  // Use static member from CreateParams
    pool_name,
    pool_id,
    // ... other parameters
);
```

**Incorrect Usage:**
```cpp
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,
    pool_query,
    "chimaera_modulename_runtime",  // WRONG - hardcoded string
    pool_name,
    pool_id,
    // ... other parameters
);
```

This requirement ensures namespace flexibility and maintains a single source of truth for module names.

### Pool Name Requirements
All ChiMod Create functions MUST require a user-provided `pool_name` parameter. Never auto-generate pool names using `pool_id_` during Create operations, as `pool_id_` is not set until after Create completes.

**Admin Pool Name Requirement:**
The admin pool name MUST always be "admin". Multiple admin pools are NOT supported.

**Correct Admin Usage:**
```cpp
// Admin container - MUST use "admin" as pool name
admin_client.Create(mctx, pool_query, "admin");
```

**Incorrect Admin Usage:**
```cpp
// WRONG - Any other pool name for admin
admin_client.Create(mctx, pool_query, "pool_1234");
admin_client.Create(mctx, pool_query, "my_admin_container");
```

**Pool Name Guidelines:**
- Use descriptive, unique names that identify the purpose or content
- For file-based devices (like BDev), the `pool_name` serves as the file path
- For RAM-based devices, the `pool_name` should be a unique identifier
- Consider using timestamp + PID combinations for uniqueness when needed

**Correct Usage:**
```cpp
// BDev file-based device
std::string file_path = "/path/to/my/device.dat";
bdev_client.Create(mctx, pool_query, file_path, chimaera::bdev::BdevType::kFile);

// BDev RAM-based device  
std::string pool_name = "my_ram_device_" + std::to_string(timestamp);
bdev_client.Create(mctx, pool_query, pool_name, chimaera::bdev::BdevType::kRam, ram_size);

// MOD_NAME container
std::string pool_name = "my_modname_container";
mod_name_client.Create(mctx, pool_query, pool_name);

// Admin container - MUST always use "admin"
admin_client.Create(mctx, pool_query, "admin");
```

**Incorrect Usage:**
```cpp
// WRONG - Using pool_id_ before it's set
std::string pool_name = "pool_" + std::to_string(pool_id_.ToU64());

// WRONG - Using empty string for pool names
bdev_client.Create(mctx, pool_query, "", chimaera::bdev::BdevType::kRam);

// WRONG - Auto-generating instead of requiring user input
// Create functions should not auto-generate names internally
```

**BDev Interface Requirements:**
- Use single `Create()` and `AsyncCreate()` methods (not multiple overloads)  
- For file-based BDev: `pool_name` parameter IS the file path
- For RAM-based BDev: `pool_name` parameter serves as unique identifier
- Signature: `Create(mctx, pool_query, pool_name, bdev_type, total_size, io_depth, alignment)`
- No separate `file_path` parameter - the `pool_name` serves dual purpose

## ChiMod Linking Requirements

### Target Naming and Aliases
ChiMod libraries use consistent underscore-based naming:

**Target Names:**
- Runtime: `${NAMESPACE}_${CHIMOD_NAME}_runtime` (e.g., `chimaera_admin_runtime`)
- Client: `${NAMESPACE}_${CHIMOD_NAME}_client` (e.g., `chimaera_admin_client`)

**CMake Aliases:**
- Runtime: `${NAMESPACE}::${CHIMOD_NAME}_runtime` (e.g., `chimaera::admin_runtime`)
- Client: `${NAMESPACE}::${CHIMOD_NAME}_client` (e.g., `chimaera::admin_client`)

**Package Names:**
- Format: `${NAMESPACE}_${CHIMOD_NAME}` (e.g., `chimaera_admin`)
- Used with `find_package(chimaera_admin REQUIRED)`
- Core package: `chimaera` (provides `chimaera::cxx`)

**Namespace Discovery:**
The namespace is automatically read from the `chimaera_repo.yaml` file in the ChiMod repository root.

**Complete Example:**
For the chimaera namespace with admin module:
```yaml
# chimaera_repo.yaml
namespace: chimaera
```

This creates:
- **Package**: `chimaera_admin` (for `find_package`)
- **Targets**: `chimaera_admin_client`, `chimaera_admin_runtime` 
- **Aliases**: `chimaera::admin_client`, `chimaera::admin_runtime` (**use these**)

**Standard ChiMod Examples:**
- **Admin**: `chimaera::admin_client`, `chimaera::admin_runtime`
- **BDev**: `chimaera::bdev_client`, `chimaera::bdev_runtime`
- **MOD_NAME**: `chimaera::MOD_NAME_client`, `chimaera::MOD_NAME_runtime`

### Automatic Dependency Linking
ChiMod libraries automatically handle common dependencies:

**Automatic Dependencies for Runtime Code:**
- `rt` library: Automatically linked to all ChiMod runtime targets for POSIX real-time library support (async I/O)
- Admin ChiMod: Automatically linked to all non-admin ChiMod runtime and client targets
- Admin includes: Automatically added to include directories for non-admin ChiMods

**For External Applications:**
When linking against installed ChiMod libraries, use the underscore-based target pattern:
```cmake
target_link_libraries(your_target
  chimaera::mod_name_runtime    # Specific ChiMod runtime library
  chimaera::mod_name_client     # Specific ChiMod client library 
)
# rt, admin, and chimaera::cxx dependencies are automatically included by ChiMod libraries
# All target names use underscores consistently
```

### ChiMod Creation and Installation
ChiMod libraries are created using ChimaeraCommon.cmake utilities with separate client and runtime targets.

**Automatic Dependencies:**
ChiMod libraries automatically handle common dependencies:

- **Runtime Libraries**: Automatically links `rt` (POSIX real-time library) to all runtime targets for async I/O operations
- **Admin ChiMod Integration**: For non-admin chimods, automatically links both `chimaera_admin_runtime` and `chimaera_admin_client` and includes admin headers
- **Client Dependencies**: For non-admin chimods, client libraries automatically link `chimaera_admin_client` and include admin headers

This eliminates the need for manual dependency configuration in individual ChiMod CMakeLists.txt files.

**ChiMod Installation:**
Installation is automatic - no separate `install_chimod()` call required. The `add_chimod_client()` and `add_chimod_runtime()` functions automatically handle:
- Target installation with proper export sets
- Header installation to `include/[namespace]/[module_name]/`
- Package configuration file generation (`[namespace]_[module]Config.cmake`)
- CMake export file creation for external projects
- Runtime libraries automatically link to client libraries when both exist

### Include Directory Requirements
When using ChiMods, include the necessary headers:

```cmake
target_include_directories(your_target PUBLIC
  ${CMAKE_SOURCE_DIR}/chimods/admin/include     # Admin module headers (always required)
  ${CMAKE_SOURCE_DIR}/chimods/your_chimod/include  # Your ChiMod headers
)
```

**Note**: All ChiMod headers are organized under the namespace in include directories:
- Structure: `[chimod_directory]/include/[namespace]/[module_name]/`
- Example: Admin headers are in `admin/include/chimaera/admin/` (where `chimaera` is the namespace from `chimaera_repo.yaml`)
- Headers include: `[module_name]_client.h`, `[module_name]_runtime.h`, `[module_name]_tasks.h`
- Auto-generated headers are in the `autogen/` subdirectory
- **Note**: The namespace is read from `chimaera_repo.yaml` and the chimod directory name doesn't need to match the namespace

### Compile Definitions
For runtime code, define `CHIMAERA_RUNTIME=1`:

```cmake
target_compile_definitions(your_target PRIVATE CHIMAERA_RUNTIME=1)
```

### Runtime Object Dependencies
The Chimaera runtime has several core objects that must be properly linked:

**Core runtime libraries required:**
- `chimaera`: Main Chimaera library containing core runtime objects
- `chimaera_admin_runtime`: Admin module runtime (required for all runtime applications)
- `chimaera_admin_client`: Admin module client (required for all runtime applications)

**Runtime executable linking pattern:**
```cmake
target_link_libraries(chimaera_start_runtime
  cxx                    # Core runtime objects and initialization
  hshm::cxx              # HermesShm shared memory framework
  ${CMAKE_THREAD_LIBS_INIT}   # Threading support for runtime
)
```

**Client executable linking pattern (no CHIMAERA_RUNTIME definition):**
```cmake
target_link_libraries(chimaera_stop_runtime
  cxx                    # Client-side objects only
  hshm::cxx              # HermesShm libraries
)
```

**Key runtime objects accessed via singletons:**
- `CHI_CHIMAERA_MANAGER`: Main runtime manager
- `CHI_WORK_ORCHESTRATOR`: Task scheduling and execution
- `CHI_IPC_MANAGER`: Inter-process communication
- `CHI_POOL_MANAGER`: Pool management

**Runtime initialization pattern:**
```cpp
// Runtime initialization (server-side)
bool CHIMAERA_RUNTIME_INIT() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  return chimaera_manager->ServerInit();
}

// Client initialization
bool CHIMAERA_CLIENT_INIT() {
  auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
  return chimaera_manager->ClientInit();
}
```

### External Application Linking
External applications can link to installed ChiMod libraries using the dynamic CMake export system:

**Installation requirement:**
```bash
cmake --preset debug
cmake --build build
cmake --install build --prefix /usr/local
```

**Dynamic module-based approach:**
Each ChiMod is automatically installed as a separate CMake package:

**External CMakeLists.txt pattern:**
```cmake
# Find individual module packages
find_package(chimaera_MOD_NAME REQUIRED)   # Example custom ChiMod
find_package(chimaera_admin REQUIRED)      # Admin ChiMod (often needed)
find_package(chimaera REQUIRED)            # Core Chimaera library

target_link_libraries(your_external_app
  chimaera::MOD_NAME_client     # ChiMod client library
  chimaera::admin_client        # Admin client (required for most ChiMod clients)
)

# Client mode is determined at runtime using IsClient() method
```

**External application usage:**
```cpp
#include <chimaera/chimaera.h>
#include <chimaera/MOD_NAME/MOD_NAME_client.h>
#include <chimaera/admin/admin_client.h>

int main() {
  // Initialize Chimaera client
  chi::CHIMAERA_CLIENT_INIT();
  
  // Create ChiMod client with pool ID
  const chi::PoolId pool_id = chi::PoolId(7000, 0);
  chimaera::MOD_NAME::Client mod_client(pool_id);
  
  // Create container
  auto pool_query = chi::PoolQuery::Local();
  mod_client.Create(HSHM_MCTX, pool_query, "my_pool_name");
}
```

**Header Include Structure:**
All ChiMod headers follow the consistent pattern: `#include <[namespace]/[module_name]/[module_name]_[type].h>`
- Admin client: `#include <chimaera/admin/admin_client.h>` (where `chimaera` is the namespace)
- Custom ChiMod client: `#include <chimaera/MOD_NAME/MOD_NAME_client.h>`
- Runtime headers: `#include <[namespace]/[module_name]/[module_name]_runtime.h>`
- Task headers: `#include <[namespace]/[module_name]/[module_name]_tasks.h>`

**Dependency requirements:**
External applications must have access to all dependencies:
- HermesShm (with its MPI, Boost, and other dependencies)
- cereal
- Boost (fiber, context components)
- Set `CMAKE_PREFIX_PATH` to include installation prefixes of all dependencies

**Dynamic export system details:**
- **Package names**: `chimaera_<module>` (e.g., `chimaera_admin`, `chimaera_bdev`)
- **Core package**: `chimaera` (provides `chimaera::cxx` target, automatically included by ChiMod libraries)
- **Target names**: `chimaera::<module>_client`, `chimaera::<module>_runtime`
- **Config files**: `chimaera_<module>Config.cmake` and `chimaera_<module>ConfigVersion.cmake`
- **Installation paths**: `lib/cmake/chimaera_<module>/`
- **Example targets**: `chimaera::admin_client`, `chimaera::admin_runtime`, `chimaera::bdev_client`

**ChiMod Repository Structure:**
- **ChiMod repo**: The directory containing `chimaera_repo.yaml` (e.g., `chimods/` in this project)
- **ChiMod repo location**: Can be the project root directory OR a subdirectory
- **Example in this project**: The chimod repo is `chimods/` containing `chimods/chimaera_repo.yaml`
- **External projects**: The chimod repo can be the entire repository root with `chimaera_repo.yaml` at the top level

**Example external build:**
```bash
# Set environment for dependencies
export CMAKE_PREFIX_PATH="/usr/local:/path/to/hermes-shm:/path/to/other/deps"

# Configure and build external project
mkdir build && cd build
cmake ..
make
```

**For comprehensive external ChiMod development guidance, see:**
`doc/MODULE_DEVELOPMENT_GUIDE.md` - Section "External ChiMod Development"

This section provides complete step-by-step instructions for:
- Setting up external ChiMod repositories
- Repository structure and configuration (`chimaera_repo.yaml` placement)
- CMake package discovery (`find_package` usage)
- Custom namespace configuration
- Build system integration and troubleshooting

**Key Point**: The `chimaera_repo.yaml` file defines the chimod repo boundary and must be placed in the directory that contains your chimods. This can be either the project root or a subdirectory like `chimods/`.

## BDev ChiMod Requirements

### Unified Pool Name Interface
BDev ChiMods use a simplified interface where the `pool_name` parameter serves dual purposes:
- **For file-based BDev**: The `pool_name` IS the file path
- **For RAM-based BDev**: The `pool_name` is a unique identifier

**Correct Usage:**
```cpp
// File-based BDev - pool_name is the file path
std::string file_path = "/path/to/my/device.dat";
bdev_client.Create(mctx, pool_query, file_path, chimaera::bdev::BdevType::kFile, 
                   total_size, io_depth, alignment);

// RAM-based BDev - pool_name is unique identifier  
std::string pool_name = "my_ram_device_" + std::to_string(timestamp);
bdev_client.Create(mctx, pool_query, pool_name, chimaera::bdev::BdevType::kRam,
                   total_size, io_depth, alignment);
```

**Key Benefits:**
- **Simplified interface**: No separate `file_path` parameter needed
- **Unified approach**: Single `pool_name` parameter serves both purposes
- **Direct correspondence**: Pool name directly maps to file path for file-based operations
- **Clear semantics**: Pool identification and file access use the same parameter

**CreateParams Structure:**
The BDev CreateParams no longer includes a separate `file_path_` field. The runtime extracts the pool name from the task and uses it directly as the file path when needed:

```cpp
// Runtime implementation uses pool_name directly
std::string pool_name = task->pool_name_.str();
file_fd_ = open(pool_name.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
```

This ensures that the pool name directly corresponds to the file being accessed, making pool identification and management more intuitive.

## Workflow
Use the incremental logic builder agent when making code changes.

Use the compiler subagent for making changes to cmakes and identifying places that need to be fixed in the code.

Always verify that code continue to compiles after making changes. Avoid commenting out code to fix compilation issues.

Whenever building unit tests, make sure to use the unit testing agent.

Whenever performing filesystem queries or executing programs, use the filesystem ops script agent.

NEVER DO MOCK CODE OR STUB CODE UNLESS SPECIFICALLY STATED OTHERWISE. ALWAYS IMPLEMENT REAL, WORKING CODE.

## ChiMod Runtime Code Standards

### Autogenerated Code Duplication
Runtime code (`*_runtime.cc` files) should **NEVER** duplicate autogenerated code methods. The following methods are automatically generated and must not be manually implemented in runtime source files:

**Prohibited duplicate implementations:**
- `SaveIn()` - Serialization from input parameters
- `LoadIn()` - Deserialization to input parameters
- `SaveOut()` - Serialization from output parameters
- `LoadOut()` - Deserialization to output parameters
- `NewCopy()` - Task copy constructor methods (container dispatcher, not task method)
- `Aggregate()` - Task aggregation dispatcher (container dispatcher, not task method)

**Rationale:**
These methods are automatically generated in `autogen/` files based on task definitions. Manual duplication in runtime files creates conflicts, compilation errors, and maintenance issues.

**Correct approach:**
- Let the code generation system handle all serialization and dispatcher methods
- Focus runtime implementation on core task execution logic only
- Implement optional `Copy()` and `Aggregate()` methods in task definitions (not runtime files)
- Use the generated methods through the normal task execution pipeline

### Task Copy and Aggregate Methods

Tasks can optionally implement `Copy()` and `Aggregate()` methods for distributed execution:

**Copy Method** (optional in task definitions):
```cpp
/**
 * Copy from another task (used for creating replicas)
 * @param other Source task to copy from
 */
void Copy(const hipc::FullPtr<YourTask> &other) {
  // Copy task-specific fields only
  // Base Task fields are copied automatically by NewCopy
  field1_ = other->field1_;
  field2_ = other->field2_;
}
```

**Aggregate Method** (optional in task definitions):
```cpp
/**
 * Aggregate results from a replica task
 * @param other Replica task with results to merge
 */
void Aggregate(const hipc::FullPtr<YourTask> &other) {
  // Common patterns:
  // 1. Last-writer-wins: Copy(other);
  // 2. Accumulation: result_ += other->result_;
  // 3. List merging: list_.insert(list_.end(), other->list_.begin(), other->list_.end());
}
```

**Base Task::Aggregate() Method:**
The base `Task` class provides a default `Aggregate()` implementation that propagates non-zero return codes from replica tasks to the origin task:

```cpp
void Task::Aggregate(const hipc::FullPtr<Task> &replica_task) {
  // If replica task has non-zero return code, propagate it to this task
  if (!replica_task.IsNull() && replica_task->GetReturnCode() != 0) {
    SetReturnCode(replica_task->GetReturnCode());
  }
}
```

**Automatic Return Code Propagation:**
The autogenerated dispatcher methods in `*_lib_exec.cc` always call the base `Task::Aggregate()` method **first** before calling any task-specific `Aggregate()` implementation. This ensures that return codes from replica tasks are always propagated to the origin task, even if tasks don't implement their own aggregation logic:

```cpp
// Generated code in *_lib_exec.cc
case Method::kSomeMethod: {
  auto typed_origin = origin_task.Cast<SomeTask>();
  auto typed_replica = replica_task.Cast<SomeTask>();
  // Call base Task aggregate to propagate return codes
  origin_task->Aggregate(replica_task);
  // Use SFINAE-based macro to call task-specific Aggregate if available, otherwise Copy
  CHI_AGGREGATE_OR_COPY(typed_origin, typed_replica);
  break;
}
```

**When to implement:**
- Implement `Copy()` for tasks that will be sent to remote nodes
- Implement `Aggregate()` for tasks that need to combine results from multiple replicas beyond just return code propagation
- Skip both for local-only tasks or tasks with no output parameters
- Return code propagation happens automatically via the base `Task::Aggregate()` method

**Note:** The autogenerated dispatcher methods in `*_lib_exec.cc` call the base `Task::Aggregate()` first, then call task-level `Aggregate()` methods (via CHI_AGGREGATE_OR_COPY macro) automatically

# Locking and Synchronization

## CoMutex and CoRwLock

The chimaera runtime provides two simplified coroutine-aware synchronization primitives for runtime code:

### CoMutex (Coroutine Mutex)
- **Header**: `chimaera/comutex.h`
- **Purpose**: Simplified mutex that uses Yield for blocking
- **Implementation**:
  - Uses a single `std::atomic<bool>` for lock state
  - Tasks that cannot acquire the lock call `Yield()` to be placed in the blocked queue
  - Tasks are retried automatically by the blocked queue mechanism
  - No complex data structures (no vectors, maps, or lists)
- **API**:
  - `Lock()`: Acquire mutex (yields if locked)
  - `Unlock()`: Release mutex
  - `TryLock()`: Non-blocking acquire attempt
  - `ScopedCoMutex`: RAII-style scoped lock

### CoRwLock (Coroutine Reader-Writer Lock)
- **Header**: `chimaera/corwlock.h`
- **Purpose**: Simplified reader-writer lock that uses Yield for blocking
- **Implementation**:
  - Uses `std::atomic<int>` for reader count and `std::atomic<bool>` for writer state
  - Supports multiple concurrent readers or a single writer
  - Tasks that cannot acquire the lock call `Yield()` to be placed in the blocked queue
  - Tasks are retried automatically by the blocked queue mechanism
  - No complex data structures (no vectors, maps, or lists)
- **API**:
  - `ReadLock()`: Acquire read lock (yields if writer active)
  - `ReadUnlock()`: Release read lock
  - `WriteLock()`: Acquire write lock (yields if readers/writers active)
  - `WriteUnlock()`: Release write lock
  - `TryReadLock()`: Non-blocking read lock attempt
  - `TryWriteLock()`: Non-blocking write lock attempt
  - `ScopedCoRwReadLock`: RAII-style scoped read lock
  - `ScopedCoRwWriteLock`: RAII-style scoped write lock

### Blocked Queue Processing
When a task calls `Yield()` due to lock contention:
1. Task is placed in the worker's blocked queue
2. `Worker::ContinueBlockedTasks()` periodically retries blocked tasks
3. When the lock becomes available, the task's retry succeeds and it proceeds

This approach eliminates the need for complex lock queues and explicit unblocking mechanisms.

### Usage Example
```cpp
#include "chimaera/comutex.h"
#include "chimaera/corwlock.h"

// CoMutex usage
chi::CoMutex mutex;
{
  chi::ScopedCoMutex lock(mutex);
  // Critical section - other tasks yield if they try to acquire
}

// CoRwLock usage
chi::CoRwLock rwlock;
{
  chi::ScopedCoRwReadLock read_lock(rwlock);
  // Multiple readers can proceed, writers yield
}
{
  chi::ScopedCoRwWriteLock write_lock(rwlock);
  // Exclusive writer access, readers and writers yield
}
```

**Note**: These locks are only for runtime code and have no client-side equivalents.

## Task Wait Functionality

### Critical Fix for Infinite Loops
The task `Wait()` function has been fixed to prevent infinite loops in the blocked task system. When a task calls `Wait()`, it automatically adds itself to the current task's `waiting_for_tasks` list in `RunContext`.

### How It Works
When `task->Wait()` is called:
1. The task adds itself to the current `RunContext::waiting_for_tasks` vector
2. This ensures `AreSubtasksCompleted()` properly tracks the task completion
3. The worker's blocked queue system can correctly determine when to resume blocked tasks
4. Prevents infinite loops where tasks continuously get added to `AddToBlockedQueue` but `AreSubtasksCompleted()` always returns `true`

### Implementation
The fix is implemented in the main `Wait()` function in `task.cc`:
```cpp
// Add this task to the current task's waiting_for_tasks list
// This ensures AreSubtasksCompleted() properly tracks this subtask
// Skip if called from yield to avoid double tracking
if (!from_yield) {
  auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
  hipc::FullPtr<Task> this_task_ptr(alloc, this);
  run_ctx->waiting_for_tasks.push_back(this_task_ptr);
}
```

### Usage
Call `Wait()` on any task - the tracking is automatic:
```cpp
task->Wait();  // Automatically tracked in parent task's waiting list
task->Wait(false);  // Same as above - explicitly tracked
task->Wait(true);   // Called from yield - not tracked to avoid double tracking
```

The `from_yield` parameter defaults to `false`. When set to `true`, it prevents adding subtasks to the RunContext, which is used by the `Yield()` function to avoid duplicate tracking.

This change ensures that the worker's `ContinueBlockedTasks()` function properly detects when subtasks are completed and can resume blocked parent tasks without infinite loops.

# ChiMod Development

When creating or modifying ChiMods (Chimaera modules), refer to the comprehensive module development guide:

**📖 See [doc/MODULE_DEVELOPMENT_GUIDE.md](doc/MODULE_DEVELOPMENT_GUIDE.md) for complete ChiMod development documentation**

This guide covers:
- Module structure and architecture
- Task development patterns
- Client and runtime implementation
- Build system integration
- Configuration and code generation
- Synchronization primitives
- **Execution modes and dynamic scheduling** (ExecMode in RunContext)
- External ChiMod development
- Best practices and common pitfalls

## Build Configuration

- Always use the debug CMakePreset when compiling code in this repo.
- All compilation warnings have been resolved as of the current state

## Code Quality Standards

### Compilation Standards
- All code must compile without warnings or errors
- Use appropriate variable types to avoid sign comparison warnings (e.g., `size_t` for container sizes)
- Mark unused variables with `(void)variable_name;` to suppress warnings when the variable is intentionally unused
- Follow strict type safety to prevent implicit conversions that generate warnings

### Thread Safety Standards

#### Atomic Task Fields
Critical task fields that may be accessed from multiple threads should use atomic types for thread safety:

**Task Return Code:**
The `return_code_` field in the Task structure is implemented as `std::atomic<u32>` to ensure thread-safe access:

```cpp
std::atomic<u32> return_code_; /**< Task return code (0=success, non-zero=error) */
```

**Usage:**
- **Reading**: Use `task->GetReturnCode()` or `task->return_code_.load()`
- **Writing**: Use `task->SetReturnCode(value)` or `task->return_code_.store(value)`
- **Initialization**: Use `task->return_code_.store(0)` in constructors

**Examples:**
```cpp
// Reading return code
u32 code = task->GetReturnCode();  // Preferred method
u32 code = task->return_code_.load();  // Direct atomic access

// Setting return code  
task->SetReturnCode(0);  // Preferred method
task->return_code_.store(42);  // Direct atomic access

// Copy operations
new_task->return_code_.store(old_task->return_code_.load());
```

This ensures that return codes can be safely read and written from different threads without data races, particularly important in the multi-threaded runtime environment where tasks may be processed by different workers.

## Unit Testing Standards

### Create Method Success Validation
**CRITICAL**: Always check if Create methods completed successfully in unit tests. Many test failures occur because Create operations fail but tests continue executing against invalid or uninitialized objects.

**Success Criteria**: Create methods succeed when the return code is 0.

**Required Pattern for All Unit Tests:**
```cpp
// After any Create operation in unit tests
ASSERT_EQ(client.GetReturnCode(), 0) << "Create operation failed with return code: " << client.GetReturnCode();

// Or for individual task-based creates
auto create_task = client.AsyncCreate(mctx, pool_query, pool_name, /* other params */);
create_task->Wait();
ASSERT_EQ(create_task->GetReturnCode(), 0) << "Create task failed with return code: " << create_task->GetReturnCode();
```

**Why This Is Critical:**
1. **Early Failure Detection**: Create failures should be caught immediately, not later when dependent operations fail
2. **Clear Error Messages**: Tests should show the actual Create failure, not cryptic downstream errors
3. **Debugging Efficiency**: Helps identify the root cause (Create failure) rather than symptoms
4. **Test Reliability**: Prevents tests from continuing with invalid state and producing misleading results

**Common Failure Scenarios:**
- Pool creation fails due to naming conflicts
- ChiMod runtime not properly initialized
- File path issues for file-based BDev
- Memory allocation failures
- Admin pool not accessible

**Examples of Correct Test Patterns:**
```cpp
TEST(BDevTest, CreateAndUse) {
  chimaera::bdev::Client bdev_client(pool_id);
  
  // Create BDev container
  bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), "test_device.dat", 
                     chimaera::bdev::BdevType::kFile, 1024*1024);
  
  // CRITICAL: Check Create success before proceeding
  ASSERT_EQ(bdev_client.GetReturnCode(), 0) 
    << "BDev Create failed with return code: " << bdev_client.GetReturnCode();
  
  // Now safe to proceed with BDev operations
  auto blocks = bdev_client.AllocateBlocks(HSHM_MCTX, 10);
  // ... rest of test
}

TEST(AdminTest, CreateContainer) {
  chimaera::admin::Client admin_client(chi::kAdminPoolId);
  
  // Create admin container
  admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), "admin");
  
  // CRITICAL: Validate admin creation succeeded
  ASSERT_EQ(admin_client.GetReturnCode(), 0)
    << "Admin Create failed with return code: " << admin_client.GetReturnCode();
  
  // Safe to use admin operations
  // ... rest of test
}
```

**Incorrect Test Pattern (Common Mistake):**
```cpp
TEST(BadTest, MissingCreateCheck) {
  chimaera::bdev::Client bdev_client(pool_id);
  
  // Create BDev container
  bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), "test_device.dat", 
                     chimaera::bdev::BdevType::kFile, 1024*1024);
  
  // WRONG: No check for Create success!
  
  // This will fail cryptically if Create failed
  auto blocks = bdev_client.AllocateBlocks(HSHM_MCTX, 10);  // May crash or return errors
  ASSERT_GT(blocks.size(), 0);  // Misleading test failure message
}
```

This requirement applies to ALL ChiMod Create operations in unit tests including admin, bdev, MOD_NAME, and any custom ChiMods.

# Docker Deployment

## Overview

The Chimaera runtime can be deployed using Docker containers for easy distributed deployment. The Docker setup includes:
- Dockerfile for building the runtime container
- docker-compose.yml for orchestrating multi-node clusters
- Entrypoint script for configuration generation
- Hostfile for cluster node management

## Quick Start

```bash
# Build and start 3-node cluster
cd docker
docker-compose up -d

# Check status
docker-compose ps

# View logs
docker-compose logs -f chimaera-node1

# Stop cluster
docker-compose down
```

## Configuration Methods

### Method 1: Environment Variables (Recommended)

Configure via environment variables in docker-compose.yml:
```yaml
environment:
  - CHI_SCHED_WORKERS=8
  - CHI_MAIN_SEGMENT_SIZE=1G
  - CHI_CLIENT_DATA_SEGMENT_SIZE=512M
  - CHI_RUNTIME_DATA_SEGMENT_SIZE=512M
  - CHI_ZMQ_PORT=5555
  - CHI_LOG_LEVEL=info
  - CHI_SHM_SIZE=2147483648
```

### Method 2: Custom Configuration File

Mount custom YAML config:
```yaml
volumes:
  - ./chimaera_config.yaml:/etc/chimaera/chimaera_config.yaml:ro
```

## Critical Requirements

### Shared Memory Size

**CRITICAL**: Set `shm_size` >= sum of all segment sizes:
```yaml
shm_size: 2gb  # Must be >= main + client_data + runtime_data segments
```

### Hostfile

Create hostfile with cluster node IPs (one per line):
```
172.20.0.10
172.20.0.11
172.20.0.12
```

Mount in docker-compose.yml:
```yaml
volumes:
  - ./hostfile:/etc/chimaera/hostfile:ro
environment:
  - CHI_HOSTFILE=/etc/chimaera/hostfile
```

## Network Configuration

- Default ZeroMQ port: 5555
- Static IPs for predictable routing
- Bridge network for cluster communication

## Files

- `docker/Dockerfile` - Container image definition
- `docker/entrypoint.sh` - Startup and configuration script
- `docker/docker-compose.yml` - Multi-node cluster orchestration
- `docker/hostfile` - Cluster node IP addresses
- `docker/README.md` - Comprehensive deployment guide

See `docker/README.md` for detailed documentation.

