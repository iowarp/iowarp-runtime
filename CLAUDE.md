## Code Style

Use the Google C++ style guide for C++.

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
- For file-based BDev: `pool_name` parameter serves as the file path
- For RAM-based BDev: `pool_name` parameter serves as unique identifier
- Signature: `Create(mctx, pool_query, pool_name, bdev_type, total_size, io_depth, alignment)`

## ChiMod Linking Requirements

### Runtime Library Linking
When linking against ChiMod runtime libraries, use the following pattern:

```cmake
target_link_libraries(your_target
  chimaera                        # Main Chimaera library (always required)
  chimaera_admin_runtime          # Admin module runtime (always required)
  chimaera_admin_client           # Admin module client (always required)
  chimaera_${CHIMOD}_runtime      # Specific ChiMod runtime library
  chimaera_${CHIMOD}_client       # Specific ChiMod client library
  ${HermesShm_LIBRARIES}          # HSHM libraries
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)
```

### ChiMod Creation and Installation
Use the ChimaeraCommon.cmake utilities for creating ChiMods:

**Creating ChiMod libraries:**
```cmake
add_chimod_both(
  CHIMOD_NAME your_chimod_name
  RUNTIME_SOURCES src/your_chimod_runtime.cc src/autogen/your_chimod_lib_exec.cc
  CLIENT_SOURCES src/your_chimod_client.cc
)
```

**Installing ChiMod libraries:**
```cmake
install_chimod(
  CHIMOD_NAME your_chimod_name
)
```

### Include Directory Requirements
When using ChiMods, include the necessary headers:

```cmake
target_include_directories(your_target PUBLIC
  ${CMAKE_SOURCE_DIR}/chimods/admin/include     # Admin module headers (always required)
  ${CMAKE_SOURCE_DIR}/chimods/your_chimod/include  # Your ChiMod headers
)
```

**Note**: All ChiMod headers are now organized under the `chimaera` namespace in include directories:
- Structure: `[chimod_directory]/include/chimaera/[module_name]/`
- Example: Admin headers are in `admin/include/chimaera/admin/`
- Headers include: `[module_name]_client.h`, `[module_name]_runtime.h`, `[module_name]_tasks.h`
- Auto-generated headers are in the `autogen/` subdirectory
- **Note**: The chimod directory name is flexible and doesn't need to match the namespace

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
  ${HermesShm_LIBRARIES}      # HSHM shared memory framework
  ${CMAKE_THREAD_LIBS_INIT}   # Threading support for runtime
)
```

**Client executable linking pattern (no CHIMAERA_RUNTIME definition):**
```cmake
target_link_libraries(chimaera_stop_runtime
  cxx                    # Client-side objects only
  ${HermesShm_LIBRARIES}      # HSHM libraries
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
Each ChiMod is automatically installed as a separate CMake package based on the namespace from `chimaera_repo.yaml`:

**External CMakeLists.txt pattern:**
```cmake
# Find individual module packages (automatically created from namespace)
find_package(chimaera::MOD_NAME REQUIRED)
find_package(chimaera::admin REQUIRED) 
find_package(chimaera::core REQUIRED)

target_link_libraries(your_external_app
  chimaera::MOD_NAME_client     # ChiMod client library
  chimaera::admin_client        # Admin client (required)
  chimaera::cxx            # Main chimaera library
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
  mod_client.Create(HSHM_MCTX, pool_query);
}
```

**Header Include Structure:**
All ChiMod headers follow the consistent pattern: `#include <chimaera/[module_name]/[module_name]_[type].h>`
- Admin client: `#include <chimaera/admin/admin_client.h>`
- MOD_NAME client: `#include <chimaera/MOD_NAME/MOD_NAME_client.h>`
- Runtime headers: `#include <chimaera/[module_name]/[module_name]_runtime.h>`
- Task headers: `#include <chimaera/[module_name]/[module_name]_tasks.h>`

**Dependency requirements:**
External applications must have access to all dependencies:
- HermesShm (with its MPI, Boost, and other dependencies)
- cereal
- Boost (fiber, context components)
- Set `CMAKE_PREFIX_PATH` to include installation prefixes of all dependencies

**Dynamic export system details:**
- Package names: Automatically generated as `<namespace>::<module>` (e.g., `chimaera::MOD_NAME`)
- Target format: `<namespace>::<module>_<type>` (e.g., `chimaera::MOD_NAME_client`)
- Config files: `<namespace>::<module>Config.cmake` and `<namespace>::<module>ConfigVersion.cmake`
- Installation paths: `lib/cmake/<namespace>::<module>/`
- Namespace read from: `chimaera_repo.yaml` file in the chimod repo directory
- No hardcoded names: System adapts to any namespace automatically

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

### Pool Naming Convention
For BDev ChiMods, the pool name MUST be the file_path parameter. Do not use separate artificial pool names.

**Correct Usage:**
```cpp
// Use file_path directly as pool name
std::string pool_name = file_path.empty() ? "bdev_ram_" + std::to_string(pool_id_.ToU64()) : file_path;
```

**Incorrect Usage:**
```cpp
// WRONG - Don't create artificial pool names
std::string pool_name = "bdev_pool_" + std::to_string(pool_id_.ToU64());
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
- `NewCopy()` - Task copy constructor methods

**Rationale:**
These methods are automatically generated in `autogen/` files based on task definitions. Manual duplication in runtime files creates conflicts, compilation errors, and maintenance issues.

**Correct approach:**
- Let the code generation system handle all serialization methods
- Focus runtime implementation on core task execution logic only
- Use the generated methods through the normal task execution pipeline

# Locking and Synchronization

## CoMutex and CoRwLock

The chimaera runtime provides two specialized coroutine-aware mutex types for runtime code:

### CoMutex (Coroutine Mutex)
- **Header**: `chimaera/comutex.h`
- **Purpose**: TaskNode-grouped mutex that allows multiple tasks from the same TaskNode to proceed together
- **Key Features**:
  - Tasks are grouped by TaskNode (ignoring minor number) to prevent deadlocks
  - Uses `unordered_map[TaskNode] -> list<FullPtr<Task>>` internally
  - Blocked tasks are sent back to their lane stored in `task->run_ctx_`
  - Provides `ScopedCoMutex` for RAII-style locking

### CoRwLock (Coroutine Reader-Writer Lock)
- **Header**: `chimaera/corwlock.h`  
- **Purpose**: TaskNode-grouped reader-writer lock with similar grouping semantics
- **Key Features**:
  - Multiple readers from any TaskNode group can proceed simultaneously
  - Single writer TaskNode group can hold exclusive access
  - Tasks from same TaskNode group as current lock holder can always proceed
  - Provides `ScopedCoRwReadLock` and `ScopedCoRwWriteLock` for RAII-style locking

### Usage Example
```cpp
#include "chimaera/comutex.h"
#include "chimaera/corwlock.h"

// CoMutex usage
chi::CoMutex mutex;
{
  chi::ScopedCoMutex lock(mutex);
  // Critical section - other TaskNodes blocked
}

// CoRwLock usage  
chi::CoRwLock rwlock;
{
  chi::ScopedCoRwReadLock read_lock(rwlock);
  // Multiple readers can proceed
}
{
  chi::ScopedCoRwWriteLock write_lock(rwlock);
  // Exclusive writer access
}
```

**Note**: These locks are only for runtime code and have no client-side equivalents.

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

