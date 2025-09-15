# Chimaera Module Development Guide

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Coding Style](#coding-style)
4. [Module Structure](#module-structure)
5. [Configuration and Code Generation](#configuration-and-code-generation)
6. [Task Development](#task-development)
7. [Synchronization Primitives](#synchronization-primitives)
8. [Pool Query and Task Routing](#pool-query-and-task-routing)
9. [Client-Server Communication](#client-server-communication)
10. [Memory Management](#memory-management)
    - [CHI_CLIENT Buffer Allocation](#chi_client-buffer-allocation)
    - [Shared-Memory Compatible Data Structures](#shared-memory-compatible-data-structures)
11. [Build System Integration](#build-system-integration)
12. [External ChiMod Development](#external-chimod-development)
13. [Example Module](#example-module)

## Overview

Chimaera modules (ChiMods) are dynamically loadable components that extend the runtime with new functionality. Each module consists of:
- **Client library**: Minimal code for task submission from user processes
- **Runtime library**: Server-side execution logic
- **Task definitions**: Shared structures for client-server communication
- **Configuration**: YAML metadata describing the module

**Header Organization**: All ChiMod headers are organized under the `chimaera` namespace directory structure (`include/chimaera/[module_name]/`) to provide clear namespace separation and prevent header conflicts.

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
│   └── chimaera/
│       └── MOD_NAME/
│           ├── MOD_NAME_client.h     # Client API
│           ├── MOD_NAME_runtime.h    # Runtime container
│           ├── MOD_NAME_tasks.h      # Task definitions
│           └── autogen/
│               └── MOD_NAME_methods.h    # Method constants
├── src/
│   ├── MOD_NAME_client.cc        # Client implementation
│   ├── MOD_NAME_runtime.cc       # Runtime implementation
│   └── autogen/
│       └── MOD_NAME_lib_exec.cc  # Auto-generated virtual method implementations
├── chimaera_mod.yaml              # Module configuration
└── CMakeLists.txt                 # Build configuration
```

**Include Directory Structure:**
- All ChiMod headers are organized under the `chimaera` namespace directory
- Structure: `include/chimaera/[module_name]/`
- Example: Admin headers are in `include/chimaera/admin/`
- Headers follow naming pattern: `[module_name]_[type].h`
- Auto-generated headers are in the `autogen/` subdirectory
- **Note**: The chimod directory name (e.g., `chimods/`, `modules/`) is flexible and doesn't need to match the namespace

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

Task definition patterns:

1. **CreateParams Structure**: Define configuration parameters for container creation
2. **CreateTask Template**: Use GetOrCreatePoolTask template for container creation (non-admin modules)
3. **Custom Tasks**: Define custom tasks with SHM/Emplace constructors and HSHM data members

```cpp
#ifndef MOD_NAME_TASKS_H_
#define MOD_NAME_TASKS_H_

#include <chimaera/chimaera.h>
#include <chimaera/MOD_NAME/autogen/MOD_NAME_methods.h>
// Include admin tasks for GetOrCreatePoolTask
#include <chimaera/admin/admin_tasks.h>

namespace chimaera::MOD_NAME {

/**
 * CreateParams for MOD_NAME chimod
 * Contains configuration parameters for MOD_NAME container creation
 */
struct CreateParams {
  // MOD_NAME-specific parameters
  std::string config_data_;
  chi::u32 worker_count_;
  
  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME";
  
  // Default constructor
  CreateParams() : worker_count_(1) {}
  
  // Constructor with allocator and parameters
  CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, 
               const std::string& config_data = "", 
               chi::u32 worker_count = 1)
      : config_data_(config_data), worker_count_(worker_count) {
    // MOD_NAME parameters use standard types, so allocator isn't needed directly
    // but it's available for future use with HSHM containers
  }
  
  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    ar(config_data_, worker_count_);
  }
};

/**
 * CreateTask - Initialize the MOD_NAME container
 * Type alias for GetOrCreatePoolTask with CreateParams (uses kGetOrCreatePool method)
 * Non-admin modules should use GetOrCreatePoolTask instead of BaseCreateTask
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * Custom operation task
 */
struct CustomTask : public chi::Task {
  // Task-specific data using HSHM macros
  INOUT chi::string data_;      // Input/output string
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
      const chi::PoolQuery &pool_query,
      const std::string &data,
      chi::u32 operation_id)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        data_(alloc, data),
        operation_id_(operation_id),
        result_code_(0) {
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kCustom;
    task_flags_.Clear();
    pool_query_ = pool_query;
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
#include <chimaera/MOD_NAME/MOD_NAME_tasks.h>

namespace chimaera::MOD_NAME {

class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Synchronous operation - waits for completion
   */
  void Create(const hipc::MemContext& mctx, 
              const chi::PoolQuery& pool_query,
              const CreateParams& params = CreateParams()) {
    auto task = AsyncCreate(mctx, pool_query, params);
    task->Wait();
    
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;
    
    CHI_IPC->DelTask(task);
  }

  /**
   * Asynchronous operation - returns immediately
   */
  hipc::FullPtr<CreateTask> AsyncCreate(
      const hipc::MemContext& mctx,
      const chi::PoolQuery& pool_query,
      const CreateParams& params = CreateParams()) {
    auto* ipc_manager = CHI_IPC;
    
    // CRITICAL: CreateTask MUST use admin pool for GetOrCreatePool processing
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskNode(),
        chi::kAdminPoolId,  // Always use admin pool for CreateTask
        pool_query,
        CreateParams::chimod_lib_name,  // ChiMod name from CreateParams
        pool_name_,             // Pool name from base client
        params);                // CreateParams with configuration
    
    // Submit to runtime
    ipc_manager->Enqueue(task);
    return task;
  }
};

}  // namespace chimaera::MOD_NAME

#endif  // MOD_NAME_CLIENT_H_
```

### ChiMod Name Requirements

**CRITICAL**: All ChiMod clients MUST use `CreateParams::chimod_lib_name` instead of hardcoding module names.

#### Correct Usage
```cpp
// CORRECT: Use CreateParams::chimod_lib_name
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,
    pool_query,
    CreateParams::chimod_lib_name,  // Dynamic reference to CreateParams
    pool_name,
    pool_id,
    params);
```

#### Incorrect Usage
```cpp
// WRONG: Never hardcode module names
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,
    pool_query,
    "chimaera_MOD_NAME",  // Hardcoded name breaks flexibility
    pool_name,
    pool_id,
    params);
```

#### Why This is Required

1. **Namespace Flexibility**: Allows ChiMods to work with different namespace configurations
2. **Single Source of Truth**: The module name is defined once in CreateParams
3. **External ChiMods**: Essential for external ChiMods using custom namespaces
4. **Maintainability**: Changes to module names only require updating CreateParams

#### Implementation Pattern

All non-admin ChiMods using `GetOrCreatePoolTask` must follow this pattern:

```cpp
// In AsyncCreate method
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskNode(),
    chi::kAdminPoolId,                    // Always use admin pool
    pool_query,
    CreateParams::chimod_lib_name,        // REQUIRED: Use static member
    pool_name,                            // Pool identifier
    pool_id,                              // Target pool ID
    /* ...CreateParams arguments... */);
```

**Note**: The admin ChiMod uses `BaseCreateTask` directly and doesn't require the chimod name parameter.

### Runtime Container (MOD_NAME_runtime.h/cc)

The runtime container executes tasks server-side:

```cpp
#ifndef MOD_NAME_RUNTIME_H_
#define MOD_NAME_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/MOD_NAME/MOD_NAME_tasks.h>

namespace chimaera::MOD_NAME {

class Container : public chi::Container {
 public:
  Container() = default;
  ~Container() override = default;

  /**
   * Initialize client for this container (REQUIRED)
   */
  void InitClient(const chi::PoolId& pool_id) {
    // Initialize the client for this ChiMod
    client_ = Client(pool_id);
  }

  /**
   * Create the container (Method::kCreate)
   * This method both creates and initializes the container
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& ctx) {
    // Initialize the container with pool information and pool query
    chi::Container::Init(task->pool_id_, task->pool_query_);
    
    // Create local queues with semantic names, lane counts, and priorities
    CreateLocalQueue(kMetadataQueue, 1, chi::kHighLatency);      // 1 lane for metadata operations
    CreateLocalQueue(kProcessingQueue, 4, chi::kLowLatency);     // 4 lanes for low latency tasks
    CreateLocalQueue(kBatchQueue, 2, chi::kHighLatency);         // 2 lanes for batch processing
    
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
        // CORRECT: Set route_lane_ to indicate where task should be routed
        auto lane_ptr = GetLaneFullPtr(kMetadataQueue, 0);
        if (!lane_ptr.IsNull()) {
          ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
        break;
      }
      case chi::MonitorModeId::kGlobalSchedule: {
        // Optional: Global coordination
        break;
      }
      case chi::MonitorModeId::kEstLoad: {
        // Estimate task execution time
        ctx.estimated_completion_time_us = 1000.0;  // 1ms for container creation
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
      case chi::MonitorModeId::kLocalSchedule: {
        // CORRECT: Set route_lane_ based on task properties for load balancing
        auto lane_ptr = GetLaneFullPtrByHash(kProcessingQueue, task->operation_id_);
        if (!lane_ptr.IsNull()) {
          ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
        }
        break;
      }
      case chi::MonitorModeId::kGlobalSchedule:
        // Global coordination logic
        break;
      case chi::MonitorModeId::kEstLoad:
        // Estimate execution time based on operation complexity
        ctx.estimated_completion_time_us = task->operation_id_ * 100.0;  // Example calculation
        break;
    }
  }

 private:
  // Queue ID constants (REQUIRED: Use semantic names, not raw integers)
  static const chi::QueueId kMetadataQueue = 0;
  static const chi::QueueId kProcessingQueue = 1;
  static const chi::QueueId kBatchQueue = 2;

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

## Configuration and Code Generation

### Overview
Chimaera uses a two-level configuration system with automated code generation:

1. **chimaera_repo.yaml**: Repository-wide configuration (namespace, version, etc.)
2. **chimaera_mod.yaml**: Module-specific configuration (method IDs, metadata)
3. **chi_refresh_repo**: Utility script that generates autogen files from YAML configurations

### chimaera_repo.yaml
Located at `chimods/chimaera_repo.yaml`, this file defines repository-wide settings:

```yaml
# Repository Configuration
namespace: chimaera        # MUST match namespace in all chimaera_mod.yaml files
version: 1.0.0
description: "Chimaera Runtime ChiMod Repository"

# Module discovery - directories to scan for ChiMods
modules:
  - MOD_NAME
  - admin  
  - bdev
```

**Key Requirements:**
- The `namespace` field MUST be identical in both chimaera_repo.yaml and all chimaera_mod.yaml files
- Used by build system for CMake package generation and installation paths
- Determines export target names: `${namespace}::${module}_runtime`, `${namespace}::${module}_client`

### chimaera_mod.yaml
Each ChiMod must have its own configuration file specifying methods and metadata:

```yaml
# MOD_NAME ChiMod Configuration
module_name: MOD_NAME
namespace: chimaera        # MUST match chimaera_repo.yaml namespace
version: 1.0.0

# Inherited Methods (fixed IDs)
kCreate: 0        # Container creation (required)
kDestroy: 1       # Container destruction (required)
kNodeFailure: -1  # Not implemented (-1 means disabled)
kRecover: -1      # Not implemented 
kMigrate: -1      # Not implemented
kUpgrade: -1      # Not implemented

# Custom Methods (start from 10, use sequential IDs)
kCustom: 10       # Custom operation method
kCoMutexTest: 20  # CoMutex synchronization testing method
kCoRwLockTest: 21 # CoRwLock reader-writer synchronization testing method
```

**Method ID Assignment Rules:**
- **0-9**: Reserved for system methods (kCreate=0, kDestroy=1, etc.)
- **10+**: Custom methods (assign sequential IDs starting from 10)
- **Disabled methods**: Use -1 to disable inherited methods not implemented
- **Consistency**: Once assigned, never change method IDs (breaks compatibility)

### chi_refresh_repo Utility

The `chi_refresh_repo` utility automatically generates autogen files from YAML configurations.

#### Usage
```bash
# From project root, regenerate all autogen files
./build/bin/chi_refresh_repo chimods

# The utility will:
# 1. Read chimaera_repo.yaml for global settings
# 2. Scan each module's chimaera_mod.yaml 
# 3. Generate MOD_NAME_methods.h with method constants
# 4. Generate MOD_NAME_lib_exec.cc with virtual method dispatch
```

#### Generated Files
For each ChiMod, the utility generates:

1. **`include/chimaera/MOD_NAME/autogen/MOD_NAME_methods.h`**:
   ```cpp
   namespace chimaera::MOD_NAME {
   namespace Method {
   GLOBAL_CONST chi::u32 kCreate = 0;
   GLOBAL_CONST chi::u32 kDestroy = 1;
   GLOBAL_CONST chi::u32 kCustom = 10;
   GLOBAL_CONST chi::u32 kCoMutexTest = 20;
   }  // namespace Method
   }  // namespace chimaera::MOD_NAME
   ```

2. **`src/autogen/MOD_NAME_lib_exec.cc`**: 
   - Virtual method dispatch (Runtime::Run, Runtime::Monitor, etc.)
   - Task serialization support (SaveIn/Out, LoadIn/Out)
   - Memory management (Del, NewCopy)

#### When to Run chi_refresh_repo
**ALWAYS** run chi_refresh_repo when:
- Adding new methods to chimaera_mod.yaml
- Changing method IDs or names
- Adding new ChiMods to the repository
- Modifying namespace or version information

#### Important Notes
- **Never manually edit autogen files** - they are overwritten by chi_refresh_repo
- **Run chi_refresh_repo before building** after YAML changes
- **Commit autogen files to git** so other developers don't need to regenerate
- **Method IDs are permanent** - changing them breaks binary compatibility

### Workflow Summary
1. Define methods in `chimaera_mod.yaml` with sequential IDs
2. Implement corresponding methods in `MOD_NAME_runtime.h/cc`
3. Run `./build/bin/chi_refresh_repo chimods` to generate autogen files
4. Build project with `make` - autogen files provide the dispatch logic
5. Autogen files handle virtual method routing, serialization, and memory management

This automated approach ensures consistency across all ChiMods and reduces boilerplate code maintenance.

## Task Development

### Task Requirements
1. **Inherit from chi::Task**: All tasks must inherit the base Task class
2. **Two Constructors**: SHM and emplace constructors are mandatory
3. **Serializable Types**: Use HSHM types (chi::string, chi::vector, etc.) for member variables
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

For modules that need container creation functionality, use the BaseCreateTask template instead of implementing custom CreateTask. However, there are different approaches depending on whether your module is the admin module or a regular ChiMod:

##### GetOrCreatePoolTask vs BaseCreateTask Usage

**For Non-Admin Modules (Recommended Pattern):**

All non-admin ChiMods should use `GetOrCreatePoolTask` which is a specialized version of BaseCreateTask designed for external pool creation:

```cpp
#include <admin/admin_tasks.h>  // Include admin templates

namespace chimaera::MOD_NAME {

/**
 * CreateParams for MOD_NAME container creation
 */
struct CreateParams {
  // Module-specific configuration
  std::string config_data_;
  chi::u32 worker_count_;
  
  // Required: chimod library name
  static constexpr const char* chimod_lib_name = "chimaera_MOD_NAME";
  
  // Constructors
  CreateParams() : worker_count_(1) {}
  
  CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
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
 * CreateTask - Non-admin modules should use GetOrCreatePoolTask
 * This uses Method::kGetOrCreatePool and is designed for external pool creation
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

}  // namespace chimaera::MOD_NAME
```

**For Admin Module Only:**

The admin module itself uses BaseCreateTask directly with Method::kCreate:

```cpp
namespace chimaera::admin {

/**
 * CreateTask - Admin uses BaseCreateTask with Method::kCreate and IS_ADMIN=true
 */
using CreateTask = BaseCreateTask<CreateParams, Method::kCreate, true>;

}  // namespace chimaera::admin
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

**GetOrCreatePoolTask Template:**

The `GetOrCreatePoolTask` template is a convenient alias that uses the optimal defaults for non-admin modules:

```cpp
template<typename CreateParamsT>
using GetOrCreatePoolTask = BaseCreateTask<CreateParamsT, Method::kGetOrCreatePool, false>;
```

**When to Use Each Pattern:**
- **GetOrCreatePoolTask**: For all non-admin ChiMods (recommended)
- **BaseCreateTask with Method::kCreate**: Only for admin module internal operations
- **BaseCreateTask with Method::kGetOrCreatePool**: Same as GetOrCreatePoolTask (not typically used directly)

#### BaseCreateTask Structure

BaseCreateTask provides a unified structure for container creation and pool operations:

```cpp
template <typename CreateParamsT, chi::u32 MethodId, bool IS_ADMIN>
struct BaseCreateTask : public chi::Task {
  // Pool operation parameters
  INOUT chi::string chimod_name_;     // ChiMod name for loading
  IN chi::string pool_name_;          // Target pool name
  INOUT chi::string chimod_params_;   // Serialized CreateParamsT
  INOUT chi::PoolId pool_id_;          // Input: requested ID, Output: actual ID
  
  // Results
  OUT chi::u32 result_code_;           // 0 = success, non-zero = error
  OUT chi::string error_message_;     // Error description if failed
  
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

**Non-Admin ChiMod Container Creation (Recommended):**
```cpp
// Use GetOrCreatePoolTask for all non-admin modules
using CreateTask = chimaera::admin::GetOrCreatePoolTask<MyCreateParams>;
```

**Admin Module Container Creation:**
```cpp
// Admin module uses BaseCreateTask with Method::kCreate and IS_ADMIN=true
using CreateTask = chimaera::admin::BaseCreateTask<AdminCreateParams, Method::kCreate, true>;
```

**Alternative (Not Recommended):**
```cpp
// Direct BaseCreateTask usage - GetOrCreatePoolTask is cleaner
using CreateTask = chimaera::admin::BaseCreateTask<MyCreateParams, Method::kGetOrCreatePool, false>;
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
                      const chi::PoolQuery &pool_query)
      : chi::Task(alloc, task_node, pool_id, pool_query, 0) {
    method_ = Method::kCreate;  // Static casting required
    // ... initialization code ...
  }
};
```

**After (GetOrCreatePoolTask - Recommended for Non-Admin Modules):**
```cpp
// Create params structure
struct CreateParams {
  static constexpr const char* chimod_lib_name = "chimaera_mymodule";
  // ... other params ...
  template<class Archive> void serialize(Archive& ar) { /* ... */ }
};

// Simple type alias using GetOrCreatePoolTask - no custom implementation needed
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
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

## Synchronization Primitives

Chimaera provides specialized cooperative synchronization primitives designed for the runtime's task-based architecture. **These should be used instead of standard synchronization primitives** like `std::mutex`, `std::shared_mutex`, or `pthread_mutex` when synchronizing access to module data structures.

### Why Use Chimaera Synchronization Primitives?

**Critical: Always use CoMutex and CoRwLock for module synchronization:**

1. **Cooperative Design**: Compatible with Chimaera's fiber-based task execution
2. **TaskNode Grouping**: Tasks sharing the same TaskNode can proceed together (bypassing locks)
3. **Deadlock Prevention**: Designed to prevent deadlocks in the runtime environment
4. **Runtime Integration**: Automatically integrate with CHI_CUR_WORKER and task context
5. **Performance**: Optimized for the runtime's execution model

**Do NOT use these standard synchronization primitives in module code:**
- ❌ `std::mutex` - Can cause fiber blocking issues
- ❌ `std::shared_mutex` - Not compatible with task execution model
- ❌ `pthread_mutex_t` - Can deadlock with runtime scheduling
- ❌ `std::condition_variable` - Incompatible with cooperative scheduling

### CoMutex: Cooperative Mutual Exclusion

CoMutex provides mutual exclusion with TaskNode grouping support. Tasks sharing the same TaskNode can bypass the lock and execute concurrently.

#### Basic Usage

```cpp
#include <chimaera/comutex.h>

class Runtime : public chi::Container {
private:
  // Static member for shared synchronization across all container instances
  static chi::CoMutex shared_mutex_;
  
  // Instance member for per-container synchronization
  chi::CoMutex instance_mutex_;

public:
  void SomeTask(hipc::FullPtr<SomeTaskType> task, chi::RunContext& rctx) {
    // Manual lock/unlock
    shared_mutex_.Lock();
    // ... critical section ...
    shared_mutex_.Unlock();
    
    // OR use RAII scoped lock (recommended)
    chi::ScopedCoMutex lock(instance_mutex_);
    // ... critical section ...
    // Automatically unlocks when leaving scope
  }
};

// Static member definition (required)
chi::CoMutex Runtime::shared_mutex_;
```

#### Key Features

1. **Automatic Task Context**: Uses CHI_CUR_WORKER internally - no task parameters needed
2. **TaskNode Grouping**: Tasks with the same TaskNode bypass the mutex
3. **RAII Support**: ScopedCoMutex for automatic lock management
4. **Try-Lock Support**: Non-blocking lock attempts

#### API Reference

```cpp
namespace chi {
  class CoMutex {
  public:
    // Blocking operations
    void Lock();                    // Block until lock acquired
    void Unlock();                  // Release the lock
    bool TryLock();                 // Non-blocking lock attempt
    
    // No task parameters needed - uses CHI_CUR_WORKER automatically
  };
  
  // RAII wrapper (recommended)
  class ScopedCoMutex {
  public:
    explicit ScopedCoMutex(CoMutex& mutex);  // Locks in constructor
    ~ScopedCoMutex();                        // Unlocks in destructor
  };
}
```

### CoRwLock: Cooperative Reader-Writer Lock

CoRwLock provides reader-writer semantics with TaskNode grouping. Multiple readers can proceed concurrently, but writers have exclusive access.

#### Basic Usage

```cpp
#include <chimaera/corwlock.h>

class Runtime : public chi::Container {
private:
  static chi::CoRwLock data_lock_;  // Protect shared data structures

public:
  void ReadTask(hipc::FullPtr<ReadTaskType> task, chi::RunContext& rctx) {
    // Manual reader lock
    data_lock_.ReadLock();
    // ... read operations ...
    data_lock_.ReadUnlock();
    
    // OR use RAII scoped reader lock (recommended)
    chi::ScopedCoRwReadLock lock(data_lock_);
    // ... read operations ...
    // Automatically unlocks when leaving scope
  }
  
  void WriteTask(hipc::FullPtr<WriteTaskType> task, chi::RunContext& rctx) {
    // RAII scoped writer lock (recommended)
    chi::ScopedCoRwWriteLock lock(data_lock_);
    // ... write operations ...
    // Automatically unlocks when leaving scope
  }
};

// Static member definition
chi::CoRwLock Runtime::data_lock_;
```

#### Key Features

1. **Multiple Readers**: Concurrent read access when no writers are active
2. **Exclusive Writers**: Writers get exclusive access, blocking all other operations
3. **TaskNode Grouping**: Tasks with same TaskNode can bypass reader locks
4. **Automatic Context**: Uses CHI_CUR_WORKER for task identification
5. **RAII Support**: Scoped locks for both readers and writers

#### API Reference

```cpp
namespace chi {
  class CoRwLock {
  public:
    // Reader operations
    void ReadLock();                // Acquire reader lock
    void ReadUnlock();              // Release reader lock
    bool TryReadLock();             // Non-blocking reader lock attempt
    
    // Writer operations  
    void WriteLock();               // Acquire exclusive writer lock
    void WriteUnlock();             // Release writer lock
    bool TryWriteLock();            // Non-blocking writer lock attempt
  };
  
  // RAII wrappers (recommended)
  class ScopedCoRwReadLock {
  public:
    explicit ScopedCoRwReadLock(CoRwLock& lock);  // Acquire read lock
    ~ScopedCoRwReadLock();                        // Release read lock
  };
  
  class ScopedCoRwWriteLock {
  public:
    explicit ScopedCoRwWriteLock(CoRwLock& lock); // Acquire write lock
    ~ScopedCoRwWriteLock();                       // Release write lock
  };
}
```

### TaskNode Grouping Behavior

Both CoMutex and CoRwLock support TaskNode grouping, which allows related tasks to bypass synchronization:

```cpp
// Tasks created with the same TaskNode can proceed together
auto task_node = chi::CreateTaskNode();

// These tasks share the same TaskNode - they can bypass CoMutex/CoRwLock
auto task1 = ipc_manager->NewTask<Task1>(task_node, pool_id, pool_query, ...);
auto task2 = ipc_manager->NewTask<Task2>(task_node, pool_id, pool_query, ...);

// This task has a different TaskNode - must respect locks normally
auto task3 = ipc_manager->NewTask<Task3>(chi::CreateTaskNode(), pool_id, pool_query, ...);
```

**Key Points:**
- Tasks with the same TaskNode are considered "grouped" and can bypass locks
- Use TaskNode grouping for logically related operations that don't need mutual exclusion
- Different TaskNodes must respect normal lock semantics

### Best Practices

1. **Use RAII Wrappers**: Always prefer `ScopedCoMutex` and `ScopedCoRw*Lock` over manual lock/unlock
2. **Static vs Instance**: Use static members for cross-container synchronization, instance members for per-container data
3. **Member Definition**: Don't forget to define static members in your .cc file
4. **Choose Appropriate Lock**: Use CoRwLock for read-heavy workloads, CoMutex for simple mutual exclusion
5. **Minimal Critical Sections**: Keep locked sections as small as possible
6. **TaskNode Design**: Group related tasks that can safely bypass locks

### Example: Module with Synchronized Data Structure

```cpp
// In MOD_NAME_runtime.h
class Runtime : public chi::Container {
private:
  // Synchronized data structure
  chi::hash_map<chi::u32, ModuleData> data_map_;
  
  // Synchronization primitives
  static chi::CoRwLock data_lock_;        // For data_map_ access
  static chi::CoMutex operation_mutex_;   // For exclusive operations

public:
  void ReadData(hipc::FullPtr<ReadDataTask> task, chi::RunContext& rctx);
  void WriteData(hipc::FullPtr<WriteDataTask> task, chi::RunContext& rctx);
  void ExclusiveOperation(hipc::FullPtr<ExclusiveTask> task, chi::RunContext& rctx);
};

// In MOD_NAME_runtime.cc  
chi::CoRwLock Runtime::data_lock_;
chi::CoMutex Runtime::operation_mutex_;

void Runtime::ReadData(hipc::FullPtr<ReadDataTask> task, chi::RunContext& rctx) {
  chi::ScopedCoRwReadLock lock(data_lock_);  // Multiple readers allowed
  
  // Safe to read data_map_ concurrently
  auto it = data_map_.find(task->key_);
  if (it != data_map_.end()) {
    task->result_data_ = it->second;
    task->result_ = 0;  // Success
  } else {
    task->result_ = 1;  // Not found
  }
}

void Runtime::WriteData(hipc::FullPtr<WriteDataTask> task, chi::RunContext& rctx) {
  chi::ScopedCoRwWriteLock lock(data_lock_);  // Exclusive writer access
  
  // Safe to modify data_map_ exclusively
  data_map_[task->key_] = task->new_data_;
  task->result_ = 0;  // Success
}

void Runtime::ExclusiveOperation(hipc::FullPtr<ExclusiveTask> task, chi::RunContext& rctx) {
  chi::ScopedCoMutex lock(operation_mutex_);  // Exclusive operation
  
  // Perform operation that requires complete exclusivity
  // ... complex operation ...
  task->result_ = 0;  // Success
}
```

This synchronization model ensures thread-safe access to module data structures while maintaining compatibility with Chimaera's cooperative task execution system.

## Pool Query and Task Routing

### Overview of PoolQuery

PoolQuery is a fundamental component of Chimaera's task routing system that determines where and how tasks are executed across the distributed runtime. It provides flexible routing strategies for load balancing, locality optimization, and distributed execution patterns.

### PoolQuery Types

The `chi::PoolQuery` class provides six different routing modes through static factory methods:

#### 1. Local Mode
```cpp
chi::PoolQuery::Local()
```
- **Purpose**: Routes tasks to the local node only
- **Use Case**: Operations that must execute on the calling node
- **Example**: Local file system operations, node-specific diagnostics
```cpp
// Client usage
client.Create(HSHM_MCTX, chi::PoolQuery::Local());
```

#### 2. Direct ID Mode
```cpp
chi::PoolQuery::DirectId(ContainerId container_id)
```
- **Purpose**: Routes to a specific container by its unique ID
- **Use Case**: Targeted operations on known containers
- **Example**: Container-specific configuration changes
```cpp
// Route to container with ID 42
auto query = chi::PoolQuery::DirectId(ContainerId(42));
client.UpdateConfig(HSHM_MCTX, query, new_config);
```

#### 3. Direct Hash Mode
```cpp
chi::PoolQuery::DirectHash(u32 hash)
```
- **Purpose**: Routes using consistent hash-based load balancing
- **Use Case**: Distributing operations across containers deterministically
- **Example**: Key-value store operations where keys map to specific containers
```cpp
// Hash-based routing for a key
u32 hash = std::hash<std::string>{}(key);
auto query = chi::PoolQuery::DirectHash(hash);
client.Put(HSHM_MCTX, query, key, value);
```

#### 4. Range Mode
```cpp
chi::PoolQuery::Range(u32 offset, u32 count)
```
- **Purpose**: Routes to a range of containers
- **Use Case**: Batch operations across multiple containers
- **Example**: Parallel scan operations, bulk updates
```cpp
// Process containers 10-19 (10 containers starting at offset 10)
auto query = chi::PoolQuery::Range(10, 10);
client.BulkUpdate(HSHM_MCTX, query, update_data);
```

#### 5. Broadcast Mode
```cpp
chi::PoolQuery::Broadcast()
```
- **Purpose**: Routes to all containers in the pool
- **Use Case**: Global operations affecting all containers
- **Example**: Configuration updates, global cache invalidation
```cpp
// Broadcast configuration change to all containers
auto query = chi::PoolQuery::Broadcast();
client.InvalidateCache(HSHM_MCTX, query);
```

#### 6. Physical Mode
```cpp
chi::PoolQuery::Physical(u32 node_id)
```
- **Purpose**: Routes to a specific physical node by ID
- **Use Case**: Node-specific operations in distributed deployments
- **Example**: Remote node administration, cross-node data migration
```cpp
// Execute on physical node 3
auto query = chi::PoolQuery::Physical(3);
client.NodeDiagnostics(HSHM_MCTX, query);
```

### PoolQuery Usage Guidelines

#### Best Practices

1. **Never use null queries**: Always specify an explicit PoolQuery type
2. **Default to Local**: When in doubt, use `PoolQuery::Local()`
3. **Consider locality**: Prefer local execution to minimize network overhead
4. **Use appropriate granularity**: Match routing mode to operation scope

#### Common Patterns

**Client Creation Pattern**:
```cpp
// Always use PoolQuery::Local() for container creation
client.Create(HSHM_MCTX, chi::PoolQuery::Local());
```

**Load-Balanced Operations**:
```cpp
// Use hash-based routing for even distribution
for (const auto& item : items) {
  u32 hash = ComputeHash(item.id);
  auto query = chi::PoolQuery::DirectHash(hash);
  client.Process(HSHM_MCTX, query, item);
}
```

**Batch Processing**:
```cpp
// Process containers in chunks
const u32 total_containers = pool_info->num_containers_;
const u32 batch_size = 10;
for (u32 offset = 0; offset < total_containers; offset += batch_size) {
  u32 count = std::min(batch_size, total_containers - offset);
  auto query = chi::PoolQuery::Range(offset, count);
  client.BatchProcess(HSHM_MCTX, query, batch_data);
}
```

### Runtime Routing Implementation

The runtime uses PoolQuery to determine task routing through several stages:

1. **Query Validation**: Ensures the query parameters are valid
2. **Container Resolution**: Maps query to specific container(s)
3. **Task Distribution**: Routes task to appropriate worker queues
4. **Load Balancing**: Applies distribution strategies for multi-container queries

### PoolQuery in Task Definitions

Tasks must include PoolQuery in their constructors:

```cpp
class CustomTask : public chi::Task {
 public:
  CustomTask(hipc::Allocator *alloc,
             const chi::TaskNode &task_node,
             const chi::PoolId &pool_id,
             const chi::PoolQuery &pool_query,  // Required parameter
             /* custom parameters */)
      : chi::Task(alloc, task_node, pool_id, pool_query, method_id) {
    // Task initialization
  }
};
```

### Advanced PoolQuery Features

#### Query Introspection
```cpp
PoolQuery query = PoolQuery::Range(0, 10);

// Check routing mode
if (query.IsRangeMode()) {
  u32 offset = query.GetRangeOffset();
  u32 count = query.GetRangeCount();
  // Process range parameters
}

// Get routing mode enum
RoutingMode mode = query.GetRoutingMode();
switch (mode) {
  case RoutingMode::Local:
    // Handle local routing
    break;
  case RoutingMode::Broadcast:
    // Handle broadcast
    break;
  // ... other cases
}
```

#### Combining with Task Priorities
```cpp
// High-priority broadcast
auto query = chi::PoolQuery::Broadcast();
auto task = ipc_manager->NewTask<UpdateTask>(
    chi::CreateTaskNode(),
    pool_id,
    query,
    update_data
);
ipc_manager->Enqueue(task, chi::kHighPriority);
```

### Troubleshooting PoolQuery Issues

**Common Errors**:

1. **Null Query Error**: "NEVER use a null pool query"
   - Solution: Always use a factory method like `PoolQuery::Local()`

2. **Invalid Container ID**: Container not found for DirectId query
   - Solution: Verify container exists before using DirectId

3. **Range Out of Bounds**: Range exceeds available containers
   - Solution: Check pool size before creating Range queries

4. **Node ID Invalid**: Physical node ID doesn't exist
   - Solution: Validate node IDs against cluster configuration

## Client-Server Communication

### Client Implementation Patterns

#### Critical Pool ID Update Pattern

**IMPORTANT**: All ChiMod clients that implement Create methods MUST update their `pool_id_` field with the actual pool ID returned from completed CreateTask operations. This is essential because:

1. CreateTask operations may return a different pool ID than initially specified
2. Pool creation may reuse existing pools with different IDs
3. Subsequent client operations depend on the correct pool ID

**Required Pattern for All Client Create Methods:**

```cpp
void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query, ...) {
    auto task = AsyncCreate(mctx, pool_query, ...);
    task->Wait();
    
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;
    
    CHI_IPC->DelTask(task);
}
```

**Why This Is Required:**

- **Pool Reuse**: CreateTask is actually a GetOrCreatePoolTask that may return an existing pool
- **ID Assignment**: The admin ChiMod may assign a different pool ID than requested
- **Client Consistency**: All subsequent operations must use the correct pool ID
- **Distributed Operation**: Pool IDs must be consistent across all client instances

**Examples of Correct Implementation:**

```cpp
// Admin client Create method
void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query) {
    auto task = AsyncCreate(mctx, pool_query);
    task->Wait();

    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;

    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(task);
}

// Bdev client Create method (with parameters)
void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
            BdevType bdev_type, const std::string& file_path, 
            chi::u64 total_size, chi::u32 io_depth, chi::u32 alignment) {
    auto task = AsyncCreate(mctx, pool_query, bdev_type, file_path, 
                           total_size, io_depth, alignment);
    task->Wait();
    
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;
    
    CHI_IPC->DelTask(task);
}
```

**Common Mistakes to Avoid:**

- ❌ **Forgetting to update pool_id_**: Leads to incorrect pool ID for subsequent operations
- ❌ **Using original pool_id_**: The task may return a different pool ID than initially specified
- ❌ **Updating before task completion**: Always wait for task completion before reading new_pool_id_
- ❌ **Not implementing this pattern**: All synchronous Create methods must follow this pattern

This pattern is mandatory for all ChiMod clients and ensures correct pool ID management throughout the client lifecycle.

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
chi::string my_string(ctx_alloc, "initial value");

// Allocate vector
chi::vector<u32> my_vector(ctx_alloc);
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
    HSHM_MCTX,
    chi::TaskNode(0),
    pool_id_,
    pool_query,
    input_data,
    operation_id);

// Client side - cleanup (after task completion)
ipc_manager->DelTask(task, chi::kMainSegment);

// Runtime side - automatic cleanup (no code needed)
// Framework Del dispatcher calls ipc_manager->DelTask() automatically
```

### CHI_IPC Buffer Allocation

The `CHI_IPC` singleton provides centralized buffer allocation for shared memory operations in client code. Use this for allocating temporary buffers that need to be shared between client and runtime processes.

**Important**: `AllocateBuffer` is a template function that returns `hipc::FullPtr<T>`, not `hipc::Pointer`. You must specify the template type parameter when calling it.

#### Basic Usage
```cpp
#include <chimaera/chimaera.h>

// Get the IPC manager singleton
auto* ipc_manager = CHI_IPC;

// Allocate a buffer in shared memory (returns FullPtr<T>, not hipc::Pointer)
size_t buffer_size = 1024;
hipc::FullPtr<void> buffer_ptr = ipc_manager->AllocateBuffer<void>(buffer_size);

// Use the buffer (example: copy data into it)
void* buffer_data = buffer_ptr.ptr_;
memcpy(buffer_data, source_data, data_size);

// Alternative: Allocate typed buffer
hipc::FullPtr<char> char_buffer = ipc_manager->AllocateBuffer<char>(buffer_size);
strncpy(char_buffer.ptr_, "example data", buffer_size);

// The buffer will be automatically freed when buffer_ptr goes out of scope
// or when explicitly deallocated by the framework
```

#### Use Cases for CHI_IPC Buffers
- **Temporary data transfer**: When passing large data to tasks
- **Intermediate storage**: For computations that need shared memory
- **I/O operations**: Reading/writing data that needs to be accessible by runtime

#### Best Practices
```cpp
// ✅ Good: Use CHI_IPC for temporary shared buffers
auto* ipc_manager = CHI_IPC;
hipc::FullPtr<void> temp_buffer = ipc_manager->AllocateBuffer<void>(data_size);

// ✅ Good: Use chi::ipc types for persistent task data
chi::ipc::string task_string(ctx_alloc, "persistent data");

// ❌ Avoid: Don't use CHI_IPC for small, simple task parameters
// Use chi::ipc types directly in task definitions instead
```

### Shared-Memory Compatible Data Structures

For task definitions and any data that needs to be shared between client and runtime processes, always use shared-memory compatible types instead of standard C++ containers.

#### chi::ipc::string
Use `chi::ipc::string` or `hipc::string` instead of `std::string` in task definitions:

```cpp
#include <chimaera/types.h>

// Task definition using shared-memory string
struct CustomTask : public chi::Task {
  INOUT hipc::string input_data_;     // Shared-memory compatible string
  INOUT hipc::string output_data_;    // Results stored in shared memory
  
  CustomTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
             const std::string& input) 
    : input_data_(alloc, input),      // Initialize from std::string
      output_data_(alloc) {}          // Empty initialization
      
  // Conversion to std::string when needed
  std::string getResult() const {
    return std::string(output_data_.data(), output_data_.size());
  }
};
```

#### chi::ipc::vector
Use `chi::ipc::vector` instead of `std::vector` for arrays in task definitions:

```cpp
// Task definition using shared-memory vector
struct ProcessArrayTask : public chi::Task {
  INOUT chi::ipc::vector<chi::u32> data_array_;
  INOUT chi::ipc::vector<chi::f32> result_array_;
  
  ProcessArrayTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T>& alloc,
                   const std::vector<chi::u32>& input_data)
    : data_array_(alloc),
      result_array_(alloc) {
    // Copy from std::vector to shared-memory vector
    data_array_.resize(input_data.size());
    std::copy(input_data.begin(), input_data.end(), data_array_.begin());
  }
};
```

#### When to Use Each Type

**Use shared-memory types (chi::ipc::string, hipc::string, chi::ipc::vector, etc.) for:**
- Task input/output parameters
- Data that persists across task execution
- Any data structure that needs serialization
- Data shared between client and runtime

**Use std::string/vector for:**
- Local variables in client code
- Temporary computations
- Converting to/from shared-memory types

#### Type Conversion Examples
```cpp
// Converting between std::string and shared-memory string types
std::string std_str = "example data";
hipc::string shm_str(ctx_alloc, std_str);          // std -> shared memory
std::string result = std::string(shm_str);         // shared memory -> std

// Converting between std::vector and shared-memory vector types
std::vector<int> std_vec = {1, 2, 3, 4, 5};
chi::ipc::vector<int> shm_vec(ctx_alloc);
shm_vec.assign(std_vec.begin(), std_vec.end());    // std -> shared memory

std::vector<int> result_vec(shm_vec.begin(), shm_vec.end());  // shared memory -> std
```

#### Serialization Support
Both `chi::ipc::string` and `chi::ipc::vector` automatically support serialization for task communication:

```cpp
// Task definition - no manual serialization needed
struct SerializableTask : public chi::Task {
  INOUT hipc::string message_;
  INOUT chi::ipc::vector<chi::u64> timestamps_;
  
  // Cereal automatically handles chi::ipc types
  template<class Archive>
  void serialize(Archive& ar) {
    ar(message_, timestamps_);  // Works automatically
  }
};
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

**IMPORTANT:** The `chimod_lib_name` should **NOT** include the `_runtime` suffix. The module manager automatically appends `_runtime` when loading the library. For example, use `"chimaera_mymodule"` not `"chimaera_mymodule_runtime"`.

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

## Auto-Generated Code Pattern

### Overview

ChiMods use auto-generated source files to implement the Container virtual APIs (Run, Monitor, Del, SaveIn, LoadIn, SaveOut, LoadOut, NewCopy). This approach provides consistent dispatch logic and reduces boilerplate code.

### New Pattern: Auto-Generated Source Files

Instead of using inline functions in headers, ChiMods now use auto-generated `.cc` source files that implement the virtual methods directly. This pattern:

- **Eliminates inline dispatchers**: Virtual methods are implemented directly in auto-generated source
- **Reduces header dependencies**: No need to include autogen headers in runtime files
- **Improves compilation**: Source files compile once, not in every including file
- **Maintains consistency**: All ChiMods use the same dispatch pattern

### File Structure

```
src/
└── autogen/
    └── MOD_NAME_lib_exec.cc    # Auto-generated virtual method implementations
```

The auto-generated source file contains:
- Container virtual method implementations (Run, Monitor, Del, etc.)
- Switch-case dispatch based on method IDs
- Proper task type casting and method invocation
- IPC manager integration for task lifecycle management

### Auto-Generated Source Template

```cpp
/**
 * Auto-generated execution implementation for MOD_NAME ChiMod
 * Implements Container virtual APIs directly using switch-case dispatch
 * 
 * This file is autogenerated - do not edit manually.
 * Changes should be made to the autogen tool or the YAML configuration.
 */

#include <chimaera/MOD_NAME/MOD_NAME_runtime.h>
#include <chimaera/MOD_NAME/autogen/MOD_NAME_methods.h>
#include <chimaera/chimaera.h>

namespace chimaera::MOD_NAME {

//==============================================================================
// Runtime Virtual API Implementations
//==============================================================================

void Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      Create(task_ptr.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      Destroy(task_ptr.Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kCustom: {
      Custom(task_ptr.Cast<CustomTask>(), rctx);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

void Runtime::Monitor(chi::MonitorModeId mode, chi::u32 method, 
                       hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  switch (method) {
    case Method::kCreate: {
      MonitorCreate(mode, task_ptr.Cast<CreateTask>(), rctx);
      break;
    }
    case Method::kDestroy: {
      MonitorDestroy(mode, task_ptr.Cast<DestroyTask>(), rctx);
      break;
    }
    case Method::kCustom: {
      MonitorCustom(mode, task_ptr.Cast<CustomTask>(), rctx);
      break;
    }
    default: {
      // Unknown method - do nothing
      break;
    }
  }
}

void Runtime::Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) {
  // Use IPC manager to deallocate task from shared memory
  auto* ipc_manager = CHI_IPC;
  
  switch (method) {
    case Method::kCreate: {
      ipc_manager->DelTask(task_ptr.Cast<CreateTask>());
      break;
    }
    case Method::kDestroy: {
      ipc_manager->DelTask(task_ptr.Cast<DestroyTask>());
      break;
    }
    case Method::kCustom: {
      ipc_manager->DelTask(task_ptr.Cast<CustomTask>());
      break;
    }
    default: {
      // For unknown methods, still try to delete from main segment
      ipc_manager->DelTask(task_ptr);
      break;
    }
  }
}

// SaveIn, LoadIn, SaveOut, LoadOut, and NewCopy follow similar patterns...

} // namespace chimaera::MOD_NAME
```

### Runtime Implementation Changes

With the new autogen pattern, runtime source files (`MOD_NAME_runtime.cc`) no longer include autogen headers or implement dispatcher methods:

#### Before (Old Pattern):
```cpp
// No autogen header includes needed with new pattern

void Runtime::Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  // Dispatch to the appropriate method handler
  chimaera::MOD_NAME::Run(this, method, task_ptr, rctx);
}

void Runtime::Monitor(chi::MonitorModeId mode, chi::u32 method, 
                     hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) {
  // Dispatch to the appropriate monitor handler
  chimaera::MOD_NAME::Monitor(this, mode, method, task_ptr, rctx);
}

// Similar dispatcher implementations for Del, SaveIn, LoadIn, SaveOut, LoadOut, NewCopy...
```

#### After (New Pattern):
```cpp
// No autogen header includes needed
// No dispatcher method implementations needed

// Virtual method implementations are now in src/autogen/MOD_NAME_lib_exec.cc
// Runtime source focuses only on business logic methods like Create(), Custom(), etc.
```

### CMake Integration

The auto-generated source file must be included in the `RUNTIME_SOURCES`:

```cmake
add_chimod_both(
  CHIMOD_NAME MOD_NAME
  RUNTIME_SOURCES src/MOD_NAME_runtime.cc src/autogen/MOD_NAME_lib_exec.cc
  CLIENT_SOURCES src/MOD_NAME_client.cc
)
```

### Benefits of the New Pattern

1. **Cleaner Runtime Code**: Runtime implementations focus on business logic, not dispatching
2. **Better Compilation**: Source files compile once instead of being inlined in every header include
3. **Consistent Pattern**: All ChiMods use identical dispatch logic
4. **Header Simplification**: No need to include complex autogen headers
5. **Better IDE Support**: Proper source files work better with IDEs than inline templates

### Migration Guide

To migrate from the old inline header pattern to the new source pattern:

1. **Create autogen source directory**: `mkdir -p src/autogen/`
2. **Generate new autogen source**: Create `src/autogen/MOD_NAME_lib_exec.cc` with virtual method implementations
3. **Remove autogen header includes**: Delete `#include "autogen/MOD_NAME_lib_exec.h"` from runtime source (replaced by .cc files)
4. **Remove dispatcher methods**: Delete all virtual method implementations from runtime source (Run, Monitor, Del, etc.)
5. **Update CMakeLists.txt**: Add autogen source to `RUNTIME_SOURCES`
6. **Keep business logic**: Retain the actual task processing methods (Create, Custom, etc.)

### Important Notes

- **Auto-generated files**: These files should be generated by tools, not hand-written
- **Do not edit**: Manual changes to autogen files will be lost when regenerated
- **Template consistency**: All ChiMods should follow the same autogen template pattern
- **Build integration**: Autogen source files must be included in CMake build

## External ChiMod Development

When developing ChiMods in external repositories (outside the main Chimaera project), you need to link against the installed Chimaera libraries and use the CMake package discovery system.

### Prerequisites

Before developing external ChiMods, ensure Chimaera is properly installed:

```bash
# Configure and build Chimaera
cmake --preset debug
cmake --build build

# Install Chimaera libraries and CMake configs  
cmake --install build --prefix /usr/local
```

This installs:
- Core Chimaera library (`libcxx.so`)
- ChiMod libraries (`libchimaera_admin_runtime.so`, `libchimaera_admin_client.so`, etc.)
- CMake package configuration files for external discovery
- Header files for development

### External ChiMod Repository Structure

Your external ChiMod repository should follow this structure:

```
my_external_chimod/
├── chimaera_repo.yaml          # Repository namespace configuration
├── CMakeLists.txt              # Root CMake configuration
├── modules/                    # ChiMod modules directory (name is flexible)
│   └── my_module/
│       ├── chimaera_mod.yaml   # Module configuration
│       ├── CMakeLists.txt      # Module build configuration  
│       ├── include/
│       │   └── chimaera/
│       │       └── my_module/
│       │           ├── my_module_client.h
│       │           ├── my_module_runtime.h
│       │           ├── my_module_tasks.h
│       │           └── autogen/
│       │               └── my_module_methods.h
│       └── src/
│           ├── my_module_client.cc
│           ├── my_module_runtime.cc
│           └── autogen/
│               └── my_module_lib_exec.cc
```

**Note**: The directory name for modules (shown here as `modules/`) is flexible. You can use `chimods/`, `components/`, `plugins/`, or any other name that fits your project structure. The directory name doesn't need to match the namespace.

### Repository Configuration (chimaera_repo.yaml)

Create a `chimaera_repo.yaml` file in your repository root to define the namespace:

```yaml
# Repository-level configuration
namespace: myproject      # Your custom namespace (replaces "chimaera")
```

This namespace will be used for:
- CMake target names: `myproject_my_module_runtime`, `myproject_my_module_client`
- Library file names: `libmyproject_my_module_runtime.so`, `libmyproject_my_module_client.so`
- C++ namespaces: `myproject::my_module`

### Root CMakeLists.txt

Your repository's root `CMakeLists.txt` must find and link to the installed Chimaera packages:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_external_chimod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required Chimaera packages
# These packages are installed by 'cmake --install build --prefix /usr/local'
find_package(chimaera-core REQUIRED)        # Core Chimaera library (libcxx.so)
find_package(chimaera-admin REQUIRED)       # Admin ChiMod (required for all ChiMods)

# Set CMAKE_PREFIX_PATH if Chimaera is installed in a custom location
# set(CMAKE_PREFIX_PATH "/path/to/chimaera/install" ${CMAKE_PREFIX_PATH})

# Include ChimaeraCommon.cmake utilities for add_chimod_both() and install_chimod()
# This file is installed with Chimaera and provides the standard ChiMod build functions
include(ChimaeraCommon)

# Add subdirectories containing your ChiMods
add_subdirectory(modules/my_module)  # Use your actual directory name
```

### ChiMod CMakeLists.txt

Each ChiMod's `CMakeLists.txt` uses the standard Chimaera build utilities:

```cmake
cmake_minimum_required(VERSION 3.20)

# Create both client and runtime libraries using standard Chimaera utilities
# These functions are provided by ChimaeraCommon.cmake (included in root CMakeLists.txt)
add_chimod_both(
  CHIMOD_NAME my_module
  RUNTIME_SOURCES 
    src/my_module_runtime.cc 
    src/autogen/my_module_lib_exec.cc
  CLIENT_SOURCES 
    src/my_module_client.cc
)

# Install the ChiMod libraries
install_chimod(
  CHIMOD_NAME my_module
)

# Optional: Add additional dependencies if your ChiMod needs external libraries
# get_property(RUNTIME_TARGET GLOBAL PROPERTY my_module_RUNTIME_TARGET)
# get_property(CLIENT_TARGET GLOBAL PROPERTY my_module_CLIENT_TARGET)
# target_link_libraries(${RUNTIME_TARGET} PRIVATE some_external_lib)
# target_link_libraries(${CLIENT_TARGET} PRIVATE some_external_lib)
```

### External ChiMod Implementation

Your external ChiMod implementation follows the same patterns as internal ChiMods:

#### CreateParams Configuration

In your `my_module_tasks.h`, the `CreateParams` must reference your custom namespace:

```cpp
struct CreateParams {
  // Your module-specific parameters
  std::string config_data_;
  chi::u32 worker_count_;
  
  // CRITICAL: Library name must match your namespace
  static constexpr const char* chimod_lib_name = "myproject_my_module";
  
  // Constructors and serialization...
};
```

#### C++ Namespace

Use your custom namespace throughout your implementation:

```cpp
// In all header and source files
namespace myproject::my_module {

// Your ChiMod implementation...
class Runtime : public chi::Container {
  // Implementation...
};

class Client : public chi::ContainerClient {  
  // Implementation...
};

} // namespace myproject::my_module
```

### Building External ChiMods

```bash
# Configure your external ChiMod project
mkdir build && cd build
cmake ..

# Build your ChiMods
make

# Optional: Install your ChiMods
make install
```

The build system will automatically:
- Link against `chimaera::cxx` (the core Chimaera library)
- Link against `chimaera::admin_client` and `chimaera::admin_runtime` (required dependencies)
- Generate libraries with your custom namespace: `libmyproject_my_module_runtime.so`
- Configure proper include paths and dependencies

### Usage in Applications

Applications using your external ChiMod would reference it as:

```cpp
#include <chimaera/chimaera.h>
#include <chimaera/my_module/my_module_client.h>
#include <chimaera/admin/admin_client.h>

int main() {
  // Initialize Chimaera client
  chi::CHIMAERA_CLIENT_INIT();
  
  // Create your ChiMod client
  const chi::PoolId pool_id = chi::PoolId(7000, 0);
  myproject::my_module::Client client(pool_id);
  
  // Use your ChiMod
  auto pool_query = chi::PoolQuery::Local();
  client.Create(HSHM_MCTX, pool_query);
}
```

### Dependencies and Installation Paths

External ChiMod development requires these components to be installed:

1. **Core Library**: `chimaera::cxx` (main Chimaera library)
2. **Admin ChiMod**: `chimaera::admin_client` and `chimaera::admin_runtime` (always required)
3. **CMake Configs**: Package discovery files (`.cmake` files)
4. **Headers**: All Chimaera framework headers
5. **ChimaeraCommon.cmake**: Build utilities for `add_chimod_both()` and `install_chimod()`

If Chimaera is installed in a custom location, set `CMAKE_PREFIX_PATH`:

```bash
export CMAKE_PREFIX_PATH="/path/to/chimaera/install:$CMAKE_PREFIX_PATH"
```

### Common External Development Issues

**ChimaeraCommon.cmake Not Found:**
- Ensure Chimaera was installed with `cmake --install build --prefix <path>`
- Verify `CMAKE_PREFIX_PATH` includes the Chimaera installation directory
- Check that `find_package(chimaera-core REQUIRED)` succeeded

**Library Name Mismatch:**
- Ensure `CreateParams::chimod_lib_name` exactly matches your namespace and module name
- For namespace "myproject" and module "my_module": `chimod_lib_name = "myproject_my_module"`
- The system automatically appends "_runtime" to find the runtime library

**Missing Dependencies:**
- Always include `find_package(chimaera-admin REQUIRED)` - required for all ChiMods
- Ensure all external dependencies (Boost, MPI, etc.) are available in your build environment
- Use the same dependency versions that Chimaera was built with

### External ChiMod Checklist

- [ ] **Repository Configuration**: `chimaera_repo.yaml` with custom namespace
- [ ] **CMake Setup**: Root CMakeLists.txt finds `chimaera-core` and `chimaera-admin` packages
- [ ] **ChiMod Configuration**: `chimaera_mod.yaml` with method definitions
- [ ] **Library Name**: `CreateParams::chimod_lib_name` matches namespace pattern
- [ ] **C++ Namespace**: All code uses custom namespace consistently  
- [ ] **Build Integration**: ChiMod CMakeLists.txt uses `add_chimod_both()` and `install_chimod()`
- [ ] **Dependencies**: All required external libraries available at build time

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
3. **Access to Task Data**: Since `Create` receives the CreateTask, you have access to pool_id and pool_query from the task

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
  chi::Container::Init(task->pool_id_, task->pool_query_);
  
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

### CRITICAL: Correct Lane Routing vs Direct Enqueuing

**NEVER directly enqueue tasks in Monitor methods**. The work orchestrator handles actual task enqueuing.

#### ❌ INCORRECT Pattern (Direct Enqueuing):
```cpp
// DON'T DO THIS - bypasses work orchestrator
case chi::MonitorModeId::kLocalSchedule:
  if (auto* lane = GetLane(chi::kLowLatency, 0)) {
    lane->Enqueue(task_ptr.shm_);  // WRONG - direct enqueuing
  }
  break;
```

#### ✅ CORRECT Pattern (Route Lane Setting):
```cpp
// DO THIS - let work orchestrator handle enqueuing
case chi::MonitorModeId::kLocalSchedule:
  // Set route_lane_ to indicate where task should be routed
  {
    auto lane_ptr = GetLaneFullPtr(kProcessingQueue, 0);
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
- Proper routing enables task migration, load balancing, and debugging

### CreateLocalQueue Parameters

The `CreateLocalQueue` method requires three parameters:

```cpp
void CreateLocalQueue(QueueId queue_id, u32 num_lanes, QueuePriority priority);
```

**Parameters:**
- `queue_id`: Semantic constant (e.g., `kMetadataQueue`, never raw integers)
- `num_lanes`: Number of concurrent processing lanes for this queue
- `priority`: Either `chi::kLowLatency` or `chi::kHighLatency`

**Example Usage:**
```cpp
// Create queues with different characteristics
CreateLocalQueue(kMetadataQueue, 1, chi::kHighLatency);    // Single lane for sequential metadata
CreateLocalQueue(kProcessingQueue, 4, chi::kLowLatency);   // 4 lanes for parallel low-latency work
CreateLocalQueue(kBatchQueue, 2, chi::kHighLatency);       // 2 lanes for batch processing
```

## Task Monitoring Requirements

### Mandatory kLocalSchedule Implementation
Every task type must have a corresponding Monitor method that implements `kLocalSchedule`. This is critical for task execution:

```cpp
void MonitorCustom(chi::MonitorModeId mode, 
                  hipc::FullPtr<CustomTask> task_ptr,
                  chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // CORRECT: Set route_lane_ for work orchestrator
      auto lane_ptr = GetLaneFullPtrByHash(kProcessingQueue, task_ptr->operation_id_);
      if (!lane_ptr.IsNull()) {
        rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule:
      // Optional: Global coordination logic
      break;
      
    case chi::MonitorModeId::kEstLoad:
      // Estimate execution time
      rctx.estimated_completion_time_us = task_ptr->operation_id_ * 100.0;
      break;
  }
}
```

### Lane Selection Strategies
When implementing `kLocalSchedule`, choose the appropriate lane selection strategy:

1. **Fixed Lane Assignment**:
```cpp
// Always use lane 0 for simple cases
{
  auto lane_ptr = GetLaneFullPtr(kProcessingQueue, 0);
  if (!lane_ptr.IsNull()) {
    rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
  }
}
```

2. **Hash-Based Load Balancing**:
```cpp
// Distribute based on task data for load balancing
{
  auto lane_ptr = GetLaneFullPtrByHash(kProcessingQueue, task_ptr->operation_id_);
  if (!lane_ptr.IsNull()) {
    rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
  }
}
```

3. **Priority-Based Routing**:
```cpp
// Route to different queues based on task properties
{
  QueueId queue_id = (task_ptr->operation_id_ > 1000) ? 
                     kBatchQueue : kProcessingQueue;
  auto lane_ptr = GetLaneFullPtr(queue_id, 0);
  if (!lane_ptr.IsNull()) {
    rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
  }
}
```

### Common Monitor Implementation Pattern
```cpp
void MonitorTaskType(chi::MonitorModeId mode,
                    hipc::FullPtr<TaskType> task_ptr,
                    chi::RunContext& rctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // STEP 1: Choose appropriate queue and lane based on task properties
      QueueId queue_id = DetermineQueueId(task_ptr);
      LaneId lane_id = DetermineLaneId(task_ptr);
      
      // STEP 2: Get the lane using FullPtr
      auto lane_ptr = GetLaneFullPtr(queue_id, lane_id);
      if (lane_ptr.IsNull()) {
        // Fallback to default lane
        lane_ptr = GetLaneFullPtr(kDefaultQueue, 0);
      }
      
      // STEP 3: Set route_lane_ for work orchestrator
      if (!lane_ptr.IsNull()) {
        rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    
    case chi::MonitorModeId::kGlobalSchedule:
      // Implement global coordination if needed
      break;
      
    case chi::MonitorModeId::kEstLoad:
      // Estimate task execution time
      rctx.estimated_completion_time_us = EstimateExecutionTime(task_ptr);
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
    task->data_ = chi::string(main_allocator_, e.what());
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

## Unit Testing

Unit testing for ChiMods is covered in the separate [Module Test Guide](module_test_guide.md). This guide provides comprehensive information on:

- Test environment setup and configuration
- Environment variables and module discovery
- Test framework integration patterns
- Complete test examples with fixtures
- CMake integration and build setup
- Best practices for ChiMod testing

The test guide demonstrates how to test both runtime and client components in the same process, enabling comprehensive integration testing without complex multi-process coordination.

## Quick Reference Checklist

When creating a new Chimaera module, ensure you have:

### Task Definition Checklist (`_tasks.h`)
- [ ] Tasks inherit from `chi::Task` or use GetOrCreatePoolTask template (recommended for non-admin modules)
- [ ] **Use GetOrCreatePoolTask**: For non-admin modules instead of BaseCreateTask directly
- [ ] **Use BaseCreateTask with IS_ADMIN=true**: Only for admin module
- [ ] SHM constructor with CtxAllocator parameter (if custom task)
- [ ] Emplace constructor with all required parameters (if custom task)
- [ ] Uses HSHM serializable types (chi::string, chi::vector, etc.)
- [ ] Method constant assigned in constructor (e.g., `method_ = Method::kCreate;`)
- [ ] **No static casting**: Use Method namespace constants directly
- [ ] Include auto-generated methods file for Method constants

### Runtime Container Checklist (`_runtime.h/cc`)
- [ ] Inherits from `chi::Container`
- [ ] **InitClient() method implemented** - initializes client for this ChiMod
- [ ] Create() method calls `chi::Container::Init()`
- [ ] Create() method calls `CreateLocalQueue()` with semantic queue IDs, lane counts, and priorities
- [ ] **Queue ID constants defined** - use semantic names like `kMetadataQueue`, not raw integers
- [ ] All task methods use `hipc::FullPtr<TaskType>` parameters
- [ ] **CRITICAL**: Every Monitor method implements `kLocalSchedule` case
- [ ] `kLocalSchedule` calls `GetLaneFullPtr()` to get lane pointer
- [ ] `kLocalSchedule` sets `rctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_)` 
- [ ] **NEVER directly enqueue** - use route_lane_ assignment, let work orchestrator handle enqueuing
- [ ] **NO custom Del methods needed** - framework calls `ipc_manager->DelTask()` automatically
- [ ] Uses `CHI_TASK_CC(ClassName)` macro for entry points

### Client API Checklist (`_client.h/cc`)
- [ ] Inherits from `chi::ContainerClient`
- [ ] Uses `CHI_IPC->NewTask<TaskType>()` for allocation
- [ ] Uses `CHI_IPC->Enqueue()` for task submission
- [ ] Uses `CHI_IPC->DelTask()` for cleanup
- [ ] Provides both sync and async methods
- [ ] **CRITICAL**: Create methods update `pool_id_ = task->new_pool_id_` after task completion

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
- [ ] ❌ **CRITICAL: Direct lane enqueuing** in Monitor methods (bypasses work orchestrator)
- [ ] ❌ **CRITICAL: Not updating pool_id_ in Create methods** (leads to incorrect pool ID for subsequent operations)
- [ ] ❌ Using raw pointers instead of FullPtr in runtime methods
- [ ] ❌ Not calling `chi::Container::Init()` in Create method
- [ ] ❌ Using non-HSHM types in task data members
- [ ] ❌ Forgetting to create local queues in Create method
- [ ] ❌ **Using raw integers for queue IDs** (use semantic constants like `kMetadataQueue`)
- [ ] ❌ **Forgetting InitClient() implementation** (prevents client calls within runtime)
- [ ] ❌ Implementing custom Del methods (framework calls `ipc_manager->DelTask()` automatically)
- [ ] ❌ Writing complex extern "C" blocks (use `CHI_TASK_CC` macro instead)
- [ ] ❌ **Using static_cast with Method values** (use Method::kName directly)
- [ ] ❌ **Missing chimaera.h include** in methods file (GLOBAL_CONST won't work)
- [ ] ❌ **Using enum class for methods** (use namespace with GLOBAL_CONST instead)
- [ ] ❌ **Using BaseCreateTask directly for non-admin modules** (use GetOrCreatePoolTask instead)
- [ ] ❌ **Forgetting GetOrCreatePoolTask template** for container creation (reduces boilerplate)

Remember: **kLocalSchedule is mandatory** - without it, your tasks will never be executed!

## Pool Name Requirements
**CRITICAL**: All ChiMod Create functions MUST require a user-provided `pool_name` parameter. Never auto-generate pool names using `pool_id_` during Create operations.

### Why Pool Names Are Required
1. **pool_id_ Not Available**: `pool_id_` is not set until after Create completes
2. **User Intent**: Users should explicitly name their pools for better organization
3. **Uniqueness**: Users can ensure uniqueness better than auto-generation
4. **Debugging**: Named pools are easier to identify during debugging

### Pool Naming Guidelines
- **Descriptive Names**: Use names that identify purpose or content
- **File-based Devices**: For BDev file devices, `pool_name` serves as the file path
- **RAM-based Devices**: For BDev RAM devices, `pool_name` should be unique identifier
- **Unique Identifiers**: Consider timestamp + PID combinations when needed

### Correct Pool Naming Usage
```cpp
// BDev file-based device - pool_name is the file path
std::string file_path = "/path/to/device.dat";
bdev_client.Create(mctx, pool_query, file_path, chimaera::bdev::BdevType::kFile);

// BDev RAM-based device - pool_name is unique identifier
std::string pool_name = "my_ram_device_" + std::to_string(timestamp);
bdev_client.Create(mctx, pool_query, pool_name, chimaera::bdev::BdevType::kRam, ram_size);

// Other ChiMods - pool_name is descriptive identifier
std::string pool_name = "my_container_" + user_identifier;
mod_client.Create(mctx, pool_query, pool_name);
```

### Incorrect Pool Naming Usage
```cpp
// WRONG: Using pool_id_ before it's set (will be 0 or garbage)
std::string bad_name = "pool_" + std::to_string(pool_id_.ToU64());

// WRONG: Using empty strings
client.Create(mctx, pool_query, "");

// WRONG: Auto-generating inside Create function
// Create functions should not auto-generate names
void Create(mctx, pool_query) {
    std::string auto_name = "pool_" + generate_id();  // Wrong approach
}
```

### Client Interface Pattern
All ChiMod clients should follow this interface pattern:
```cpp
class Client : public chi::ContainerClient {
 public:
  // Synchronous Create with required pool_name
  void Create(const hipc::MemContext& mctx, 
              const chi::PoolQuery& pool_query,
              const std::string& pool_name /* user-provided name */) {
    auto task = AsyncCreate(mctx, pool_query, pool_name);
    task->Wait();
    pool_id_ = task->new_pool_id_;  // Set AFTER Create completes
    // ... cleanup
  }
  
  // Asynchronous Create with required pool_name  
  hipc::FullPtr<CreateTask> AsyncCreate(
      const hipc::MemContext& mctx,
      const chi::PoolQuery& pool_query, 
      const std::string& pool_name /* user-provided name */) {
    // Use pool_name directly, never generate internally
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskNode(),
        chi::kAdminPoolId,  // Always use admin pool
        pool_query,
        CreateParams::chimod_lib_name,  // Never hardcode
        pool_name,  // User-provided name
        pool_id_    // Target pool ID (unset during Create)
    );
    return task;
  }
 
private:
  // Utility for generating unique names when users need it
  static std::string GeneratePoolName(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    pid_t pid = getpid();
    return prefix + "_" + std::to_string(timestamp) + "_" + std::to_string(pid);
  }
};
```

### BDev-Specific Requirements
- **Single Interface**: Use only one `Create()` and `AsyncCreate()` method (no multiple overloads)
- **File Devices**: `pool_name` parameter serves as the file path
- **RAM Devices**: `pool_name` parameter serves as unique identifier
- **Method Signature**: `Create(mctx, pool_query, pool_name, bdev_type, total_size=0, io_depth=32, alignment=4096)`