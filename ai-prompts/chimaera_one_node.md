# Chimaera

Chimaera is a distributed task execution framework. Tasks represent arbitrary C++ functions, similar to RPCs. However, Chimaera aims to implement dynamic load balancing and reorganization to reduce stress. Chimaera's fundamental abstraction are ChiPools and ChiContainers. A ChiPool represents a distributed system (e.g., key-value store), while a ChiContainer represents a subset of the global state (e.g., a bucket). These ChiPools can be communicate to form several I/O paths simultaneously. 

Use google c++ style guide for the implementation. Implement a draft of chimaera. Implement most code in the source files rather than headers. Ensure you document each function in the files you create. Do not make markdown files for this initially, just direct comments in C++. Use the namespace chi:: for all core chimaera types.

Define a macro called CHIMAERA_RUNTIME that can be added at compile-time to separate runtime code from client code.

Put a focus on getting the codebase compiling. Start with building an empty interface. Get the interface compiling. Include variables and constructors, but minimal logic. After this is compiling, move onto implementing the APIs.

## CMake specifiction
Create CMake export targets so that external libraries can include chimaera and build their own chimods. Use RPATH and enable CMAKE_EXPORT_COMPILE_COMMANDS for building all chimaera objects.
Ensure to find hshm and boost.

The root CMakeLists.txt should read environment variables from .env.cmake. This should be enabled/disabled using an option CHIMAERA_ENABLE_CMAKE_DOTENV.

Struct cmakes into at least 5 sections: 
1. options 
2. compiler optimization. Have modes for debug and release. Debug should have no optimization (e.g., -O0).
3. find_package
4. source compilation. E.g., add_subdirectory, etc.
5. install code

## Pools and Domains

Pools represent a group of containers. Containers process tasks. Each container has a unique ID in the pool starting from 0. A SubDomain represents a named subset of containers in the pool. A SubDomainId represents a unique address of the container within the pool. A DomainId represents a unique address of the container in the entire system.  The following SubDomains should be provided: 
```cpp
/** Major identifier of subdomain */
typedef u32 SubDomainGroup;

/** Minor identifier of subdomain */
typedef u32 SubDomainMinor;

namespace SubDomain {
// Maps to an IP address of a node
static GLOBAL_CROSS_CONST SubDomainGroup kPhysicalNode = 0;
// Maps to a logical address global to the entire pool
static GLOBAL_CROSS_CONST SubDomainGroup kGlobal = 1
// Maps to a logical adress local to this node
static GLOBAL_CROSS_CONST SubDomainGroup kLocal = 2;
} // namespace SubDomain
// NOTE: we avoid using a class and static variables for SubDomain for GPU compatability. CUDA does not support static class variables.

struct SubDomainId {
  SubDomainGroup major_; /**< NodeSet, ContainerSet, ... */
  SubDomainMinor minor_; /**< NodeId, ContainerId, ... */
}

/** Represents a scoped domain */
struct DomainId {
  PoolId pool_id_;
  SubDomainId sub_id_;
}
```

A DomainQuery should be implemented that can be used for selecting basic regions of a domain. DomainQuery is not like a SQL query and should focus on being small in size and avoiding strings. DomainQuery has the following options:
1. LocalId(u32 id): Send task to container using its local address
2. GetGlobalId(u32 id): Send task to container using its global address 
3. LocalHash(u32 hash): Hash task to a container by taking modulo of the kLocal subdomain
4. GetGlobalHash(u32 hash): Hash task to a container by taking module of the kGlobal subdomain
5. GetGlobalBcast(): Replicates task to every node in the domain
5. GetDynamic(): Send this request to the container's Monitor method with MonitorMode kGlobalSchedule

Containers can internally create a set of concurrent queues for accepting requests. Queues have an ID. Lanes of these queues will be scheduled within the runtime when they have tasks to execute. The queues will be based on the multi_mpsc_queue data structure of hshm.

## The Base Task

Tasks are used to communicate with containers and pools. Tasks are like RPCs. They contain a DomainQuery to determine which pool and containers to send the task, they contain a method identifier, and any parameters to the method they should execute. There is a base task data structure that all specific tasks inherit from. At minimum, tasks look as follows:
```cpp
/** Decorator macros */
#define IN  // This is an input by the client
#define OUT  // This is output by the runtime
#define INOUT  // This is both an input and output
#define TEMP  // This is internally used by the runtime or client.

/** A container method to execute + parameters */
struct Task : public hipc::ShmContainer {
public:
  IN PoolId pool_id_;        /**< The unique ID of a pool */
  IN TaskNode task_node_;    /**< The unique ID of this task in the graph */
  IN DomainQuery dom_query_; /**< The nodes that the task should run on */
  IN MethodId method_;       /**< The method to call in the container */
  IN ibitfield task_flags_;  /**< Properties of the task */
  IN double period_ns_;      /**< The period of the task */

  void Wait();  // Wait for this task to complete
  void Wait(Task *subtask);  // Wait for a subtask to complete
  template <typename TaskT>
  HSHM_INLINE void Wait(std::vector<FullPtr<TaskT>> &subtasks);  // Wait for subtasks to complete
}
```

Tasks can have the following properties (task_flags_):
1. TASK_PERIODIC: This task will execute periodically. If this is not set, then the task is executed exactly once.
2. TASK_FIRE_AND_FORGET: This task has no return result and should be freed by the runtime upon completion.

TaskNode is the unique ID of a task in a task graph. I.e., if a task spawns a subtask, they should have the same major id, but different minors.
This way, locks can detect if a subtask reacquires the lock, which is used to avoid deadlocking.

Since tasks are stored in shared memory, they should never use virtual functions. Tasks should support serialization.

The serializer should allow many tasks to be serialized in the same buffer. It should keep track of the number of tasks serialized. 

An example task for compression is as follows: 
```cpp 
/** The CompressTask task */
struct CompressTask : public chi::Task {
  /** SHM default constructor */
  explicit CompressTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : chi::Task(alloc) {}

  /** Emplace constructor */
  explicit CompressTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, const chi::DomainQuery &dom_query)
      : chi::Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }
}; 
```

## The Runtime

The runtime implements an intelligent, multi-threaded task execution system. The runtime read the environment variable CHI_SERVER_CONF to see the server configuration yaml file, which stores all configurations for the runtime. There should be a Configration parser that inherits from Hermes SHM's BaseConfig.

Make a default configuration in the config directory. Turn this config into a C++ constant and place into a header file. Use LoadYaml to read the constant and get default values.

### Initialization

Create a new class called Chimaera with methods ClientInit and ServerInit in include/chimaera/chimaera.h. Make a singleton using hshm for this class. Implement the following methods in the created source file: CHIMAERA_CLIENT_INIT and CHIMAERA_RUNTIME_INIT, which both call the singletons.

### Configuration Manager
Make a singleton using hshm for this class. The configuration manager is responsible for parsing the chimaera server YAML file. A singleton should be made so that subsequent classes can access the config data.

### IPC Manager
Make a singleton using hshm for this class. It implements a ClientInit and ServerInit method. The IPC manager should be different for client and runtime. The runtime should create shared memory segments, while clients load the segments.

For ServerInit, when the runtime initially starts, it must spawn a ZeroMQ server using the local loopback address. Use lightbeam from hshm for this. Clients can use this to detect a client on this node is executing and initially connect to the server. 

After this, shared memory backends and allocators over those backends are created. There should be three memory segments:
* main: allocates tasks shared by client and runtime
* client_data: allocates data shared by clients and runtime
* runtime_data: allocates data internally shared by runtime and clients

 The allocator used should be the following compiler macros:
 * ``CHI_MAIN_ALLOC_T``. The default value should be ``hipc::ThreadLocalAllocator``.  
 * ``CHI_CDATA_ALLOC_T``. The default value should be ``hipc::ThreadLocalAllocator``.  
 * ``CHI_RDATA_ALLOC_T``. The default value should be ``hipc::ThreadLocalAllocator``.  

After this, a concurrent, priority queue named the process_queue is stored in the shared memory. This queue is for external processes to submit tasks to the runtime. The number of lanes (i.e., concurrency) is determined by the number of workers. There should be the following priorities: kLowLatency and kHighLatency. The queue lanes are implemented on top of multi_mpsc_queue from hshm. The depth of the queue and is configurable. It does not necessarily need to be a simple typedef.

The chimaera configuration should include an entry for specifying the hostfile. hshm::ConfigParse::ParseHostfile should be used to load the set of hosts. In the runtime, the IPC manager reads this hostfile. It attempts to spawn a ZeroMQ server for each ip address. On the first success, it stops trying. The offset in this list + 1 is the ID of this node.

The IPC manager should expose functions for allocating tasks and freeing them. There should also be 

### Module Manager
The module manager is responsible for dynamically loading all modules on this node. It uses hshm::SharedLibrary for loading shared library symbols. It uses the environment variable LD_LIBRARY_PATH and CHI_REPO_PATH to scan for libraries. It will scan all files in each directory specified and check if they have the entrypoints needed to be a chimaera task. If they do, then they will be loaded and registered. 

ChiMods should have functions to query the name of the chimod and allocate a ChiContainer from the ChiMod. A table should be stored mapping chimod names to their hshm::SharedLibrary.

### Pool Manager
Should maintain the set of ChiPools and ChiContainers on this node. A table should be stored mapping a ChiPool id to the ChiContainers it has on this node. Should be ways to get the chipool name from id quickly, etc. 

### Work Orchestrator
Make a work orchestrator class and singleton. It will spawn a configurable number of worker threads. There four types of worker threads:
1. Low latency: threads that execute only low-latency lanes. This includes lanes from the process queue. 
2. High latency: threads that execute only high-latency lanes.
3. Reinforcement: threads dedicated to the reinforcement of ChiMod performance models
4. Process Wreaper: detects when a process has died and frees its associated memory. For now, do not implement

Use ``HSHM_THREAD_MODEL->Spawn`` for spawning the threads.

When initially spawning the workers, the work orchestrator must also initially map the queues from the IPC Manager to each worker. It maps low-latency lanes to a subset of workers and then high-latency lanes to a different subset of workers. 

### Locking and Synchronization

Implement a custom mutex and reader-writer lock for the chimeara runtime. Modules should use these locks internally as much as possible, instead of std::mutex. Use the TaskNode property to detect if a particular group of tasks are holding the lock. If two tasks with the same major ID attempt to hold the lock, then grant them access. Otherwise, store a table mapping major id to a vector of tasks with the same major. When unlocked, choose a particular major group and then reschedule all tasks in that group.

### Worker
Low-latency and high-latency workers iterate over a set of lanes and execute tasks from those lanes. Workers store an active lane queue and a cold lane queue. The active queue stores the set of lanes to iterate over. The cold queue stores lanes this worker is responsible for, but do not currently have activity. Lanes should have a setting 

When the worker executes a task, it must do the following:
1. Resolve the domain query of the task. I.e., identify the exact set of nodes to distribute the task to. There are a few cases. First, if GetDynamic was used, then get the local container and call the Monitor function using the MonitorMode kGlobalSchedule. This will replace the domain query with something more concrete. Next, if the task does not resolve to kLocal addresses, then send the task to the local remote queue container for scheduling. If the task is local, then get the container to send this task to. Call the Monitor function with the kLocalSchedule MonitorMode to route the task to a specific lane. If the lane was initially empty, then the worker processing it likely will ignore it. 
2. Create a ``RunContext`` for the task, representing all state needed by the runtime for executing the task. This can include timers for periodic scheduling, boost fiber context, and anything else needed that shouldn't be stored in shared memory for the task. 
3. Allocate a stack space (64KB), initiate ``boost::fiber``, and then execute the task. This should be state apart of the ``RunContext``. For synchronization, you should implement custom mutexes and reader-write locks for returning from the coroutine. If the task is waiting for a coroutine lock, it should enter a blocked state and be removed from the active lane. Within the coroutine lock, it should store a set of tasks to unblock using ``hshm::ext_ring_buffer``, which is an extensible ring buffer. Use real mutex to protect access to this data structure. It has the same API as mpsc_queue.
4. If the task is completed (i.e., it returns from the coroutine and is not in a blocked state), free any state in the RunContext, such as the ``boost::fiber`` allocations. If the task is marked TASK_FIRE_AND_FORGET, then use the

If a task is blocked, it is either waiting for another task to complete or is waiting for a mutex. In either case, the task should be rescheduled upon the completion of these events. 

The main goal of these workers is to map tasks to a container and then execute the tasks. Containers should expose functions for scheduling the task locally within this node (to a lane of the container) and globally (to a container in a pool). These functions should not be tasks, rather virtual functions.

## ChiMod Specification

There is a client and a server part to the ChiMod. The server executes in the runtime. The client executes in user processes. Client code should be minimal. Client code essentially allocates tasks and places them in a queue using the IPC Manager. Client code should not perform networking, or any complex logic. Logic should be handled in the runtime code. Runtime objects should include methods for scheduling individual tasks locally (within the lanes) and globally. For each task, there should be a function for executing the task and another function for monitoring the task. For example, a task named CompressTask would have a run function named Compress and a monitor function called MonitorCompress. MonitorCompress should be a switch-case style design, and have different monitoring modes (e.g., kLocalSchedule, kGlobalSchedule).

Each ChiMod should have a function for creating a ChiPool and destroying it. When clients create the chipool, it should store the ID internally. 

Each ChiMod for a project should be located in a single directory called the ChiMod repo. ChiMod repos should have a strict structure that is ideal for code autogeneration. 
For example:
```bash
my_mod_repo
├── chimaera_repo.yaml  # Repo metadata
├── CMakeLists.txt      # Repo cmake
└── mod_name
    ├── chimaera_mod.yaml  # Module metadata, including task names
    ├── CMakeLists.txt     # Module cmake
    ├── autogen
    │   └── mod_name_lib_exec.h  
    │   └── mod_name_methods.h  
    ├── include
    │   └── mod_name
    │       ├── mod_name_client.h      # Client API 
    │       └── mod_name_tasks.h       # Task struct definitions 
    └── src
        ├── CMakeLists.txt          # Builds mod_name_client and runtime  
        ├── mod_name_client.cc    # Client API source
        └── mod_name_runtime.cc   # Runtime API source
```

### Container Server
```cpp
namespace chi {

/**
 * Represents a custom operation to perform.
 * Tasks are independent of Hermes.
 * */
#ifdef CHIMAERA_RUNTIME
class ContainerRuntime {
public:
  PoolId pool_id_;           /**< The unique name of a pool */
  std::string pool_name_;    /**< The unique semantic name of a pool */
  ContainerId container_id_; /**< The logical id of a container */

  /** Create a lane group */
  void CreateLocalQueue(QueueId queue_id, u32 num_lanes, chi::IntFlag flags);

  /** Get lane */
  Lane *GetLane(QueueId queue_id, LaneId lane_id);

  /** Get lane */
  Lane *GetLaneByHash(QueueId queue_id, u32 hash);

  /** Virtual destructor */
  HSHM_DLL virtual ~Module() = default;

  /** Run a method of the task */
  HSHM_DLL virtual void Run(u32 method, Task *task, RunContext &rctx) = 0;

  /** Monitor a method of the task */
  HSHM_DLL virtual void Monitor(MonitorModeId mode, u32 method, hipc::FullPtr<Task> task,
                                RunContext &rctx) = 0;

  /** Delete a task */
  HSHM_DLL virtual void Del(const hipc::MemContext &ctx, u32 method,
                            hipc::FullPtr<Task> task) = 0;
};
#endif // CHIMAERA_RUNTIME
} // namespace chi

extern "C" {
/** Allocate a state (no construction) */
typedef Container *(*alloc_state_t)();
/** New state (with construction) */
typedef Container *(*new_state_t)(const chi::PoolId *pool_id,
                                  const char *pool_name);
/** Get the name of a task */
typedef const char *(*get_module_name_t)(void);
} // extern c

/** Used internally by task source file */
#define CHI_TASK_CC(TRAIT_CLASS, MOD_NAME)                                     \
  extern "C" {                                                                 \
  HSHM_DLL void *alloc_state(const chi::PoolId *pool_id,                       \
                             const char *pool_name) {                          \
    chi::Container *exec =                                                     \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)());    \
    return exec;                                                               \
  }                                                                            \
  HSHM_DLL void *new_state(const chi::PoolId *pool_id,                         \
                           const char *pool_name) {                            \
    chi::Container *exec =                                                     \
        reinterpret_cast<chi::Container *>(new TYPE_UNWRAP(TRAIT_CLASS)());    \
    exec->Init(*pool_id, pool_name);                                           \
    return exec;                                                               \
  }                                                                            \
  HSHM_DLL const char *get_module_name(void) { return MOD_NAME; }              \
  HSHM_DLL bool is_chimaera_task_ = true;                                      \
  }
```

Internally, servers expose a queue stored in private memory. Tasks are routed from the process queue to lanes of the local queue. This routing is done in the Monitor method.
Monitor contains a switch-case statement that can be used to enact different phases of scheduling. Currently, there should be:
* MonitorModeId::kLocalSchedule: Route a task to a lane of the container's queue.

### Container Client
```cpp
namespace chi {

/** Represents the Module client-side */
class ContainerClient {
public:
  PoolId pool_id_; /**< The unique name of a pool */

  template <typename Ar> void serialize(Ar &ar) { ar(pool_id_); }
};
} // namespace chi
```

### Module Repo

Below is an example file tree of a module repo (my_mod_repo) containing one module (mod_name). 
```bash
my_mod_repo
├── chimaera_repo.yaml  # Repo metadata
├── CMakeLists.txt      # Repo cmake
└── mod_name
    ├── chimaera_mod.yaml  # Module metadata, including task names
    ├── CMakeLists.txt     # Module cmake
    ├── autogen
    │   └── mod_name_lib_exec.h  
    │   └── mod_name_methods.h  
    ├── include
    │   └── mod_name
    │       ├── mod_name_client.h      # Client API 
    │       └── mod_name_tasks.h       # Task struct definitions 
    └── src
        ├── CMakeLists.txt          # Builds mod_name_client and runtime  
        ├── mod_name_client.cc    # Client API source
        └── mod_name_runtime.cc   # Runtime API source
```

A module repo should have a namespace. This is used to affect how external libraries link to our targets and how the targets are named. E.g., if namespace "example" is chosen for this repo, the targets that get exported should be something like ``example::mod_name_client`` and "``example::mod_name_runtime``. The namespace should be stored in chimaera_repo.yaml and in the repo cmake. In addition, this namespace is used in the C++ code to make namespace commands. Aliases should be made so these targets can be linked internally in the project's cmake as well. 

Make sure to follow the naming convention [REPO_NAME]:[MOD_NAME] in the modules you build for namespaces.

#### chimeara_mod.yaml
This will include all methods that the container exposes. For example:
```yaml
# Inherited Methods
kCreate: 0        # 0
kDestroy: 1       # 1
kNodeFailure: -1  # 2
kRecover: -1      # 3
kMigrate: -1      # 4
kUpgrade: -1       # 5

# Custom Methods (start from 10)
kCompress: 10
kDecompress: 11
```

Here values of -1 mean that this container should not support those methods.

#### include/mod_name/mod_name_tasks.h
This contains all task struct definitions. For example:
```cpp
namespace example::mod_name {
/** The CompressTask task */
struct CompressTask : public chi::Task {
  /** SHM default constructor */
  explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : chi::Task(alloc) {}

  /** Emplace constructor */
  explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, const chi::DomainQuery &dom_query)
      : chi::Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }
};
}
```

IN, INOUT, and OUT are empty macros used just for helping visualize which parameters are inputs and which are outputs.

Tasks should be compatible with shared memory. Use hipc::strings and vectors for storing information within tasks.

#### include/mod_name/mod_name_client.h and cc
This will expose methods for external programs to send tasks to the chimaera runtime. This includes tasks for creating a pool of this container type.
For example, here is an example client code.
```cpp
namespace example::mod_name {
class Client : public ContainerClient {
  public:
    // Create a pool of mod_name
    void Compress(const hipc::MemContext &mctx,
                const chi::DomainQuery &dom_query) {
        // allocate the Create
        auto *ipc_manager = CHI_IPC_MANAGER;
        hipc::FullPtr<CompressTask> task = AsyncCreate(args)
        task->Wait();
        ipc_manager->DelTask<CompressTask>(task);
    }
    void AsyncCreate(const hipc::MemContext &mctx,
                     const chi::DomainQuery &dom_query) {
        auto *ipc_manager = CHI_IPC_MANAGER;
        FullPtr<CompressTask> task = ipc_manager->NewTask<CompressTask>(mctx, dom_query, create_ctx);
        ipc_manager->Enqueue(task);
    }
}
}
```

#### src/mod_name_client.cc
This is mainly for global variables and constants. Most of the client should be implemented in the header.

#### src/mod_name_runtime.cc
Contains the runtime task processing implementation. E.g.,
```cpp
namespace example::mod_name {
class Runtime : public ContainerRuntime {
 public:
   void Compress(hipc::FullPtr<CompressTask> task, chi::RunContext &rctx) {
     // Compress data
   }
   void MonitorCompress(chi::MonitorModeId mode, hipc::FullPtr<CompressTask> task, chi::RunContext &rctx) {
     switch (mode) {
        
     }
   }
}
}

CHI_TASK_CC(mod_name);
```

#### autogen/mod_name_lib_exec.h
A switch-case lambda function for every implemented method. This should be autogenerated based on the
methods.yaml file.
```cpp
namespace example::mod_name { 
void Run(Method method, hipc::FullPtr<Task> task, chi::RunContext &rctx) {
    switch (method) {
        case Method::kCreate: {
            Create(hipc::FullPtr<CreateTask>(task), rctx);
        }
        case Method::kCompress: {
            Compress(hipc::FullPtr<CompressTask>(task), rctx)
        }
    }
}

// Similar switch-case for other override functions
}
```

#### autogen/mod_name_methods.h

Defines the set of methods the module implements in C++. This should be autogenerated from the methods.yaml file.
```cpp
namespace example::mod_name {
class Method {
  CLS_CONST int kCreate = 1,
  CLS_CONST int kCompress = 10;
  CLS_CONST int kDecompress = 11;
}
}
```

### Admin ChiMod

This is a special chimod that must be loaded for the chimaera runtime to fully function. This chimod is responsible for creating chipools, destroying them, and stopping the runtime. Processes initially send tasks containing the parameters to the chimod they want to instantiate to the admin chimod, which then distributes the chipool. It should use the PoolManager singleton to create containers locally. The chimod has three main tasks:
1. CreatePool
2. DestroyPool
3. StopRuntime

When creating a container, a table should be built mapping DomainIds to either node ids or other DomainIds. These are referred to as domain tables. These tables should be stored as part of the pool metadata in PoolInfo. Two domains should be stored: kLocal and kGlobal. Local domain maps containers on this node to the global DomainId. Global maps DomainId to physical DomainIds, representing node Ids. The global domain table should be consistent across all nodes. 

## Utilities

Implement an executable to launch and stop the runtime: chimeara_start_runtime and chimaera_stop_runtime.
