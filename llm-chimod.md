
# The IOWarp Runtime: Chimaera

Chimaera is a flexible runtime system for building and executing data-intensive tasks.
Chimaera defines a set of abstractions that make the development of active,
reliable, upgradeable and malleable I/O stacks easier. The core abstractions are the
ChiPool and ChiContainer. A **ChiPool** represents a set of ChiContainers. Each individual
**ChiContainer** represents a subset of data and the computations that can happen to that
data. ChiContainers can be migrated and are addressed using an unchanging, machine-independent
DomainId. For example, a ChiPool could represent a distributed key-value store while a ChiContainer
represents a bucket in that key-value store.

# Client API Example

Below is an example of how someone would use an existing module in their code.
In this case, the module is named ``small_message`` and exposes an api named ``Md``.
The small_message ChiPool, when created, stores an integer (by default) in each ChiContainer.
``Md`` reads that integer and returns it to the user.

```cpp
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "small_message/small_message_client.h"

CHI_NAMESPACE_INIT

int main() {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  CHI_ADMIN->RegisterModule(HSHM_DEFAULT_MEM_CTX,
                            chi::DomainQuery::GetGlobalBcast(),
                            "small_message");
  chi::small_message::Client client;
  client.Create(
      HSHM_DEFAULT_MEM_CTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  hshm::Timer t;
  size_t ops = 256;
  HILOG(kInfo, "OPS: {}", ops);
  t.Resume();
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    int cont_id = i;
    int ret = client.Md(HSHM_DEFAULT_MEM_CTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomainId::kGlobalContainers, cont_id),
                        depth, 0);
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks", ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}
```

## Headers and Macros

The first few lines relate to the includes.
```cpp
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "small_message/small_message_client.h"

CHI_NAMESPACE_INIT
```

* ``chimaera/api/chimaera_client.h`` includes all code for
connecting to the runtime
* ``chimaera_admin/chimaera_admin.h`` includes all APIs for
registering modules to the runtime
* ``small_message/small_message.h`` is the small_message module
* ``CHI_NAMESPACE_INIT`` is a macro that makes various typedefs
to avoid having the same "using chi::*" preambles everywhere

## Initializing Connection

To initialize the connection to the client, the main function first calls:
```cpp
CHIMAERA_CLIENT_INIT();
```

This function will connect to a shared-memory segment between this process
and the runtime to allow tasks to be scheduled.

## RegisterModule

```cpp
CHI_ADMIN->RegisterModule(HSHM_DEFAULT_MEM_CTX,
                          chi::DomainQuery::GetGlobalBcast(),
                          "small_message");
```

This line of code will register the "small_message" module on
every node. Registering a module means searching for the
shared object named "small_message". 

Before the runtime is spawned, the "small_message" object needs
to be in the runtime's search path. This could be in the variables
PATH or LD_LIBRARY_PATH.

## Create the Small Message ChiPool

```cpp
chi::small_message::Client client;
client.Create(
    HSHM_DEFAULT_MEM_CTX,
    chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
    chi::DomainQuery::GetGlobalBcast(), "ipc_test");
```

``Create`` will create a ChiPool. This pool will span all nodes 
(``chi::DomainQuery::GetGlobalBcast()``) and will
be registered first by Chimaera Admin's first container 
(``chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0)``).
By default, there will be one container per node in the provided domain.
In this case, the set of all nodes.

## Send the Metadata Task

```cpp
  hshm::Timer t;
  size_t ops = 256;
  HILOG(kInfo, "OPS: {}", ops);
  t.Resume();
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    int cont_id = i;
    int ret = client.Md(HSHM_DEFAULT_MEM_CTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomainId::kGlobalContainers, cont_id),
                        depth, 0);
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks", ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
```

In a loop, ``client.Md`` will construct an MdTask object and schedule
to the container ``cont_id`` in the Small Message ChiPool. At the
end, the overall performance is reported.

# Building Modules

Chimaera's objective is to be flexible towards a variety of I/O stack designs. The main step
to achieve this is through modularity. It is possible to create and dynamically register
new custom modules in the chimaera runtime. This section discusses how this modularity is
achieved and what can be done with it.

We will explain this process by adding an example compression module to a custom repo.
Chimaera does follow a strict naming convention to allow for more
code to be generated. We document here the expectations on certain key variable and class
names to ensure that code is properly generated.

## Module Repos
In Chimaera, a module (or **ChiMod**) is the code object representing a ChiContainer. These
modules can be registered dynamically in the runtime. A module repo represents a set of 
ChiMods. In the example below, we show the set of core modules defined
by Chimaera. These include modules for block devices (bdev), networking (remote_queue), 
work orchestration policies (worch_proc_round_robin and worch_queue_round_robin), and lastly
the Chimaera Admin, which is responsible for general runtime tasks (e.g., upgrading modules, 
creating new ChiPools, etc.).

In the tree output below, the folder named "tasks" is the module repo.
```bash
chimaera
├── benchmark
├── build
├── src
├── tasks
    ├── bdev
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    ├── chimaera_admin
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    ├── CMakeLists.txt
    ├── remote_queue
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    ├── small_message
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    ├── TASK_NAME
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    ├── worch_proc_round_robin
    │   ├── CMakeLists.txt
    │   ├── include
    │   └── src
    └── worch_queue_round_robin
        ├── CMakeLists.txt
        ├── include
        └── src
```

Module repos can be attached as a subdirectory to a project -- or it can
be the entire project itself. In the above example, we demonstrate a case
where the repo is apart of a broader project.

A module repo can be created as follows:
```bash
chi_make_repo /path/to/repo namespace
```

For example:
```bash
chi_make_repo ~/my_mod_repo example
```

The namespace is used to help autogenerate cmakes. When you build your project,
other projects will be able to link to it using "namespace::my_mod_name". 

At this time, the repo should look as follows:
```bash
my_mod_repo
├── chimaera_repo.yaml
└── CMakeLists.txt
```

* ``chimaera_repo.yaml``: Recommended against touching directly. Indicates
this is a module repo and provides some metadata (mainly the namespace).
* ``CMakeLists.txt``: The cmake that will be used to build the modules
in the repo, when we make them.

## Bootstrap a New Module in a Repo

To create a module, use the ``chi_make_mod`` command. This is installed
as part of the library chimaera-util, which is a dependency of iowarp.
Below is an example where we create the compressor module within the
my_mod_repo repo.

```bash
chi_make_mod ~/my_mod_repo/compressor
```

In this case, ``chi_make_mod`` copy-pastes the TASK_NAME module from chimaera
and renames it (and associated classes / namespaces) to be compressor. The 
path does not need to be absolute.

This will create a module with the following directory structure:
```bash
my_mod_repo
├── chimaera_repo.yaml  # Repo metadata
├── CMakeLists.txt      # Repo cmake
└── compressor
    ├── chimaera_mod.yaml  # Module metadata
    ├── CMakeLists.txt     # Module cmake
    ├── include
    │   └── compressor
    │       ├── compressor_client.h      # Client API
    │       ├── compressor_lib_exec.h    # (autogenerated from *methods.yaml)
    │       ├── compressor_methods.h     # (autogenerated from *methods.yaml)
    │       ├── compressor_methods.yaml  # Task declarations 
    │       └── compressor_tasks.h       # Task struct definitions 
    └── src
        ├── CMakeLists.txt          # Builds compressor_client and runtime  
        ├── compressor_client.cc    # Client API source
        ├── compressor_monitor.py   # Used for monitoring
        └── compressor_runtime.cc   # Runtime API source
```

### Refresh the module repo
To ensure that the repo's cmakes all know of the new module, run:
```bash
chi_refresh_repo ~/my_mod_repo
```

### Try Compiling
You should be able to compile this code as-is.

```bash
cd ~/my_mod_repo
mkdir build
cd build
cmake ..
make -j32
```

## Declare all tasks

The first file to consider is the ``include/compressor/compressor_methods.yaml``. This file declares 
the set of tasks that your module exposes by giving each task a unique
identifier. By unique, we mean that each task declared within this file should
be distinct. In other words, the ID does not need to be unique across modules.

Your ``*_methods.yaml`` file will initially look as follows:
```yaml
# Inherited Methods
kCreate: 0        # 0
kDestroy: 1       # 1
kNodeFailure: -1  # 2
kRecover: -1      # 3
kMigrate: -1      # 4
kUpgrade: -1       # 5

# Custom Methods (start from 10)
# kCustom: 10

# NOTE: When you add a new method, 
# call chi_refresh_repo to update
# all autogenerated files.
```

Inherited methods are common across all modules. Of these,
only kCreate is strictly required. The others are optional,
which is indicated by setting -1 to avoid code generation for them.

Custom methods can range from 10 to 2^32. Let's add the methods
kCompress and kDecompress.

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

## Autogenerate task helper files

To autogenerate the Compress and Decompress functions, we can run the following command:
```bash
chi_refresh_repo ~/my_mod_repo
```

This will edit various files to bootstrap code for you.

### Autogenerated tasks structs

A task is simply a C++ struct containing the parameters to a function.
Tasks are located in ``include/compressor/compressor_tasks.h``.

```cpp
CHI_BEGIN(Create)
/** A task to create compressor */
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "example_compressor";

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc) {}

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {}
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;
CHI_END(Create)

CHI_BEGIN(Destroy)
/** A task to destroy compressor */
typedef chi::Admin::DestroyContainerTask DestroyTask;
CHI_END(Destroy)

CHI_BEGIN(Compress)
/** The CompressTask task */
struct CompressTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }

  /** Duplicate message */
  void CopyStart(const CompressTask &other, bool deep) {}

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {}

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {}
};
CHI_END(Compress)

CHI_BEGIN(Decompress)
/** The DecompresssTask task */
struct DecompresssTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_INLINE explicit DecompresssTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit DecompresssTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kDecompresss;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }

  /** Duplicate message */
  void CopyStart(const DecompresssTask &other, bool deep) {}

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {}

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {}
};
CHI_END(Decompress)

CHI_AUTOGEN_METHODS  // keep at class bottom
```

#### CHI_BEGIN, CHI_END, CHI_AUTOGEN_METHODS
Notice that each task is wrapped in CHI_BEGIN and CHI_END. 
For example, Decompress:
```cpp
CHI_BEGIN(Decompress)
/** The DecompresssTask task */
struct DecompresssTask : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_INLINE explicit DecompresssTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit DecompresssTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kDecompresss;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }

  /** Duplicate message */
  void CopyStart(const DecompresssTask &other, bool deep) {}

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {}

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {}
};
CHI_END(Decompress)
```

``CHI_BEGIN`` and ``CHI_END`` encapsulate a piece of code
relating to a task. ``CHI_AUTOGEN_METHODS`` marks the end
of the class where methods not wrapped in these could be
appended. ``CHI_AUTOGEN_METHODS`` is mainly
a fallback and the other decorators are prioritized. 

These exist to allow the Chimaera utility library to parse
these files and make edits to them. For example, a task
could be deleted by removing everything between ``CHI_BEGIN`` and
``CHI_END``.

### Autogenerated client APIs

The next file to look at is your client API: ``include/compressor/compressor_client.h``.
For any application that will interact with your module, this is the API that
they will use.
```cpp
/** Create compressor requests */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() = default;

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  CHI_BEGIN(Create)
  /** Create a pool */
  HSHM_INLINE_CROSS_FUN
  void Create(const hipc::MemContext &mctx, const DomainQuery &dom_query,
              const DomainQuery &affinity, const chi::string &pool_name,
              const CreateContext &ctx = CreateContext()) {
    FullPtr<CreateTask> task =
        AsyncCreate(mctx, dom_query, affinity, pool_name, ctx);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);
  CHI_END(Create)

  CHI_BEGIN(Destroy)
  /** Destroy pool + queue */
  HSHM_INLINE_CROSS_FUN
  void Destroy(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    CHI_ADMIN->DestroyContainer(mctx, dom_query, id_);
  }
  CHI_END(Destroy)

  CHI_BEGIN(Compress)
  /** Compress task */
  void Compress(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    FullPtr<CompressTask> task = AsyncCompress(mctx, dom_query);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Compress);
  CHI_END(Compress)

  CHI_BEGIN(Decompress)
  /** Decompresss task */
  void Decompresss(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    FullPtr<DecompresssTask> task = AsyncDecompresss(mctx, dom_query);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Decompresss);
  CHI_END(Decompress)
};
```

The class name should always be Client. However, you can change the namespace however you
like, so long as it is done consistently across all files in the module repo. This
can be easily done in any text editor.

#### What is CHI_TASK_METHODS?

``CHI_TASK_METHODS`` is a macro that creates several useful APIs. This primarily
includes methods that abstract lower-level Chimaera APIs for allocating and
submitting tasks. 

``CHI_TASK_METHODS(Create)`` makes the following methods:
```cpp
template <typename... Args>   
HSHM_CROSS_FUN hipc::FullPtr<CreateTask> AsyncCreateAlloc(        
    const hipc::MemContext &mctx, const TaskNode &task_node,            
    const DomainQuery &dom_query, Args &&...args) {                     
  hipc::FullPtr<CUSTOM##Task> task = CHI_CLIENT->NewTask<CUSTOM##Task>( 
      mctx, task_node, id_, dom_query, std::forward<Args>(args)...);    
  return task;                                                          
}                                              

template <typename... Args>                                             
HSHM_CROSS_FUN hipc::FullPtr<CreateTask> AsyncCreate(               
    const hipc::MemContext &mctx, Args &&...args) {                     
  return CHI_CLIENT->ScheduleNewTask<CreateTask>(                     
      mctx, id_, std::forward<Args>(args)...);                          
}      

template <typename... Args>                                             
HSHM_CROSS_FUN hipc::FullPtr<CreateTask> AsyncCreateBase(         
    const hipc::MemContext &mctx, chi::Task *parent,                    
    const chi::TaskNode &task_node, Args &&...args) {                   
  return CHI_CLIENT->ScheduleNewTask<CreateTask>(                     
      mctx, parent, task_node, id_, std::forward<Args>(args)...);       
}
```

* ``AsyncCreateAlloc``: Allocates a CreateTask (but does not schedule it).
Typically used more for debugging.
* ``AsyncCreate``: Allocates + schedules a CreateTask. Users will often
call this API.
* ``AsyncCreateBase``: Allocates + Schedules a task, but explicitly states
whether or not the task was spawned as a subtask of another "parent" task.
This API is generally used only internally.

### Autogenerated runtime APIs

The last file to consider is ``src/compressor_runtime.cc``. This file 
contains the code that will be executed by the runtime. It holds
a class named Server. This class should always be named Server.

```cpp
class Server : public Module {
 public:
  CLS_CONST LaneGroupId kDefaultGroup = 0;

 public:
  Server() = default;

  CHI_BEGIN(Create)
  /** Construct compressor */
  void Create(CreateTask *task, RunContext &rctx) {
    // Create a set of lanes for holding tasks
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_LOW_LATENCY);
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}
  CHI_END(Create)

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    // Route tasks to lanes based on their properties
    // E.g., a strongly consistent filesystem could map tasks to a lane
    // by the hash of an absolute filename path.
    return GetLaneByHash(kDefaultGroup, task->prio_, 0);
  }

  CHI_BEGIN(Destroy)
  /** Destroy compressor */
  void Destroy(DestroyTask *task, RunContext &rctx) {}
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }
  CHI_END(Destroy)

  CHI_BEGIN(Compress)
  /** The Compress method */
  void Compress(CompressTask *task, RunContext &rctx) {}
  void MonitorCompress(MonitorModeId mode, CompressTask *task,
                       RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
      }
    }
  }
  CHI_END(Compress)

  CHI_BEGIN(Decompress)
  /** The Decompresss method */
  void Decompresss(DecompresssTask *task, RunContext &rctx) {}
  void MonitorDecompresss(MonitorModeId mode, DecompresssTask *task,
                          RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
      }
    }
  }
  CHI_END(Decompress)

 public:
#include "compressor/compressor_lib_exec.h"
};
```

``compressor/compressor_lib_exec.h`` contains methods for routing tasks to the
methods shown here. For example, a task with the method ``Method::kCompress`` will
be mapped to the function ``Compress`` because of this file.


### Try building
At this point, the code should compile again:
```bash
cd ~/my_mod_repo
cd build
cmake ..
make -j32
```

## Customize your new tasks 

A task is simply a C++ struct containing the parameters to a function.
Tasks are located in ``include/compressor/compressor_tasks.h``.

We will be editing this file now.

### Define the CreateTask

The create task contains the parameters to initialize a ChiPool.
All CreateTasks must provide a constant ``lib_name_``. This is constant is 
the base name of the ChiMod shared object (e.g., *.so or *.dll). This value is automatically
generated and shouldn't be changed. It corresponds to the
``target_link_library`` command in the cmake file that is also autogenerated. 

In our case, we will modify CreateTaskParams to include the identifier of a
compression library. We modify the non-default constructor and the serialization function.
The serialization function in particular is important.
```cpp
struct CreateTaskParams {
  CLS_CONST char *lib_name_ = "example_compressor";
  int compress_id_;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams() = default;

  HSHM_INLINE_CROSS_FUN
  CreateTaskParams(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                   int compress_id = 0) {
    compress_id_ = compress_id;
  }

  template <typename Ar>
  HSHM_INLINE_CROSS_FUN void serialize(Ar &ar) {
    ar(compress_id_);
  }
};
typedef chi::Admin::CreatePoolBaseTask<CreateTaskParams> CreateTask;
```

### Define the CompressTask

The CompressTask has two main inputs: a buffer pointing to data to compress
and the size of the data.

Below, we show our modified ``CompressTask``:
```cpp
struct CompressTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  IN size_t data_size_;

  /** SHM default constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const hipc::Pointer &data, size_t data_size)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
    data_ = data;
    data_size_ = data_size;
  }

  /** Duplicate message */
  void CopyStart(const CompressTask &other, bool deep) {
    data_ = other.data_;
    data_size_ = other.data_size_;
    if (!deep) {
      UnsetDataOwner();
    }
  }

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_WRITE, data_, data_size_);
  }

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {}
};
```

NOTE: Tasks are not directly compatible with virtual functions.
Tasks are generally stored and allocated in shared memory. Since function
addresses are not consistent across processes, using virtual functions,
raw pointers, and STL data structures is generally erronous.

#### Task inheritance
```cpp
struct CompressTask : public Task, TaskFlags<TF_SRL_SYM>
```

CompressTask inherit from two base classes ``Task`` and ``TaskFlags``.
The ``Task`` class contains various variables and helper functions which
will be discussed more later.

``TaskFlags`` is used to enable/disable certain methods of a task. 
In this case, TF_SRL_SYM means "this task can be serialized
and transferred over a network." The alternative is TF_LOCAL,
which means "This task executes on this node only."

When using TF_SRL_SYM, all of these methods are required to be implemented.
When using TF_LOCAL, only CopyStart is required.

#### Task variables
```cpp
IN hipc::Pointer data_;
IN size_t data_size_;
```

Variables can be prefixed with the keywords
IN, OUT, or INOUT. 

* ``IN``: This is an input.
* ``OUT``: This is an output.
* ``INOUT``: This is an input and an output.

These decorators are not required. They are simply
empty macros. However, they help with the readability
of your code. 

In our case, data_ and data_size_ are parameters
the user gives as input.

Why ``hipc::Pointer`` instead of ``char *``? Because
``char *`` refers to an address in the process's address
space. Since Chimaera runs in a different address space
as a separate process, all pointers must be compatible 
with shared memory. This is what ``hipc::Pointer`` does.
This avoids a very expensive data copy.

#### Emplace constructor: Method Signature

The emplace constructor for our CompressTask is below:
```cpp
  /** Emplace constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const hipc::Pointer &data, size_t data_size);
```

The emplace constructor is most important. This constructor
should contain all parameters to intialize the task. Every 
emplace constructor must start with the following 4 variables:

* ``const hipc::CtxAllocator<CHI_ALLOC_T> &alloc``: The allocator used 
to allocate this task. Automatically generated by higher-level APIs.
* ``const TaskNode &task_node``: The unique identifier of this task in the
system. Automatically passed by higher-level APIs.
* ``const PoolId &pool_id``: The ChiPool this task belongs to.
* ``const DomainQuery &dom_query``: The set of ChiContainers in the ChiPool
the task can be sent to. More discussion on domains will be later

We then have our two custom variables that come afterwards:
* ``const hipc::Pointer &data``: The process-independent pointer to some
data.
* ``size_t data_size``: The total size of data.

#### Emplace Constructor: Core Task Parameters
```cpp
  /** Emplace constructor */
  HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const hipc::Pointer &data, size_t data_size)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;
```

Next we look at the basic parameters being set. The main things
that were not described in the previous section are:
* ``prio_``: The priority of this task. Can either be kLowLatency or kHighLatency.
Tasks with different priority are placed on different queues and may be executed 
out-of-order. This is a performance optimization.
* ``method_``: This is the method that will process the task. Remember the ``compress_methods.yaml``
file? A new file called ``compress_methods.h`` was generated from that file, which is where these
methods are defined.
* ``task_flags_``: By default, no flags. There is another section describing the potential flags 
that can go here -- most of which are related to performance.

#### Emplace Constructor: Custom Parameters
```cpp
HSHM_INLINE explicit CompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const hipc::Pointer &data, size_t data_size)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kCompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
    data_ = data;
    data_size_ = data_size;
  }
```

The last part is the custom parameters. Not much magic here, 
just set them.

#### CopyStart

```cpp
  void CopyStart(const CompressTask &other, bool deep) {
    data_ = other.data_;
    data_size_ = other.data_size_;
    if (!deep) {
      UnsetDataOwner();
    }
  }
```

The CopyStart method is used to duplicate a task. It takes 
as input the other task (i.e., the original task) and 
a parameter named "deep", which indicates how much to copy.

In general, deep is false. deep true is generally optional and
will likely be considered legacy / deprecated in the future.
This would only need to be implemented if the module developer 
needed deep copies for their specific situation.

In this example, we assume deep is always false. If deep is
false, we mark this task as "not owning the data". This means
that when the task is eventually freed, the data pointer
will not be destroyed here.

Copies can happen within the networking module of chimaera.
For example, for replication, a task may be duplicated
to each container it is sent to.

#### SerializeStart

```cpp
  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_WRITE, data_, data_size_);
  }
```

This function is called by the remote_queue module of chimaera.
It serializes the custom parameters of the task. 

ar.bulk will serialize the data pointer. DT_WRITE means "I'm writing
data_ of size data_size_ to the remote host".

#### SerializeEnd
```cpp
  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
```
This function is called when the task completes on the remote node. 
In this case there are no return values, so this is just empty.

### Define the DecompressTask

```cpp
struct DecompressTask : public Task, TaskFlags<TF_SRL_SYM> {
  IN hipc::Pointer data_;
  INOUT size_t data_size_;

  /** SHM default constructor */
  HSHM_INLINE explicit DecompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE explicit DecompressTask(
      const hipc::CtxAllocator<CHI_ALLOC_T> &alloc, const TaskNode &task_node,
      const PoolId &pool_id, const DomainQuery &dom_query,
      const hipc::Pointer &data, size_t data_size)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrioOpt::kLowLatency;
    pool_ = pool_id;
    method_ = Method::kDecompress;
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
    data_ = data;
    data_size_ = data_size;
  }

  /** Duplicate message */
  void CopyStart(const DecompressTask &other, bool deep) {
    data_ = other.data_;
    data_size_ = other.data_size_;
    if (!deep) {
      UnsetDataOwner();
    }
  }

  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_EXPOSE, data_, data_size_);
  }

  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {
    ar.bulk(DT_WRITE, data_, data_size_);
  }
};
```

The main difference here is the implementations of SerializeStart and SerializeEnd.

#### SerializeStart
```cpp
  /** (De)serialize message call */
  template <typename Ar>
  void SerializeStart(Ar &ar) {
    ar.bulk(DT_EXPOSE, data_, data_size_);
  }
```

In this case, we are only "exposing" the buffer for I/O operations.
This means the remote node will not actually copy the data_ buffer.
It will just have the ability to access that buffer.

#### SerializeEnd
```cpp
  /** (De)serialize message return */
  template <typename Ar>
  void SerializeEnd(Ar &ar) {
    ar.bulk(DT_WRITE, data_, data_size_);
  }
```

When the DecompressTask finishes, it will write the modified
data buffer back to the original host. 

## Define your Client API

The next file to look at is your client API: ``include/compressor/compressor_client.h``.
For any application that will interact with your module, this is the API that
they will use.

### Modify Create
```cpp
  /** Create a pool */
  HSHM_INLINE_CROSS_FUN
  void Create(const hipc::MemContext &mctx, const DomainQuery &dom_query,
              const DomainQuery &affinity, const chi::string &pool_name,
              const CreateContext &ctx = CreateContext(), int compress_id = 0) {
    FullPtr<CreateTask> task =
        AsyncCreate(mctx, dom_query, affinity, pool_name, ctx, compress_id);
    task->Wait();
    Init(task->ctx_.id_);
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(Create);
```

If you recall, the CompressTask takes as input the type of compression to apply.
We add this parameter to the create method and pass as the last. ``AsyncCreate``
internally constructs the ``CreateTaskParams`` struct that we mentioned earlier
in the chapter.

### Modify Compress + Decompress

```cpp
/** Compress task */
void Compress(const hipc::MemContext &mctx, const DomainQuery &dom_query,
              const hipc::Pointer &data, size_t data_size) {
  FullPtr<CompressTask> task =
      AsyncCompress(mctx, dom_query, data, data_size);
  task->Wait();
  CHI_CLIENT->DelTask(mctx, task);
}
CHI_TASK_METHODS(Compress);
```

```cpp
/** Decompress task */
void Decompress(const hipc::MemContext &mctx, const DomainQuery &dom_query,
                const hipc::Pointer &data, size_t data_size) {
  FullPtr<DecompressTask> task =
      AsyncDecompress(mctx, dom_query, data, data_size);
  task->Wait();
  CHI_CLIENT->DelTask(mctx, task);
}
CHI_TASK_METHODS(Decompress);
```

We additionally pass the data and data_size parameters.

## Define your Runtime implementation

The last file to consider is ``src/compressor_runtime.cc``. This file 
contains the code that will be executed by the runtime. It holds
a class named Server. This class should always be named Server.

### Modify Create

```cpp
  /** Construct compressor */
  void Create(CreateTask *task, RunContext &rctx) {
    // Create a set of lanes for holding tasks
    CreateLaneGroup(kDefaultGroup, 1, QUEUE_LOW_LATENCY);
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}
```

This method is used to effectively construct the module. The first line of this
file should always be CreateLaneGroup. Lanes are essentially threads. A LaneGroup
is a set of threads that behave similarly and execute similar types of tasks.
Change the number of lanes to potentially have more concurrency in your module. 

In this case, there is only one lane group -- the default group. There is only 
one lane in the lane group. You can change
this to be separate groups. One situation is different lanes for metadata and data.
Alternatively, different lanes for tasks that run forever vs tasks that execute
and terminate deterministically. Generally, tasks that run forever should always
be mapped to a different lane than those that have a definite end.

### Review MapTaskToLane

This function is what routes a task to a specific lane. In this case, the algorithm
is fairly simple: route to the first lane.

```cpp
  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    // Route tasks to lanes based on their properties
    // E.g., a strongly consistent filesystem could map tasks to a lane
    // by the hash of an absolute filename path.
    return GetLaneByHash(kDefaultGroup, task->prio_, 0);
  }
```

More complex implementations of this could be to track the amount of
work each lane has to do and send tasks to lanes based on their load.

Alternatively, one could send tasks to lanes depending on data dependencies.
For example, buckets of a key-value store could be represented as different
lanes, and tasks for reading and writing from a bucket are stored in the same lane.

### Modify Decompress and Compress

```cpp
/** The Compress method */
  void Compress(CompressTask *task, RunContext &rctx) {
  }
  void MonitorCompress(MonitorModeId mode, CompressTask *task,
                       RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
      }
    }
  }

  /** The Decompress method */
  void Decompress(DecompressTask *task, RunContext &rctx) {
  }
  void MonitorDecompress(MonitorModeId mode, DecompressTask *task,
                         RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kReplicaAgg: {
        std::vector<FullPtr<Task>> &replicas = *rctx.replicas_;
      }
    }
  }
```

### What are the Monitor* methods?

The "Monitor*" functions (e.g., MonitorCompress) are used for handling
events for a specific type of task. 

One of these MonitorModes is ``kReplicaAgg``, which is an event called
by the remote_queue for when a task was replicated -- and we are now
aggregating the results of that into a single task.

For now, we leave this as-is. The current implementation of the function
essentially does nothing.

## Compile + Install

To compile and install the module repo:
```bash
scspkg create my_mod_repo
cd ~/my_mod_repo
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=$(scspkg pkg root my_mod_repo)
make -j32 install
```

## Invoking your mods
In the previous example, we created the compressor mod.

```cpp
#include "compressor/compressor_client.h"

int main() {
  CHIMAERA_CLIENT_INIT();
  chi::compressor::Client client;
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");

  size_t data_size = hshm::Unit<size_t>::Megabytes(1);
  hipc::FullPtr<char> orig_data =
      CHI_CLIENT->AllocateBuffer(HSHM_MCTX, data_size);
  client.Compress(HSHM_MCTX, chi::DomainQuery::GetLocalHash(0), orig_data.shm_,
                  data_size);
  return 0;
}
```

This will create the compressor module and then compress some data.

## Link to your mods (Internally)

Maybe you want to use your modules in the project they
are being built (internally). Below is an example cmake.
```cmake
cmake_minimum_required(VERSION 3.25)
project(internal)

add_executable(internal internal.cc)
target_link_libraries(internal example::compressor_client)
```

## Link to your mods (Externally)

For projects that are external to the mod repo, you
can link to it by finding the Example config.

Below is an example CMake:
```cmake
cmake_minimum_required(VERSION 3.25)
project(external)

find_package(Chimaera CONFIG REQUIRED)
find_package(Example CONFIG REQUIRED)

add_executable(external external.cc)
target_link_libraries(external example::compressor_client)
```

### Locate your module
```cpp
find_package(Example CONFIG REQUIRED)
``` 

The find_package for your mod repo will be in "uppercase camel case" format 
of the repo namespace. If you recall, we created the module repo with the 
following command:
```bash
chi_make_repo ~/my_mod_repo example
```
In this case, the namespace we passed to ``chi_make_repo`` is ``example``.
So in this case, our find_package would take as input ``Example`` (the upper camel case of ``example``).
If the namespace were ``hello_example``, the find_package would 
be ``HelloExample`` instead.

### Target link libraries
```cpp
target_link_libraries(external example::compressor_client)
```

There are two targets that you can access here:
1. ``example::compressor_client``: Contains the APIs for the client 
2. ``example::compressor_client_gpu``: Contains APIs for the client,
which can be called from GPU. Assumes Chimaera was compiled with
either CUDA or ROCm.

The ``example::`` prefixing the ``compressor_client`` is given by the
repo namespace. It is in snake case. If the namespace were hello_example,
then it would be ``hello_example::compressor_client``.

# Deploying Modules

In order to test your module, you should learn [Jarvis](../../../81-jarvis/02-jarvis-cd/01-index.mdx), which
is the tool that is used to deploy the iowarp runtime.

In the previous section, we built a module repo named "my_mod_repo" with the
namespace "example" and a module named "compressor" in that repo. This section will give detail
on how to build a jarvis package to test the compressor module.

## The Current Example

```bash
my_mod_repo
├── chimaera_repo.yaml  # Repo metadata
├── CMakeLists.txt      # Repo cmake
└── compressor
    ├── chimaera_mod.yaml  # Module metadata
    ├── CMakeLists.txt     # Module cmake
    ├── include
    │   └── compressor
    │       ├── compressor_client.h      # Client API
    │       ├── compressor_lib_exec.h    # (autogenerated from *methods.yaml)
    │       ├── compressor_methods.h     # (autogenerated from *methods.yaml)
    │       ├── compressor_methods.yaml  # Task declarations 
    │       └── compressor_tasks.h       # Task struct definitions 
    └── src
        ├── CMakeLists.txt          # Builds compressor_client and runtime  
        ├── compressor_client.cc    # Client API source
        ├── compressor_monitor.py   # Used for monitoring
        └── compressor_runtime.cc   # Runtime API source
```

## Create a simple unit test
```bash
mkdir ~/my_mod_repo/compressor/test
```

Edit the file ``~/my_mod_repo/compressor/test/test.cc``:
```cpp
#include "compressor/compressor_client.h"

int main() {
  CHIMAERA_CLIENT_INIT();
  chi::compressor::Client client;
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");

  size_t data_size = hshm::Unit<size_t>::Megabytes(1);
  hipc::FullPtr<char> orig_data =
      CHI_CLIENT->AllocateBuffer(HSHM_MCTX, data_size);
  client.Compress(HSHM_MCTX, chi::DomainQuery::GetLocalHash(0), orig_data.shm_,
                  data_size);
  return 0;
}
```

## Add the tester CMake
Edit the file ``~/my_mod_repo/compressor/test/CMakeLists.txt``:
```cmake
add_executable(compress_test test.cc)
target_link_libraries(compress_test example::compressor_client)

install(
  TARGETS
  compress_test
  EXPORT
  ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR}
)
```

This will create an executable named ``compress_test`` when ``make`` is executed.

## Connect the tester CMake to the overall module

And then edit compressor's root cmake ``~/my_mod_repo/compressor/CMakeLists.txt``:
```cmake                                                                                                     
# ------------------------------------------------------------------------------
# Build compressor module
# ------------------------------------------------------------------------------
include_directories(include)
add_subdirectory(src)
add_subdirectory(test)  # ADD ME

# -----------------------------------------------------------------------------
# Install compressor headers
# -----------------------------------------------------------------------------
install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})
```

## Build and Install
```bash
scspkg create compressor
cd ~/my_mod_repo
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$(scspkg pkg root compressor)
make -j32 install
```

## Create a Jarvis Repo

The following will create a repo named ``jarvis_example``.
```bash
mkdir -p ~/my_mod_repo/jarvis_example/jarvis_example
mkdir -p ~/my_mod_repo/jarvis_example/pipelines
jarvis repo add ~/my_mod_repo/jarvis_example
```

## Create a Jarvis Pkg

The following will bootstrap a compressor package in the
most recently added repo (jarvis_example).
```bash
jarvis repo create compressor app
```

You will then have the following structure:
```bash
.
├── chimaera_repo.yaml
├── CMakeLists.txt
├── compressor
│   ├── chimaera_mod.yaml
│   ├── CMakeLists.txt
│   ├── include
│   │   └── compressor
│   │       ├── compressor_client.h
│   │       ├── compressor_client.temp_h
│   │       ├── compressor_lib_exec.h
│   │       ├── compressor_methods.compiled.yaml
│   │       ├── compressor_methods.h
│   │       ├── compressor_methods.yaml
│   │       ├── compressor_tasks.h
│   │       └── compressor_tasks.temp_h
│   ├── src
│   │   ├── CMakeLists.txt
│   │   ├── CMakeLists.txt.backup
│   │   ├── compressor_client.cc
│   │   ├── compressor_monitor.py
│   │   ├── compressor_runtime.cc
│   │   └── compressor_runtime.temp_cc
│   └── test
│       ├── CMakeLists.txt
│       └── test.cc
└── jarvis_example
    ├── jarvis_example
    │   └── compressor
    │       └── pkg.py
    └── pipelines
```

## Build the Jarvis Pkg

Begin editing ``~/my_mod_repo/jarvis_example/jarvis_example/compressor/pkg.py``

The unedited code looks like this:
```python
"""
This module provides classes and methods to launch the Compressor application.
Compressor is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class Compressor(Application):
    """
    This class provides methods to launch the Compressor application.
    """
    def _init(self):
        """
        Initialize paths
        """
        pass

    def _configure_menu(self):
        """
        Create a CLI menu for the configurator method.
        For thorough documentation of these parameters, view:
        https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing

        :return: List(dict)
        """
        return [
            {
                'name': None,  # The name of the parameter
                'msg': '',  # Describe this parameter
                'type': str,  # What is the parameter type?
                'default': None,  # What is the default value if not required?
                # Does this parameter have specific valid inputs?
                'choices': [],
                # When type is list, what do the entries of the list mean?
                # A list of dicts just like this one.
                'args': [],
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        pass

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        pass

    def stop(self):
        """
        Stop a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        pass

    def kill(self):
        """
        Forcibly a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        pass

    def clean(self):
        """
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        pass
```

### Edit configure menu
```python
    def _configure_menu(self):
        """
        Create a CLI menu for the configurator method.
        For thorough documentation of these parameters, view:
        https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing

        :return: List(dict)
        """
        return []
```

Our package will take no arguments.

## Edit the start function
```python
    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        Exec('compress_test',
             LocalExecInfo(env=self.env))
```

* ``Exec`` will run the command ``compress_test``.
* ``LocalExecInfo`` indicates that this will be executed on this machine.
* ``env=self.env`` ensures that the pipeline's environment is used for this command.
The environment is stored in a YAML file in the pipeline's config directory and is
built using commands like ``jarvis ppl env build`` and ``jarvis ppl env copy``.

## More documentation on Jarvis PKGs
[This guide](../../../81-jarvis/02-jarvis-cd/09-building-package.md) goes over
in more detail each jarvis pkg method.

[This guide](../../../81-jarvis/01-jarvis-util/02-program-execution.md) goes over
more ways to execute distributed programs with tools like PSSH and MPI.

[This guide](../../../81-jarvis/01-jarvis-util/04-argument-parsing.md) goes over
more about the argument parser.

## Build a Pipeline Script

Next we must build a pipeline for the tool. Below is an 
example pipeline script that you should place in 

``~/my_mod_repo/compressor/jarvis_example/pipelines/test.yaml``
```yaml
name: test_pipeline
env: chimaera
pkgs:
  - pkg_type: chimaera_run
    pkg_name: chimaera_run
    sleep: 5
    do_dbg: false
    dbg_port: 4000
    port: 6000
    modules: ['example_compressor']
  - pkg_type: compressor
    pkg_name: compressor
```

The directory ``~/my_mod_repo/compressor/jarvis_example/pipelines`` is known
as a pipeline index. It is simply a place where jarvis knows pipelines are
located. This means you don't have to remember the full path to the script.

* ``env: chimaera``: This will copy the named environment named ``chimaera``. We will 
create this environment in the next section.
* ``modules``: The modules that the iowarp runtime (Chimaera) searches for at launch.
By using ``example_compressor`` here, the runtime will search for various shared objects, such as
``libexample_compressor.so`` in ``LD_LIBRARY_PATH``.
* ``pkg_type: compressor``: This corresponds to the ``jarvis repo create compressor`` command.
* ``pkg_name: compressor``: This can be whatever string you want. However, I typically just keep
it the same as ``pkg_type``.

## Build pipeline environment
```bash
spack load iowarp
module load compressor
jarvis env build chimaera
```

## Load + Run the Pipeline

Here is how we would run the pipeline
```bash
jarvis ppl index copy jarvis_example.test
jarvis ppl load yaml test.yaml
jarvis ppl run
```

# Domains

Domains are used to represent groupings of ChiContainers for logical organization.
By default, the following domains are created:
* ``chi::SubDomainId::kGlobalContainers``: The set of all containers in the pool
* ``chi::SubDomainId::kLocalContainers``: The set of containers on this node

There is also a ``chi::SubDomainId::kPhysicalNode``, which represents the
address of an actual machine. When a runtime is launched on a node, it is assigned a
unique ID between 1 and 2^32. This domain gets used for situations such as migrating
ChiContainers across nodes.

## Domain Query

A Domain Query represents a subset of addresses. For example, a hashtable may
route a task to a specific ChiContainer, which represents the bucket. Alternatively,
a cache invalidation algorithm may want to broadcast a task across all ChiContainers
to invalidate data caches.

### Range Flags

Range flags represent the scope of the query:
* ``kLocal``: Only addresses local to this node are used
* ``kDirect``: A single specific address is used
* ``kGlobal``: Can be a range of addresses across all nodes
* ``kSchedule``: Determine the addresses later through a scheduler

###  Selection Flags

Selection flags indicate how the addresses are represented:
* ``kId``: The address is the exact address to be used
* ``kHash``: The address will be hashed against the size of the domain
* ``kRange``: The address will be a range of nodes

### Iteration Algorithms

Iteration flags indicate how  a task will be dispersed among the domain
* ``kBroadcast``: Will broadcast the task across all nodes in the domain
* ``kRepUntilSuccess``: Will iterate over the domain until success

## Domain Query: GetDirectHash

```cpp
static DomainQuery GetDirectHash(const SubDomainGroup &sub_id, u32 hash,
                                   u32 iter_flags = kBroadcast);
```

Hash the task to an offset in the subdomain vector. Good for things
like hashtables.

## Domain Query:: GetGlobal

```cpp
static DomainQuery GetGlobalBcast();
```

Broadcast the task to every ChiContainer in the ChiPool. Useful
for things like flushing.

# Statically Scheduling Tasks and Concurrency

The section gives the overview of how to send tasks to containers in a pool.
We go over an end-to-end flow of a block device pool and how tasks are sent
to containers in that pool. We demonstrate "static scheduling", where the
client is responsible for sending the task to a particular container. We 
discuss "dynamic scheduling" in a future section, where the location of
the task is determined by an algorithm defined by the module developer.

## Creating a Pool

```cpp
int main() {
  chi::bdev::Client client;
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "tempdir",
      "fs::///tmp/chi_test_bdev.bin", GIGABYTES(1));
}
```

Here we create the block device pool.
* ``chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0)``: The admin container
where the creation task is sent to. In this case, container 0. 
* ``chi::DomainQuery::GetGlobalBcast()``: The set of nodes that can address the container. This
is the set of all nodes. 
* ``"tempdir"``: The name of the block device in iowarp. 
* ``"fs::///tmp/chi_test_bdev.bin"``: The name of the block device on the system. Just a file in 
a filesystem.
* ``GIGABYTES(1)``: The maximum size of the file.

This command creates a pool where containers can be migrated to any node in the entire system.
Use ``GetGlobalBcast`` for now -- smaller domains for ``Create`` are not currently supported.
This will spawn a ``CreateTask`` that gets sent to the iowarp runtime for processing.

## Creating a Container

Below is a snippet from the ``Create`` function that gets invoked within the runtime.
The Pool Creation task will call the ``Create`` function of the module. The creation is
responsible for initiating parameters to the container. The most important parameter
we discuss are Lanes.

```cpp
  /** Construct bdev */
  void Create(CreateTask *task, RunContext &rctx) {
    CreateTaskParams params = task->GetParams();
    std::string url = params.path_.str();
    size_t dev_size = params.size_;
    url_.Parse(url);
    url_.path_ = hshm::Formatter::format("{}.{}", url_.path_, container_id_);
    alloc_.Init(1, dev_size);
    CreateLaneGroup(kMdGroup, 1, QUEUE_LOW_LATENCY);
    CreateLaneGroup(kDataGroup, 32, QUEUE_HIGH_LATENCY);
```

Lanes store a set of tasks to execute to process in sequence.
They are simply FIFO queues. The number of lanes can be thought of as the
maximum concurrency of a Container. Lanes will be dynamically
mapped to worker threads based on load.

Lanes get divided into different groups to logically separate tasks
that affect either different data structures or have very different performance
characteristics. Below is an example for a block device code, where we create 
two groups:
* kMdGroup: where metadata-related tasks (e.g., block allocations) go
* kDataGroup: where data I/O operations go

Lane groups are set with a certain priority. Lanes 
support two priorities: QUEUE_LOW_LATENCY and QUEUE_HIGH_LATENCY.
This affects how the lanes are mapped and scheduled to workers. Generally,
low-latency lanes are sent to workers dedicated to a core.

Additionally, each group has parameter for concurreny (number of lanes).
In this example, only one lane for metadata. This is because the data structure
for block allocation does not support concurrent access. However, the data
object gets 32 lanes, implying a maximum concurreny of 32 threads performing
I/O.

## Block Dev API Review

Below is a snippet of the APIs the block device exposes:
```cpp
/** Allocate a section of the block device */
  HSHM_INLINE_CROSS_FUN
  std::vector<Block> Allocate(const hipc::MemContext &mctx,
                              const DomainQuery &dom_query, size_t size);

  /** Free a section of the block device */
  HSHM_INLINE_CROSS_FUN
  void Free(const hipc::MemContext &mctx, const DomainQuery &dom_query,
            const Block &block);

  /** Write to the block device */
  HSHM_INLINE_CROSS_FUN
  void Write(const hipc::MemContext &mctx, const DomainQuery &dom_query,
             const hipc::Pointer &data, Block block);

  /** Read from the block device */
  HSHM_INLINE_CROSS_FUN
  void Read(const hipc::MemContext &mctx, const DomainQuery &dom_query,
            const hipc::Pointer &data, Block &block);
```

We will discuss how a task for Allocate is mapped to the runtime.

## Spawn an Allocate task

```cpp
int main() {
  chi::bdev::Client client;
  // ...
  std::vector<chi::Block> blocks =
        client.Allocate(HSHM_MCTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomain::kGlobalContainers, 1),
                        MEGABYTES(1));
}
```

This is an example where we allocate 1MB of data from bdev container 1.

## Routing Tasks to Lanes

When a task is mapped to a container, it must then be assigned to a lane.
Many algorithms are possible -- it is up to the developer to implement an
algorithm of choice. Below is the example:
```cpp
Lane *MapTaskToLane(const Task *task) override {
    switch (task->method_) {
      case Method::kRead:
      case Method::kWrite: {
        return GetLeastLoadedLane(
            kDataGroup, task->prio_,
            [](Load &lhs, Load &rhs) { return lhs.cpu_load_ < rhs.cpu_load_; });
      }
      default: {
        return GetLaneByHash(kMdGroup, task->prio_, 0);
      }
    }
  }
```

In this example, the read and write tasks are mapped to the kDataGroup lanes.
The least-loaded lane will be chosen for the mapping.

# Dynamic Scheduling

Continuing from the previous example, we now describe Dynamic Scheduling.
In this case, users do not provide the exact container they want the task
to be mapped to. Instead, the pool provides a scheduling algorithm that
determines where the task should go.

## Spawn an Allocate task

```cpp
int main() {
  chi::bdev::Client client;
  // ...
  std::vector<chi::Block> blocks =
        client.Allocate(HSHM_MCTX,
                        chi::DomainQuery::GetDynamic(),
                        MEGABYTES(1));
}
```

This is an example where we allocate 1MB of data from the pool dynamically.

## The Monitor Callback

Each task has a Monitor function associated with it. Below is an example
of how it is can be used for dynamic scheduling callbacks.

```cpp
  template <typename TaskT>
  void IoRoute(TaskT *task) {
    // Concretize the domain to map the task
    task->dom_query_ = chi::DomainQuery::GetDirectHash(
        chi::SubDomainId::kGlobalContainers, 0);
    task->SetDirect();
    task->UnsetRouted();
  }


  void MonitorWrite(MonitorModeId mode, WriteTask *task, RunContext &rctx) {
    switch (mode) {
      case MonitorMode::kSchedule: {
        IoRoute<WriteTask>(task);
        return;
      }
    }
  }
```

This function simply converts the task's domain to always route to container 0.
Better algorithms can be made. For every task that should support dynamic scheduling,
a code snippet like this should be added to their Monitor function. For example,
for Reads, you should update MonitorRead.

# Long-Running Tasks

Long-running (i.e., periodic) tasks are periodically scheduled on an interval.
This can be used for things like monitoring and batching requests.

Below is an example of PollStatsTask, which gets periodically scheduled.
```cpp
struct PollStatsTask : public Task, TaskFlags<TF_SRL_SYM> {
  OUT BdevStats stats_;

  /** SHM default constructor */
  HSHM_INLINE_CROSS_FUN
  explicit PollStatsTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc)
      : Task(alloc) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN
  explicit PollStatsTask(const hipc::CtxAllocator<CHI_ALLOC_T> &alloc,
                         const TaskNode &task_node, const PoolId &pool_id,
                         const DomainQuery &dom_query, u32 period_ms)
      : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    pool_ = pool_id;
    method_ = Method::kPollStats;
    if (period_ms) {
      task_flags_.SetBits(TASK_PERIODIC);
      prio_ = TaskPrioOpt::kHighLatency;
    } else {
      task_flags_.SetBits(0);
      prio_ = TaskPrioOpt::kLowLatency;
    }
    dom_query_ = dom_query;

    SetPeriodMs(period_ms);
    HILOG(kInfo, "PollStatsTask: period_ms={}", period_ms);
  } 
};
```

There are two things that make a task scheduled periodically.
1. ``task_flags_.SetBits(TASK_PERIODIC)``: This will mark the task as periodically scheduled. There is now a
``TASK_LONG_RUNNING`` flag which is an alias to ``TASK_PERIODIC``.
1. ``SetPeriodMs(period_ms)``: This sets the minimum amount of time to elapse before rescheduling this task.

There are several different timing functions:
1. ``SetPeriodNs``
1. ``SetPeriodUs``
1. ``SetPeriodMs``
1. ``SetPeriodSec``
1. ``SetPeriodMin``
