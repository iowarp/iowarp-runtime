## Code Style

Use the Google C++ style guide for C++.

You should store the pointer returned by the singleton GetInstance method. Avoid dereferencing GetInstance method directly using either -> or *. E.g., do not do ``hshm::Singleton<T>::GetInstance()->var_``. You should do ``auto *x = hshm::Singleton<T>::GetInstance(); x->var_;``.


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
- Structure: `chimods/[module_name]/include/chimaera/[module_name]/`
- Example: Admin headers are in `chimods/admin/include/chimaera/admin/`
- Headers include: `[module_name]_client.h`, `[module_name]_runtime.h`, `[module_name]_tasks.h`
- Auto-generated headers are in the `autogen/` subdirectory

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
  const chi::PoolId pool_id = static_cast<chi::PoolId>(7000);
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
- Namespace read from: `chimaera_repo.yaml` file
- No hardcoded names: System adapts to any namespace automatically

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
- Repository structure and configuration (`chimaera_repo.yaml`)
- CMake package discovery (`find_package` usage)
- Custom namespace configuration
- Build system integration and troubleshooting

## Workflow
Use the incremental logic builder agent when making code changes.

Use the compiler subagent for making changes to cmakes and identifying places that need to be fixed in the code.

Always verify that code continue to compiles after making changes. Avoid commenting out code to fix compilation issues.

Whenever building unit tests, make sure to use the unit testing agent.

Whenever performing filesystem queries or executing programs, use the filesystem ops script agent.

NEVER DO MOCK CODE OR STUB CODE UNLESS SPECIFICALLY STATED OTHERWISE. ALWAYS IMPLEMENT REAL, WORKING CODE.

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
  chi::ScopedCoMutex lock(mutex, current_task);
  // Critical section - other TaskNodes blocked
}

// CoRwLock usage  
chi::CoRwLock rwlock;
{
  chi::ScopedCoRwReadLock read_lock(rwlock, current_task);
  // Multiple readers can proceed
}
{
  chi::ScopedCoRwWriteLock write_lock(rwlock, current_task);
  // Exclusive writer access
}
```

**Note**: These locks are only for runtime code and have no client-side equivalents.

# ChiMod Development Requirements

## Container Initialization Pattern

All ChiMod containers must implement proper initialization following these requirements:

### 1. InitClient Method
Every runtime container must implement `InitClient()` to initialize its client interface:

```cpp
void Runtime::InitClient(const chi::PoolId& pool_id) {
  // Initialize the client for this ChiMod
  client_ = Client(pool_id);
}
```

**Purpose**: Enables the runtime container to make client calls to itself or other ChiMods during task execution.

### 2. Create Method Implementation 
The `Create()` method must initialize the container and create local queues:

```cpp
void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  // Initialize the container with pool information and domain query
  chi::Container::Init(task->pool_id_, task->pool_query_);
  
  // Create local queues with semantic names and priorities
  CreateLocalQueue(kMetadataQueue, 1, chi::kHighLatency);         // Metadata operations
  CreateLocalQueue(kClientSendTaskInQueue, 1, chi::kHighLatency); // Client task input processing
  CreateLocalQueue(kServerRecvTaskInQueue, 1, chi::kHighLatency); // Server task input reception
  // Add more queues as needed for your container
  
  std::cout << "Container created and initialized for pool: " << pool_name_
            << " (ID: " << task->pool_id_ << ")" << std::endl;
}
```

**CreateLocalQueue Parameters**:
- `queue_id`: Semantic constant (e.g., `kMetadataQueue`, not raw integers)
- `num_lanes`: Number of concurrent processing lanes for this queue
- `priority`: Either `chi::kLowLatency` or `chi::kHighLatency`

### 3. Monitor Method Implementation
Monitor methods handle task routing via `rctx.route_lane_` pointer assignment:

```cpp
void Runtime::MonitorCreate(chi::MonitorModeId mode,
                           hipc::FullPtr<CreateTask> task_ptr,
                           chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // CORRECT: Set route_lane_ to indicate where task should be routed
      {
        auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
        if (!lane_ptr.IsNull()) {
          rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
      }
      break;
      
    case chi::MonitorModeId::kGlobalSchedule:
      // Coordinate global distribution
      break;
      
    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      rctx.estimated_completion_time_us = 1000.0;  // 1ms example
      break;
  }
}
```

## CRITICAL: Correct Lane Routing Pattern

**NEVER directly enqueue tasks in Monitor methods**. The work orchestrator handles actual task enqueuing.

### ❌ INCORRECT Pattern:
```cpp
// DON'T DO THIS - bypasses work orchestrator
case chi::MonitorModeId::kLocalSchedule:
  if (auto* lane = GetLane(chi::kLowLatency, 0)) {
    lane->Enqueue(task_ptr.shm_);  // WRONG - direct enqueuing
  }
  break;
```

### ✅ CORRECT Pattern:
```cpp
// DO THIS - let work orchestrator handle enqueuing
case chi::MonitorModeId::kLocalSchedule:
  // Set route_lane_ to indicate where task should be routed
  {
    auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
    if (!lane_ptr.IsNull()) {
      rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
    }
  }
  break;
```

**Why this matters**:
- Work orchestrator manages task lifecycle and scheduling policies
- Direct enqueuing bypasses load balancing and monitoring
- `route_lane_` pointer tells the orchestrator where to place the task
- Framework handles actual enqueuing after monitor returns

### Queue ID Naming Requirements

**NEVER use raw integers for queue IDs**. Always use semantic constants:

```cpp
// Define semantic queue IDs
private:
  static const chi::QueueId kMetadataQueue = 0;
  static const chi::QueueId kProcessingQueue = 1;  
  static const chi::QueueId kNetworkQueue = 2;

// Use semantic names in CreateLocalQueue calls
CreateLocalQueue(kMetadataQueue, 1, chi::kHighLatency);
CreateLocalQueue(kProcessingQueue, 4, chi::kLowLatency);
```

## ChiMod Library Naming Requirements

### CreateParams chimod_lib_name Convention

When defining the `chimod_lib_name` in CreateParams structures, **DO NOT include the `_runtime` suffix**. The runtime system automatically appends `_runtime` when loading the library.

**✅ CORRECT:**
```cpp
struct CreateParams {
  // Other parameters...
  
  // Required: chimod library name WITHOUT _runtime suffix
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME";
};
```

**❌ INCORRECT:**
```cpp
struct CreateParams {
  // Other parameters...
  
  // WRONG - includes _runtime suffix
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME_runtime";
};
```

**Why this matters:**
- The module manager automatically appends `_runtime` to locate the runtime library
- Including `_runtime` in the name would result in looking for `chimaera_MOD_NAME_runtime_runtime.so`
- This convention keeps the library naming consistent and predictable

## Build Configuration

- Always use the debug CMakePreset when compiling code in this repo.
- All compilation warnings have been resolved as of the current state

## Code Quality Standards

### Compilation Standards
- All code must compile without warnings or errors
- Use appropriate variable types to avoid sign comparison warnings (e.g., `size_t` for container sizes)
- Mark unused variables with `(void)variable_name;` to suppress warnings when the variable is intentionally unused
- Follow strict type safety to prevent implicit conversions that generate warnings

## ChiMod Documentation Requirements

### Documentation Structure
Every ChiMod MUST include a `doc/` directory with comprehensive API documentation and integration guides:

```
chimods/MOD_NAME/
├── doc/
│   ├── README.md           # Overview and quick start
│   ├── API.md             # Detailed API reference
│   └── INTEGRATION.md     # CMake linking and build integration
├── include/chimaera/MOD_NAME/
├── src/
└── CMakeLists.txt
```

### Required Documentation Files

#### 1. README.md - Module Overview
Must contain:
- **Purpose**: What the ChiMod does and its use cases
- **Quick Start**: Basic usage example with client initialization
- **Dependencies**: Required libraries and runtime components
- **Installation**: Building and installing the ChiMod
- **Configuration**: Any module-specific configuration options

#### 2. API.md - Complete API Reference
Must document:
- **Client API**: All public methods in the Client class
  - Method signatures with parameter descriptions
  - Return values and error conditions
  - Usage examples for each method
  - Sync vs async method pairs
- **Task Types**: All task structures and their fields
  - Input/output parameter descriptions
  - Required vs optional fields
  - Serialization requirements
- **Configuration Parameters**: CreateParams structure fields
  - Default values and valid ranges
  - Impact of different parameter choices
- **Error Handling**: Error codes and their meanings

#### 3. INTEGRATION.md - CMake and Build Integration
Must provide:
- **CMake Integration**: Complete find_package examples
- **Include Paths**: Required header includes with full paths
- **Linking Requirements**: Target linking examples for different use cases
- **Build Dependencies**: External library requirements
- **Installation Instructions**: Step-by-step installation guide
- **Troubleshooting**: Common build and runtime issues

### Documentation Template Structure

#### README.md Template:
```markdown
# ModuleName ChiMod

## Overview
Brief description of what this ChiMod provides...

## Quick Start
```cpp
#include <chimaera/chimaera.h>
#include <chimaera/module_name/module_name_client.h>

int main() {
  chi::CHIMAERA_CLIENT_INIT();
  const chi::PoolId pool_id = static_cast<chi::PoolId>(7000);
  chimaera::module_name::Client client(pool_id);
  
  // Basic usage example
  auto pool_query = chi::PoolQuery::Local();
  client.Create(HSHM_MCTX, pool_query);
}
```

## Dependencies
- HermesShm
- Chimaera core runtime
- Admin ChiMod (always required)

## Installation
See [INTEGRATION.md](INTEGRATION.md) for complete build instructions.
```

#### API.md Template:
```markdown
# ModuleName API Reference

## Client Class: `chimaera::module_name::Client`

### Container Management
#### `Create()`
Creates and initializes the module container.

**Signature:**
```cpp
void Create(const hipc::MemContext& mctx, 
           const chi::PoolQuery& pool_query,
           const CreateParams& params = CreateParams())
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `pool_query`: Pool domain query (typically `chi::PoolQuery::Local()`)
- `params`: Configuration parameters (optional)

**Example:**
```cpp
auto pool_query = chi::PoolQuery::Local();
client.Create(HSHM_MCTX, pool_query);
```

### Custom Operations
Document each custom method...

## Task Types

### CreateTask
Container creation task with module-specific parameters.

### CustomTask
Document each custom task type...

## Configuration

### CreateParams Structure
Document all configuration fields...
```

#### INTEGRATION.md Template:
```markdown
# ModuleName Integration Guide

## CMake Integration

### External Projects
```cmake
find_package(chimaera::module_name REQUIRED)
find_package(chimaera::admin REQUIRED)
find_package(chimaera::core REQUIRED)

target_link_libraries(your_app
  chimaera::module_name_client
  chimaera::admin_client
  chimaera::cxx
)
```

### Include Requirements
```cpp
#include <chimaera/chimaera.h>
#include <chimaera/module_name/module_name_client.h>
#include <chimaera/admin/admin_client.h>
```

## Build Dependencies
- HermesShm with MPI and Boost support
- cereal serialization library
- Boost.Fiber and Boost.Context

## Installation
1. Build Chimaera with this module
2. Install to system or custom prefix
3. Set CMAKE_PREFIX_PATH for external projects

## Troubleshooting
Common issues and solutions...
```

### Documentation Standards
- **Completeness**: Every public API must be documented
- **Accuracy**: Keep documentation synchronized with code changes
- **Examples**: Include working code examples for all major features
- **Format**: Use consistent Markdown formatting and code blocks
- **Links**: Cross-reference between documentation files
- **Updates**: Update documentation as part of code review process

### Integration with Build System
Documentation should be included in CMake installation:

```cmake
# Install documentation
install(DIRECTORY doc/
  DESTINATION share/doc/chimaera-${CHIMOD_NAME}
  COMPONENT documentation
)
```

This ensures documentation is available to users of installed ChiMods and maintains consistency across all modules in the Chimaera ecosystem.