# Chimaera Module Development Guide

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Coding Style](#coding-style)
4. [Module Structure](#module-structure)
5. [Task Development](#task-development)
6. [Client-Server Communication](#client-server-communication)
7. [Memory Management](#memory-management)
8. [Build System Integration](#build-system-integration)
9. [Example Module](#example-module)

## Overview

Chimaera modules (ChiMods) are dynamically loadable components that extend the runtime with new functionality. Each module consists of:
- **Client library**: Minimal code for task submission from user processes
- **Runtime library**: Server-side execution logic
- **Task definitions**: Shared structures for client-server communication
- **Configuration**: YAML metadata describing the module

## Architecture

### Core Principles
1. **Client-Server Separation**: Clients only submit tasks; runtime handles all logic
2. **Shared Memory Communication**: Tasks are allocated in shared memory segments
3. **Task-Based Processing**: All operations are expressed as tasks with methods
4. **Zero-Copy Design**: Data stays in shared memory; only pointers are passed

### Key Components
```
ChiMod/
├── include/
│   └── MOD_NAME/
│       ├── MOD_NAME_client.h     # Client API
│       ├── MOD_NAME_runtime.h    # Runtime container
│       ├── MOD_NAME_tasks.h      # Task definitions
│       └── autogen/
│           ├── MOD_NAME_methods.h    # Method enums
│           └── MOD_NAME_lib_exec.h   # Library exports
├── src/
│   ├── MOD_NAME_client.cc        # Client implementation
│   └── MOD_NAME_runtime.cc       # Runtime implementation
├── chimaera_mod.yaml              # Module configuration
└── CMakeLists.txt                 # Build configuration
```

## Coding Style

### General Guidelines
1. **Namespace**: All module code under `chimaera::MOD_NAME`
2. **Naming Conventions**:
   - Classes: `PascalCase` (e.g., `CustomTask`)
   - Methods: `PascalCase` for public, `camelCase` for private
   - Variables: `snake_case_` with trailing underscore for members
   - Constants: `kConstantName`
   - Enums: `kEnumValue`

3. **Header Guards**: Use `#ifndef MOD_NAME_COMPONENT_H_`
4. **Includes**: System headers first, then library headers, then local headers
5. **Comments**: Use Doxygen-style comments for public APIs

### Code Formatting
```cpp
namespace chimaera::MOD_NAME {

/**
 * Brief description
 * 
 * Detailed description if needed
 * @param param_name Parameter description
 * @return Return value description
 */
class ExampleClass {
 public:
  // Public methods
  void PublicMethod();
  
 private:
  // Private members with trailing underscore
  u32 member_variable_;
};

}  // namespace chimaera::MOD_NAME
```

## Module Structure

### Task Definition (MOD_NAME_tasks.h)

Every task must include:
1. **SHM Constructor**: For deserialization from shared memory
2. **Emplace Constructor**: For creating new tasks with parameters
3. **Data Members**: Using HSHM serializable types

```cpp
#ifndef MOD_NAME_TASKS_H_
#define MOD_NAME_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/MOD_NAME_methods.h"

namespace chimaera::MOD_NAME {

/**
 * Task for Method::kCreate
 * Initializes the container
 */
struct CreateTask : public chi::Task {
  // Required: SHM default constructor
  explicit CreateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc) {}

  // Required: Emplace constructor with parameters
  explicit CreateTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id,
      const chi::DomainQuery &dom_query)
      : chi::Task(alloc, task_node, pool_id, dom_query, 0) {
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCreate;
    task_flags_.Clear();
    dom_query_ = dom_query;
  }
};

/**
 * Custom operation task
 */
struct CustomTask : public chi::Task {
  // Task-specific data using HSHM macros
  INOUT hipc::string data_;      // Input/output string
  IN chi::u32 operation_id_;     // Input parameter
  OUT chi::u32 result_code_;     // Output result

  // SHM constructor
  explicit CustomTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc), 
        data_(alloc), 
        operation_id_(0), 
        result_code_(0) {}

  // Emplace constructor
  explicit CustomTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id,
      const chi::DomainQuery &dom_query,
      const std::string &data,
      chi::u32 operation_id)
      : chi::Task(alloc, task_node, pool_id, dom_query, 10),
        data_(alloc, data),
        operation_id_(operation_id),
        result_code_(0) {
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCustom;
    task_flags_.Clear();
    dom_query_ = dom_query;
  }
};

}  // namespace chimaera::MOD_NAME

#endif  // MOD_NAME_TASKS_H_
```

### Client Implementation (MOD_NAME_client.h/cc)

The client provides a simple API for task submission:

```cpp
#ifndef MOD_NAME_CLIENT_H_
#define MOD_NAME_CLIENT_H_

#include <chimaera/chimaera.h>
#include "MOD_NAME_tasks.h"

namespace chimaera::MOD_NAME {

class Client : public chi::ChiContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Synchronous operation - waits for completion
   */
  void Create(const hipc::MemContext& mctx, 
              const chi::DomainQuery& dom_query) {
    auto task = AsyncCreate(mctx, dom_query);
    task->Wait();
    CHI_IPC->DelTask(task, chi::kMainSegment);
  }

  /**
   * Asynchronous operation - returns immediately
   */
  hipc::FullPtr<CreateTask> AsyncCreate(
      const hipc::MemContext& mctx,
      const chi::DomainQuery& dom_query) {
    auto* ipc_manager = CHI_IPC;
    
    // Allocate task in shared memory
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::kMainSegment,
        HSHM_DEFAULT_MEM_CTX,
        chi::TaskNode(0),
        pool_id_,
        dom_query);
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    return task;
  }
};

}  // namespace chimaera::MOD_NAME

#endif  // MOD_NAME_CLIENT_H_
```

### Runtime Container (MOD_NAME_runtime.h/cc)

The runtime container executes tasks server-side:

```cpp
#ifndef MOD_NAME_RUNTIME_H_
#define MOD_NAME_RUNTIME_H_

#include <chimaera/chimaera.h>
#include "MOD_NAME_tasks.h"

namespace chimaera::MOD_NAME {

class Container : public chi::Container {
 public:
  Container() = default;
  ~Container() override = default;

  /**
   * Create the container (Method::kCreate)
   * This method both creates and initializes the container
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& ctx) {
    // Initialize the container with pool information and domain query
    chi::Container::Init(task->pool_id_, task->dom_query_);
    
    // Create local queues for different priorities
    CreateLocalQueue(chi::kLowLatency, 4);   // 4 lanes for low latency tasks
    CreateLocalQueue(chi::kHighLatency, 2);  // 2 lanes for high latency tasks
    
    // Additional container-specific initialization logic here
    std::cout << "Container created and initialized for pool: " << pool_name_
              << " (ID: " << task->pool_id_ << ")" << std::endl;
  }

  /**
   * Monitor create progress
   */
  void MonitorCreate(chi::MonitorModeId mode, hipc::FullPtr<CreateTask> task,
                     chi::RunContext& ctx) {
    switch (mode) {
      case chi::MonitorModeId::kLocalSchedule: {
        // REQUIRED: Route task to local queue
        if (auto* lane = GetLane(chi::kLowLatency, 0)) {
          lane->Enqueue(task.shm_);
        }
        break;
      }
      case chi::MonitorModeId::kGlobalSchedule: {
        // Optional: Global coordination
        break;
      }
      case chi::MonitorModeId::kCleanup: {
        // Optional: Cleanup - framework handles most cleanup automatically
        break;
      }
    }
  }

  /**
   * Custom operation (Method::kCustom)
   */
  void Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& ctx) {
    // Process the operation
    std::string result = processData(task->data_.str(), 
                                    task->operation_id_);
    task->data_ = hipc::string(main_allocator_, result);
    task->result_code_ = 0;
    // Task completion is handled by the framework
  }

  /**
   * Monitor custom operation
   */
  void MonitorCustom(chi::MonitorModeId mode, hipc::FullPtr<CustomTask> task,
                    chi::RunContext& ctx) {
    switch (mode) {
      case chi::MonitorModeId::kLocalSchedule:
        // Route task to appropriate lane based on operation type
        if (auto* lane = GetLaneByHash(chi::kLowLatency, task->operation_id_)) {
          lane->Enqueue(task.shm_);
        }
        break;
      case chi::MonitorModeId::kGlobalSchedule:
        // Global coordination logic
        break;
      case chi::MonitorModeId::kCleanup:
        // Cleanup logic
        break;
    }
  }

 private:
  std::string processData(const std::string& input, u32 op_id) {
    // Business logic here
    return input + "_processed";
  }
};

}  // namespace chimaera::MOD_NAME

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::MOD_NAME::Container)

#endif  // MOD_NAME_RUNTIME_H_
```

## Task Development

### Task Requirements
1. **Inherit from chi::Task**: All tasks must inherit the base Task class
2. **Two Constructors**: SHM and emplace constructors are mandatory
3. **Serializable Types**: Use HSHM types (hipc::string, hipc::vector, etc.)
4. **Method Assignment**: Set the method_ field to identify the operation
5. **FullPtr Usage**: All task method signatures use `hipc::FullPtr<TaskType>` instead of raw pointers
6. **Monitor Methods**: Every task type MUST have a Monitor method that implements `kLocalSchedule`

### Method System and Auto-Generated Files

#### Method Definitions (autogen/MOD_NAME_methods.h)
Method IDs are now defined as namespace constants instead of enum class values. This eliminates the need for static casting:

```cpp
#ifndef MOD_NAME_AUTOGEN_METHODS_H_
#define MOD_NAME_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

namespace chimaera::MOD_NAME {

namespace Method {
  // Inherited methods
  GLOBAL_CONST chi::u32 kCreate = 0;
  GLOBAL_CONST chi::u32 kDestroy = 1;
  GLOBAL_CONST chi::u32 kNodeFailure = 2;
  GLOBAL_CONST chi::u32 kRecover = 3;
  GLOBAL_CONST chi::u32 kMigrate = 4;
  GLOBAL_CONST chi::u32 kUpgrade = 5;
  
  // Module-specific methods
  GLOBAL_CONST chi::u32 kCustom = 10;
}

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_AUTOGEN_METHODS_H_
```

**Key Changes:**
- **Namespace instead of enum class**: Use `Method::kMethodName` directly
- **GLOBAL_CONST values**: No more static casting required
- **Include chimaera.h**: Required for GLOBAL_CONST macro
- **Direct assignment**: `method_ = Method::kCreate;` (no casting)

#### BaseCreateTask Template System

For modules that need container creation functionality, use the BaseCreateTask template instead of implementing custom CreateTask:

```cpp
#include <admin/admin_tasks.h>  // Include admin BaseCreateTask

namespace chimaera::MOD_NAME {

/**
 * CreateParams for MOD_NAME container creation
 */
struct MOD_NAMECreateParams {
  // Module-specific configuration
  std::string config_data_;
  chi::u32 worker_count_;
  
  // Required: chimod library name
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME";
  
  // Constructors
  MOD_NAMECreateParams() : worker_count_(1) {}
  
  MOD_NAMECreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                       const std::string& config_data = "",
                       chi::u32 worker_count = 1)
      : config_data_(config_data), worker_count_(worker_count) {}
  
  // Cereal serialization
  template<class Archive>
  void serialize(Archive& ar) {
    ar(config_data_, worker_count_);
  }
};

/**
 * CreateTask - Uses BaseCreateTask template with proper method ID
 */
using CreateTask = chimaera::admin::BaseCreateTask<MOD_NAMECreateParams, Method::kCreate>;

}  // namespace chimaera::MOD_NAME
```

#### BaseCreateTask Template Parameters

The BaseCreateTask template has three parameters with smart defaults:

```cpp
template <typename CreateParamsT, 
          chi::u32 MethodId = Method::kGetOrCreatePool, 
          bool IS_ADMIN = false>
struct BaseCreateTask : public chi::Task
```

**Template Parameters:**
1. **CreateParamsT**: Your module's parameter structure (required)
2. **MethodId**: Method ID for the task (default: `kGetOrCreatePool`)
3. **IS_ADMIN**: Whether this is an admin operation (default: `false`)

**Default Values Designed for Non-Admin Modules:**
- Most modules only need to specify `CreateParamsT` and `MethodId`
- Admin-specific parameters (like `IS_ADMIN=true`) are only needed for admin operations
- Pool operations use `kGetOrCreatePool` by default

#### BaseCreateTask Structure

BaseCreateTask provides a unified structure for container creation and pool operations:

```cpp
template <typename CreateParamsT, chi::u32 MethodId, bool IS_ADMIN>
struct BaseCreateTask : public chi::Task {
  // Pool operation parameters
  INOUT hipc::string chimod_name_;     // ChiMod name for loading
  IN hipc::string pool_name_;          // Target pool name
  INOUT hipc::string chimod_params_;   // Serialized CreateParamsT
  IN chi::u32 domain_flags_;           // Domain configuration flags
  INOUT chi::PoolId pool_id_;          // Input: requested ID, Output: actual ID
  
  // Results
  OUT chi::u32 result_code_;           // 0 = success, non-zero = error
  OUT hipc::string error_message_;     // Error description if failed
  
  // Runtime flag set by template parameter
  volatile bool is_admin_;             // Set to IS_ADMIN template value
  
  // Serialization methods
  template<typename... Args>
  void SetParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, Args &&...args);
  
  CreateParamsT GetParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) const;
};
```

**Key Features:**
- **Single pool_id**: Serves as both input (requested) and output (result)
- **Serialized parameters**: `chimod_params_` stores serialized CreateParamsT
- **Error checking**: Use `result_code_ != 0` to check for failures
- **Template-driven behavior**: IS_ADMIN template parameter sets volatile variable
- **No static casting**: Direct method assignment using namespace constants

#### Usage Examples

**Regular ChiMod Container Creation:**
```cpp
// Only specify CreateParamsT and MethodId - uses IS_ADMIN=false default
using CreateTask = chimaera::admin::BaseCreateTask<MyCreateParams, Method::kCreate>;
```

**Pool Get-or-Create Operations:**
```cpp
// Use all defaults - MethodId=kGetOrCreatePool, IS_ADMIN=false
using CreateTask = chimaera::admin::BaseCreateTask<AdminCreateParams>;
```

**Admin Container Creation:**
```cpp
// Admin operations need IS_ADMIN=true explicitly
using AdminCreateTask = chimaera::admin::BaseCreateTask<AdminCreateParams, Method::kCreate, true>;
```

#### Migration from Custom CreateTask

If you have existing custom CreateTask implementations, migrate to BaseCreateTask:

**Before (Custom Implementation):**
```cpp
struct CreateTask : public chi::Task {
  // Custom constructor implementations
  explicit CreateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
                      const chi::TaskNode &task_node,
                      const chi::PoolId &pool_id,
                      const chi::DomainQuery &dom_query)
      : chi::Task(alloc, task_node, pool_id, dom_query, 0) {
    method_ = Method::kCreate;  // Static casting required
    // ... initialization code ...
  }
};
```

**After (BaseCreateTask):**
```cpp
// Create params structure
struct MyCreateParams {
  static constexpr const char* chimod_lib_name = "chimaera_mymodule";
  // ... other params ...
  template<class Archive> void serialize(Archive& ar) { /* ... */ }
};

// Simple type alias - no custom implementation needed
using CreateTask = chimaera::admin::BaseCreateTask<MyCreateParams, Method::kCreate>;
```

**Benefits of Migration:**
- **No static casting**: Direct use of `Method::kCreate`
- **Standardized structure**: Consistent across all modules
- **Built-in serialization**: SetParams/GetParams methods included
- **Error handling**: Standardized result_code and error_message
- **Less boilerplate**: No need to implement constructors manually

### Data Annotations
- `IN`: Input-only parameters (read by runtime)
- `OUT`: Output-only parameters (written by runtime)
- `INOUT`: Bidirectional parameters

### Task Lifecycle
1. Client allocates task in shared memory using `ipc_manager->NewTask()`
2. Client enqueues task pointer to IPC queue
3. Worker dequeues and executes task
4. Framework calls `ipc_manager->DelTask()` to deallocate task from shared memory
5. Task memory is properly reclaimed from the appropriate memory segment

**Note**: Individual `DelTaskType` methods are no longer required. The framework's autogenerated Del dispatcher automatically calls `ipc_manager->DelTask()` for proper shared memory deallocation.

### Framework Del Implementation
The autogenerated Del dispatcher handles task cleanup:

```cpp
inline void Del(Runtime* runtime, chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  auto* ipc_manager = CHI_IPC;
  Method method_enum = static_cast<Method>(method);
  
  switch (method_enum) {
    case Method::kCreate: {
      ipc_manager->DelTask(task_ptr.Cast<CreateTask>(), chi::kMainSegment);
      break;
    }
    case Method::kCustom: {
      ipc_manager->DelTask(task_ptr.Cast<CustomTask>(), chi::kMainSegment);
      break;
    }
    default:
      ipc_manager->DelTask(task_ptr, chi::kMainSegment);
      break;
  }
}
```

This ensures proper shared memory deallocation without requiring module-specific cleanup code.

## Client-Server Communication

### Memory Segments
Three shared memory segments are used:
1. **Main Segment**: Tasks and control structures
2. **Client Data Segment**: User data buffers
3. **Runtime Data Segment**: Runtime-only data

### IPC Queue
Tasks are submitted via a lock-free multi-producer single-consumer queue:
```cpp
// Client side
auto task = ipc_manager->NewTask<CustomTask>(...);
ipc_manager->Enqueue(task, chi::kLowLatency);

// Server side
hipc::Pointer task_ptr = ipc_manager->Dequeue(chi::kLowLatency);
```

## Memory Management

### Allocator Usage
```cpp
// Get context allocator for current segment
hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, allocator);

// Allocate serializable string
hipc::string my_string(ctx_alloc, "initial value");

// Allocate vector
hipc::vector<u32> my_vector(ctx_alloc);
my_vector.resize(100);
```

### Best Practices
1. Always use HSHM types for shared data
2. Pass CtxAllocator to constructors
3. Use FullPtr for cross-process references
4. Let framework handle task cleanup via `ipc_manager->DelTask()`

### Task Allocation and Deallocation Pattern
```cpp
// Client side - allocation
auto task = ipc_manager->NewTask<CustomTask>(
    chi::kMainSegment,
    HSHM_DEFAULT_MEM_CTX,
    chi::TaskNode(0),
    pool_id_,
    dom_query,
    input_data,
    operation_id);

// Client side - cleanup (after task completion)
ipc_manager->DelTask(task, chi::kMainSegment);

// Runtime side - automatic cleanup (no code needed)
// Framework Del dispatcher calls ipc_manager->DelTask() automatically
```

## Build System Integration

### CMakeLists.txt Template
ChiMod CMakeLists.txt files should use the standardized ChimaeraCommon.cmake functions for consistency and proper configuration:

```cmake
cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for your module
# This creates targets: ${NAMESPACE}_${CHIMOD_NAME}_runtime and ${NAMESPACE}_${CHIMOD_NAME}_client
add_chimod_both(
  CHIMOD_NAME YOUR_MODULE_NAME
  RUNTIME_SOURCES src/YOUR_MODULE_NAME_runtime.cc
  CLIENT_SOURCES src/YOUR_MODULE_NAME_client.cc
)

# Install the ChiMod
# Automatically finds and installs the targets created above
install_chimod(
  CHIMOD_NAME YOUR_MODULE_NAME
)
```

### CMakeLists.txt Guidelines

**DO:**
- Use `add_chimod_both()` and `install_chimod()` utility functions
- Set `CHIMOD_NAME` to your module's name
- List source files explicitly in `RUNTIME_SOURCES` and `CLIENT_SOURCES`
- Keep the CMakeLists.txt minimal and consistent

**DON'T:**
- Use manual `add_library()` calls - use the utilities instead
- Include relative paths like `../include/*` - use proper include directories
- Set custom compile definitions - the utilities handle this
- Manually configure target properties - the utilities provide standard settings

### ChiMod Utility Functions

The `add_chimod_both()` function automatically:
- Creates both client and runtime shared libraries
- Sets proper include directories (include/, ${CMAKE_SOURCE_DIR}/include)
- Links against the chimaera library
- Sets required compile definitions (CHI_CHIMOD_NAME, CHIMAERA_CLIENT, CHIMAERA_RUNTIME)
- Configures proper build flags and settings

The `install_chimod()` function automatically:
- Installs libraries to the correct destination
- Sets up proper runtime paths
- Configures installation properties

### Targets Created by add_chimod_both()

When you call `add_chimod_both(CHIMOD_NAME YOUR_MODULE_NAME ...)`, it creates the following CMake targets using the format `${NAMESPACE}_${CHIMOD_NAME}_{client/runtime}`:

#### Runtime Target: `${NAMESPACE}_${CHIMOD_NAME}_runtime`
- **Target Name**: `chimaera_YOUR_MODULE_NAME_runtime` (e.g., `chimaera_admin_runtime`, `chimaera_MOD_NAME_runtime`)
- **Type**: Shared library (`.so` file)
- **Purpose**: Contains server-side execution logic, runs in the Chimaera runtime process
- **Compile Definitions**:
  - `CHI_CHIMOD_NAME="${CHIMOD_NAME}"` - Module name for runtime identification
  - `CHI_NAMESPACE="${NAMESPACE}"` - Project namespace
  - `CHIMAERA_RUNTIME=1` - Enables runtime-specific code paths
- **Include Directories**:
  - `include/` - Local module headers
  - `${CMAKE_SOURCE_DIR}/include` - Chimaera framework headers
- **Dependencies**: Links against `chimaera` library

#### Client Target: `${NAMESPACE}_${CHIMOD_NAME}_client`
- **Target Name**: `chimaera_YOUR_MODULE_NAME_client` (e.g., `chimaera_admin_client`, `chimaera_MOD_NAME_client`)
- **Type**: Shared library (`.so` file)  
- **Purpose**: Contains client-side API, runs in user processes
- **Compile Definitions**:
  - `CHI_CHIMOD_NAME="${CHIMOD_NAME}"` - Module name for client identification
  - `CHI_NAMESPACE="${NAMESPACE}"` - Project namespace
  - `CHIMAERA_CLIENT=1` - Enables client-specific code paths
  - `CHIMAERA_RUNTIME=1` - Enables shared code paths between client/runtime
- **Include Directories**:
  - `include/` - Local module headers
  - `${CMAKE_SOURCE_DIR}/include` - Chimaera framework headers
- **Dependencies**: Links against `chimaera` library

#### Namespace Configuration
The namespace is automatically read from `chimaera_repo.yaml` files. The system searches up the directory tree from the CMakeLists.txt location to find the first `chimaera_repo.yaml` file:

**Main project `chimaera_repo.yaml`:**
```yaml
namespace: chimaera  # Main project namespace
```

**Module repository `chimods/chimaera_repo.yaml`:**
```yaml
namespace: chimods   # Modules get this namespace
```

This means modules in the `chimods/` directory will use the "chimods" namespace, creating targets like `chimods_admin_runtime`, while other components use the main project namespace.

#### Example Output Files
For a module named "admin" with namespace "chimods" (from `chimods/chimaera_repo.yaml`), the build produces:
```
build/bin/libchimods_admin_runtime.so    # Runtime library  
build/bin/libchimods_admin_client.so     # Client library
```

#### Using the Targets
You can reference these targets in your CMakeLists.txt using the full target name:
```cmake
# Add custom properties to the runtime target
set_target_properties(chimaera_${CHIMOD_NAME}_runtime PROPERTIES
  VERSION 1.0.0
  SOVERSION 1
)

# Add additional dependencies if needed
target_link_libraries(chimaera_${CHIMOD_NAME}_runtime PRIVATE some_external_lib)

# Or use the global property to get the actual target name
get_property(RUNTIME_TARGET GLOBAL PROPERTY ${CHIMOD_NAME}_RUNTIME_TARGET)
target_link_libraries(${RUNTIME_TARGET} PRIVATE some_external_lib)
```

### Module Configuration (chimaera_mod.yaml)
```yaml
name: MOD_NAME
version: 1.0.0
description: "Module description"
author: "Author Name"
methods:
  - kCreate
  - kCustom
dependencies: []
```

### Auto-Generated Method Files
Each module requires an auto-generated methods file at `include/MOD_NAME/autogen/MOD_NAME_methods.h`. This file must:

1. **Include chimaera.h**: Required for GLOBAL_CONST macro
2. **Use namespace constants**: Define methods as `GLOBAL_CONST chi::u32` values
3. **Follow naming convention**: Method names should start with `k` (e.g., `kCreate`, `kCustom`)

**Required Template:**
```cpp
#ifndef MOD_NAME_AUTOGEN_METHODS_H_
#define MOD_NAME_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

namespace chimaera::MOD_NAME {

namespace Method {
  // Standard inherited methods (always include these)
  GLOBAL_CONST chi::u32 kCreate = 0;
  GLOBAL_CONST chi::u32 kDestroy = 1;
  GLOBAL_CONST chi::u32 kNodeFailure = 2;
  GLOBAL_CONST chi::u32 kRecover = 3;
  GLOBAL_CONST chi::u32 kMigrate = 4;
  GLOBAL_CONST chi::u32 kUpgrade = 5;
  
  // Module-specific methods (customize these)
  GLOBAL_CONST chi::u32 kCustom = 10;
  // Add more module-specific methods starting from 10+
}

} // namespace chimaera::MOD_NAME

#endif // MOD_NAME_AUTOGEN_METHODS_H_
```

**Important Notes:**
- **GLOBAL_CONST is required**: Do not use `const` or `constexpr` - use `GLOBAL_CONST`
- **Include chimaera.h**: This header defines the GLOBAL_CONST macro
- **Standard methods 0-5**: Always include the inherited methods (kCreate through kUpgrade)
- **Custom methods 10+**: Start custom methods from ID 10 to avoid conflicts
- **No static casting needed**: Use method values directly (e.g., `method_ = Method::kCreate;`)

### Runtime Entry Points
Use the `CHI_TASK_CC` macro to define module entry points:

```cpp
// At the end of your runtime source file (_runtime.cc)
CHI_TASK_CC(your_namespace::YourContainerClass)
```

This macro automatically generates all required extern "C" functions and gets the module name from `YourContainerClass::CreateParams::chimod_lib_name`:
- `alloc_chimod()` - Creates container instance
- `new_chimod()` - Creates and initializes container  
- `get_chimod_name()` - Returns module name
- `destroy_chimod()` - Destroys container instance

**Requirements for CHI_TASK_CC to work:**
1. Your runtime class must define a public typedef: `using CreateParams = your_namespace::CreateParams;`
2. Your CreateParams struct must have: `static constexpr const char* chimod_lib_name = "your_module_name";`

Example:
```cpp
namespace chimaera::your_module {

struct CreateParams {
  static constexpr const char* chimod_lib_name = "chimaera_your_module";
  // ... other parameters
};

class Runtime : public chi::Container {
public:
  using CreateParams = chimaera::your_module::CreateParams;  // Required for CHI_TASK_CC
  // ... rest of class
};

}  // namespace chimaera::your_module
```
- `is_chimaera_chimod_` - Module identification flag

## Example Module

See the `chimods/MOD_NAME` directory for a complete working example that demonstrates:
- Task definition with proper constructors
- Client API with sync/async methods
- Runtime container with execution logic
- Build system integration
- YAML configuration

### Creating a New Module
1. Copy the MOD_NAME template directory
2. Rename all MOD_NAME occurrences to your module name
3. Update the chimaera_mod.yaml configuration
4. Define your tasks in the _tasks.h file
5. Implement client API in _client.h/cc
6. Implement runtime logic in _runtime.h/cc
7. Add `CHI_TASK_CC(YourContainerClass)` at the end of runtime source
8. Add to the build system
9. Test with client and runtime

## Recent Changes and Best Practices

### Container Initialization Pattern
Starting with the latest version, container initialization has been simplified:

1. **No Separate Init Method**: The `Init` method has been merged with `Create`
2. **Create Does Everything**: The `Create` method now handles both container creation and initialization
3. **Access to Task Data**: Since `Create` receives the CreateTask, you have access to pool_id and domain_query from the task

### Framework-Managed Task Cleanup
Task cleanup is handled by the framework using the IPC manager:

1. **No Custom Del Methods Required**: Individual `DelTaskType` methods are no longer needed
2. **IPC Manager Handles Cleanup**: The framework automatically calls `ipc_manager->DelTask()` to deallocate tasks from shared memory
3. **Memory Segment Deallocation**: Tasks are properly removed from their respective memory segments (typically `kMainSegment`)

### Simplified ChiMod Entry Points
ChiMod entry points are now hidden behind the `CHI_TASK_CC` macro:

1. **Single Macro Call**: Replace complex extern "C" blocks with one macro
2. **Automatic Container Integration**: Works seamlessly with `chi::Container` base class
3. **Cleaner Module Code**: Eliminates boilerplate entry point code

```cpp
// Old approach (complex extern "C" block)
extern "C" {
  chi::ChiContainer* alloc_chimod() { /* ... */ }
  chi::ChiContainer* new_chimod(/*...*/) { /* ... */ }
  const char* get_chimod_name() { /* ... */ }
  void destroy_chimod(/*...*/) { /* ... */ }
  bool is_chimaera_chimod_ = true;
}

// New approach (simple macro)
CHI_TASK_CC(chimaera::MOD_NAME::Runtime)
```

```cpp
void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& ctx) {
  // Initialize the container with data from the task
  chi::Container::Init(task->pool_id_, task->dom_query_);
  
  // Set up queues and resources
  CreateLocalQueue(chi::kLowLatency, 4);
  CreateLocalQueue(chi::kHighLatency, 2);
  
  // Container is now ready for operation
}
```

### FullPtr Parameter Pattern
All runtime methods now use `hipc::FullPtr<TaskType>` instead of raw pointers:

```cpp
// Old pattern (deprecated)
void Custom(CustomTask* task, chi::RunContext& ctx) { ... }

// New pattern (current)
void Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& ctx) { ... }
```

**Benefits of FullPtr:**
- **Shared Memory Safety**: Provides safe access across process boundaries
- **Automatic Dereferencing**: Use `task->field` just like raw pointers
- **Memory Management**: Framework handles allocation/deallocation
- **Null Checking**: Use `task.IsNull()` to check validity

### Migration Guide
When updating existing modules:

1. **Remove Init Override**: Delete custom `Init` method implementations
2. **Update Create Method**: Move initialization logic from `Init` to `Create`
3. **Change Method Signatures**: Replace `TaskType*` with `hipc::FullPtr<TaskType>`
4. **Update Monitor Methods**: Ensure all monitoring methods use FullPtr
5. **Implement kLocalSchedule**: Every Monitor method MUST implement `kLocalSchedule` mode
6. **Remove Del Methods**: Delete all `DelTaskType` methods - framework calls `ipc_manager->DelTask()` automatically
7. **Update Autogen Files**: Ensure Del dispatcher calls `ipc_manager->DelTask()` instead of custom Del methods
8. **Replace Entry Points**: Replace extern "C" blocks with `CHI_TASK_CC(ClassName)` macro
9. **Remove Completion Calls**: Framework handles task completion automatically

## Custom Namespace Configuration

### Overview
While the default namespace is `chimaera`, you can customize the namespace for your ChiMod modules. This is useful for:
- **Project Branding**: Use your own project or company namespace
- **Avoiding Conflicts**: Prevent naming conflicts with other ChiMod collections
- **Module Organization**: Group related modules under a custom namespace

### Configuring Custom Namespace

The namespace is controlled by the `chimaera_repo.yaml` file in your project root:

```yaml
namespace: your_custom_namespace
```

For example:
```yaml
namespace: mycompany
```

### Required Changes for Custom Namespace

When using a custom namespace, you must update several components:

#### 1. **CreateParams chimod_lib_name**
The most critical change is updating the `chimod_lib_name` in your CreateParams:

```cpp
// Default chimaera namespace
struct CreateParams {
  static constexpr const char* chimod_lib_name = "chimaera_your_module";
};

// Custom namespace example
struct CreateParams {
  static constexpr const char* chimod_lib_name = "mycompany_your_module";
};
```

#### 2. **Module Namespace Declaration**
Update your module's C++ namespace:

```cpp
// Default
namespace chimaera::your_module {
  // module code
}

// Custom
namespace mycompany::your_module {
  // module code
}
```

#### 3. **CMake Library Names**
The CMake system automatically uses your custom namespace. Libraries will be named:
- Default: `libchimaera_module_runtime.so`, `libchimaera_module_client.so`
- Custom: `libmycompany_module_runtime.so`, `libmycompany_module_client.so`

#### 4. **Runtime Integration**
If your runtime code references the admin module or other system modules, update the references:

```cpp
// Default admin module reference
auto* admin_chimod = module_manager->GetChiMod("chimaera_admin");

// Custom namespace admin module
auto* admin_chimod = module_manager->GetChiMod("mycompany_admin");
```

### Checklist for Custom Namespace

- [ ] **Update chimaera_repo.yaml** with your custom namespace
- [ ] **Update CreateParams::chimod_lib_name** to use custom namespace prefix
- [ ] **Update C++ namespace declarations** in all module files
- [ ] **Update runtime references** to admin module and other system modules
- [ ] **Update any hardcoded module names** in configuration or startup code
- [ ] **Rebuild all modules** after namespace changes
- [ ] **Update library search paths** if needed for deployment

### Example: Complete Custom Namespace Module

```yaml
# chimaera_repo.yaml
namespace: mycompany
```

```cpp
// mymodule_tasks.h
namespace mycompany::mymodule {

struct CreateParams {
  static constexpr const char* chimod_lib_name = "mycompany_mymodule";
  // ... other parameters
};

using CreateTask = chimaera::admin::BaseCreateTask<CreateParams, Method::kCreate>;

}  // namespace mycompany::mymodule
```

```cpp
// mymodule_runtime.h
namespace mycompany::mymodule {

class Runtime : public chi::Container {
public:
  using CreateParams = mycompany::mymodule::CreateParams;  // Required for CHI_TASK_CC
  // ... rest of class
};

}  // namespace mycompany::mymodule
```

```cpp
// mymodule_runtime.cc
CHI_TASK_CC(mycompany::mymodule::Runtime)
```

### Important Notes

- **Library Name Consistency**: The `chimod_lib_name` must exactly match what the CMake system generates
- **Admin Module**: If you customize the namespace, you may also want to rebuild the admin module with your custom namespace
- **Backward Compatibility**: Changing namespace breaks compatibility with existing deployments using default namespace
- **Documentation**: Update any module-specific documentation to reflect the new namespace

## Advanced Topics

### Task Scheduling
Tasks can be scheduled with different priorities:
- `kLowLatency`: For time-critical operations
- `kHighLatency`: For batch processing

### Monitoring Modes
Runtime containers support various monitoring modes:
- `kLocalSchedule`: Route tasks to local container queue lanes (REQUIRED)
- `kGlobalSchedule`: Coordinate global task distribution
- `kCleanup`: Clean up completed tasks and resources

**IMPORTANT**: All monitor methods MUST implement `kLocalSchedule` mode. This mode is responsible for routing tasks to the appropriate local queue lanes for execution. Failure to implement this will result in tasks not being processed.

## Task Monitoring Requirements

### Mandatory kLocalSchedule Implementation
Every task type must have a corresponding Monitor method that implements `kLocalSchedule`. This is critical for task execution:

```cpp
void MonitorCustom(chi::MonitorModeId mode, 
                  hipc::FullPtr<CustomTask> task_ptr,
                  chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule:
      // REQUIRED: Route task to appropriate lane
      if (auto* lane = GetLaneByHash(chi::kLowLatency, task_ptr->operation_id_)) {
        lane->Enqueue(task_ptr.shm_);
      }
      break;
      
    case chi::MonitorModeId::kGlobalSchedule:
      // Optional: Global coordination logic
      break;
      
    case chi::MonitorModeId::kCleanup:
      // Optional: Cleanup logic
      break;
  }
}
```

### Lane Selection Strategies
When implementing `kLocalSchedule`, choose the appropriate lane selection strategy:

1. **Fixed Lane Assignment**:
```cpp
// Always use lane 0 for simple cases
if (auto* lane = GetLane(chi::kLowLatency, 0)) {
  lane->Enqueue(task_ptr.shm_);
}
```

2. **Hash-Based Load Balancing**:
```cpp
// Distribute based on task data for load balancing
if (auto* lane = GetLaneByHash(chi::kLowLatency, task_ptr->operation_id_)) {
  lane->Enqueue(task_ptr.shm_);
}
```

3. **Priority-Based Routing**:
```cpp
// Route to different queues based on task properties
QueuePriority priority = (task_ptr->operation_id_ > 1000) ? 
                        chi::kHighLatency : chi::kLowLatency;
if (auto* lane = GetLane(priority, 0)) {
  lane->Enqueue(task_ptr.shm_);
}
```

### Common Monitor Implementation Pattern
```cpp
void MonitorTaskType(chi::MonitorModeId mode,
                    hipc::FullPtr<TaskType> task_ptr,
                    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // STEP 1: Choose appropriate queue and lane
      QueuePriority queue_priority = DetermineQueuePriority(task_ptr);
      LaneId lane_id = DetermineLaneId(task_ptr);
      
      // STEP 2: Get the lane
      auto* lane = GetLane(queue_priority, lane_id);
      if (!lane) {
        // Fallback to default lane
        lane = GetLane(chi::kLowLatency, 0);
      }
      
      // STEP 3: Enqueue the task
      if (lane) {
        lane->Enqueue(task_ptr.shm_);
      }
      break;
    }
    
    case chi::MonitorModeId::kGlobalSchedule:
      // Implement global coordination if needed
      break;
      
    case chi::MonitorModeId::kCleanup:
      // Implement cleanup if needed
      break;
      
    default:
      // Handle unknown modes gracefully
      break;
  }
}
```

### Error Handling
```cpp
void Custom(hipc::FullPtr<CustomTask> task, chi::RunContext& ctx) {
  try {
    // Operation logic
    task->result_code_ = 0;
  } catch (const std::exception& e) {
    task->result_code_ = 1;
    task->data_ = hipc::string(main_allocator_, e.what());
  }
  // Framework handles task completion automatically
}
```

## Debugging Tips

1. **Check Shared Memory**: Use `ipcs -m` to view segments
2. **Verify Task State**: Check task completion status
3. **Monitor Queue Depth**: Use GetProcessQueue() to inspect queues
4. **Enable Debug Logging**: Set CHI_DEBUG environment variable
5. **Use GDB**: Attach to runtime process for debugging

### Common Issues and Solutions

**Tasks Not Being Executed:**
- **Cause**: Missing `kLocalSchedule` implementation in Monitor methods
- **Solution**: Ensure every Monitor method has a `kLocalSchedule` case that enqueues the task
- **Debug**: Add logging in Monitor methods to verify they're being called

**Queue Overflow or Deadlocks:**
- **Cause**: Tasks being enqueued but not dequeued from lanes
- **Solution**: Verify lane creation in Create() method and proper task routing
- **Debug**: Check lane sizes with `lane->Size()` and `lane->IsEmpty()`

**Memory Leaks in Shared Memory:**
- **Cause**: Tasks not being properly cleaned up
- **Solution**: Implement `kCleanup` mode in Monitor methods
- **Debug**: Monitor shared memory usage with `ipcs -m`

## Performance Considerations

1. **Minimize Allocations**: Reuse buffers when possible
2. **Batch Operations**: Submit multiple tasks together
3. **Use Appropriate Segments**: Put large data in client_data_segment
4. **Avoid Blocking**: Use async operations when possible
5. **Profile First**: Measure before optimizing

## Quick Reference Checklist

When creating a new Chimaera module, ensure you have:

### Task Definition Checklist (`_tasks.h`)
- [ ] Tasks inherit from `chi::Task` or use BaseCreateTask template
- [ ] SHM constructor with CtxAllocator parameter (if custom task)
- [ ] Emplace constructor with all required parameters (if custom task)
- [ ] Uses HSHM serializable types (hipc::string, hipc::vector, etc.)
- [ ] Method constant assigned in constructor (e.g., `method_ = Method::kCreate;`)
- [ ] **No static casting**: Use Method namespace constants directly
- [ ] Include auto-generated methods file for Method constants

### Runtime Container Checklist (`_runtime.h/cc`)
- [ ] Inherits from `chi::Container`
- [ ] Create() method calls `chi::Container::Init()`
- [ ] Create() method calls `CreateLocalQueue()` for needed priorities
- [ ] All task methods use `hipc::FullPtr<TaskType>` parameters
- [ ] **CRITICAL**: Every Monitor method implements `kLocalSchedule` case
- [ ] `kLocalSchedule` calls `GetLane()` or `GetLaneByHash()`
- [ ] `kLocalSchedule` calls `lane->Enqueue(task_ptr.shm_)`
- [ ] **NO custom Del methods needed** - framework calls `ipc_manager->DelTask()` automatically
- [ ] Uses `CHI_TASK_CC(ClassName)` macro for entry points

### Client API Checklist (`_client.h/cc`)
- [ ] Inherits from `chi::ChiContainerClient`
- [ ] Uses `CHI_IPC->NewTask<TaskType>()` for allocation
- [ ] Uses `CHI_IPC->Enqueue()` for task submission
- [ ] Uses `CHI_IPC->DelTask()` for cleanup
- [ ] Provides both sync and async methods

### Build System Checklist
- [ ] CMakeLists.txt creates both client and runtime libraries
- [ ] chimaera_mod.yaml defines module metadata
- [ ] **Auto-generated methods file**: `autogen/MOD_NAME_methods.h` with Method namespace
- [ ] **Include chimaera.h**: In methods file for GLOBAL_CONST macro
- [ ] **GLOBAL_CONST constants**: Use namespace constants, not enum class
- [ ] Proper install targets configured
- [ ] Links against chimaera library

### Common Pitfalls to Avoid
- [ ] ❌ Forgetting `kLocalSchedule` implementation (tasks won't execute)
- [ ] ❌ Using raw pointers instead of FullPtr in runtime methods
- [ ] ❌ Not calling `chi::Container::Init()` in Create method
- [ ] ❌ Using non-HSHM types in task data members
- [ ] ❌ Forgetting to create local queues in Create method
- [ ] ❌ Implementing custom Del methods (framework calls `ipc_manager->DelTask()` automatically)
- [ ] ❌ Writing complex extern "C" blocks (use `CHI_TASK_CC` macro instead)
- [ ] ❌ **Using static_cast with Method values** (use Method::kName directly)
- [ ] ❌ **Missing chimaera.h include** in methods file (GLOBAL_CONST won't work)
- [ ] ❌ **Using enum class for methods** (use namespace with GLOBAL_CONST instead)
- [ ] ❌ **Forgetting BaseCreateTask template** for container creation (reduces boilerplate)

Remember: **kLocalSchedule is mandatory** - without it, your tasks will never be executed!