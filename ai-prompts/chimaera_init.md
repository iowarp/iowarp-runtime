# Chimaera

Chimaera is a distributed task execution framework. Tasks represent arbitrary C++ functions, similar to RPCs. However, Chimaera aims to implement dynamic load balancing and reorganization to reduce stress. Chimaera's fundamental abstraction are ChiPools and ChiContainers. A ChiPool represents a distributed system (e.g., key-value store), while a ChiContainer represents a subset of the global state (e.g., a bucket). These ChiPools can be communicate to form several I/O paths simultaneously. 

Use google c++ style guide for the implementation. 

## ChiMod Specification
The structure of ChiMod repos and ChiMods is in ai-prompts/chimod_doc.md.

## Containers and Pools

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
1. GetLocalId(u32 id): Send task to container using its local address
2. GetGlobalId(u32 id): Send task to container using its global address 
3. GetLocalHash(u32 hash): Hash task to a container by taking modulo of the kLocal subdomain
4. GetGlobalHash(u32 hash): Hash task to a container by taking module of the kGlobal subdomain
5. GetDynamic(): Send this request to the container's Monitor method with MonitorMode kGlobalSchedule

## The Task

Tasks are used to communicate with containers and pools. Tasks are like RPCs. They contain a DomainQuery to determine which pool and containers to send the task, they contain a method identifier, and any parameters to the method they should execute. There is a base task data structure that all specific tasks inherit from. At minimum, tasks look as follows:
```
/** Decorator macros */
#define IN
#define OUT
#define INOUT
#define TEMP

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

## The Runtime

The runtime implements an intelligent, multi-threaded task execution system. The runtime read the environment variable CHI_SERVER_CONF to see the server configuration yaml file, which stores all configurations for the runtime. There should be a Configration parser that inherits from Hermes SHM's BaseConfig.

### Initialization

### Configuration Manager
Create a class and singleton for the ConfigurationManager using hshm. The configuration manager is responsible for parsing the chimaera server YAML file. A singleton should be made so that subsequent classes can access the config data.

### IPC Manager
When the runtime initially starts, it must spawn a ZeroMQ server using the local loopback address. Use lightbeam from hermes-shm for this. Clients can use this to detect a client on this node is executing and initially connect to the server. Make a singleton using hermes shm for this class.

After this, shared memory backends and allocators over those backends are created. The allocator used should be a compiler macro ``CHI_MAIN_ALLOC_T``. The default value should be ``hipc::ThreadLocalAllocator``.  

After this, a concurrent, priority queue named the process_queue is stored in the shared memory. This queue is for external processes to submit tasks to the runtime. The number of lanes (i.e., concurrency) is determined by the number of workers. There should be the following priorities: kLowLatency and kHighLatency. The queue lanes are implemented on top of mpsc_queue from hshm. The depth of the queue and is configurable. It does not necessarily need to be a simple typedef.

### Module Manager
The module manager is responsible for holding state for all modules on this node. It uses hshm::SharedLibrary for loading shared library symbols. This will be invoked to load the chimaera_admin module and any additional modules configured in the 

```cpp
class ModuleManager {
 public:
  /**
    Check if any path matches. It checks for GPU variants first, since the
    runtime supports GPU if enabled. */
  std::vector<std::string> FindMatchingPathInDirs(const std::string &lib_name) {
    std::vector<std::string> variants = {"_gpu", "_host", ""};
    std::vector<std::string> prefixes = {"", "lib", "libchimaera_"};
    std::vector<std::string> suffixes = {"", "_runtime"};
    std::vector<std::string> extensions = {".so", ".dll"};
    std::vector<std::string> concrete_libs;
    for (const std::string &lib_dir : lib_dirs_) {
      // Determine if this directory contains the library
      std::vector<std::string> potential_paths;
      for (const std::string &variant : variants) {
        for (const std::string &prefix : prefixes) {
          for (const std::string &suffix : suffixes) {
            for (const std::string &extension : extensions) {
              potential_paths.emplace_back(hshm::Formatter::format(
                  "{}/{}{}{}{}{}", lib_dir, prefix, lib_name, variant, suffix,
                  extension));
            }
          }
        }
      }
      std::string lib_path = FindExistingPath(potential_paths);
      if (lib_path.empty()) {
        continue;
      };
      concrete_libs.emplace_back(lib_path);
    }
    return concrete_libs;
  }

  /** Load a module at path */
  bool LoadModuleAtPath(const std::string &lib_name,
                        const std::string &lib_path, ModuleInfo &info) {
    // Load the library
    info.lib_.Load(lib_path);
    if (info.lib_.IsNull()) {
      HELOG(kError, "Could not open the lib library: {}. Reason: {}", lib_path,
            info.lib_.GetError());
      return false;
    }

    // Get the allocate state function
    info.alloc_state_ = (alloc_state_t)info.lib_.GetSymbol("alloc_state");
    if (!info.alloc_state_) {
      HELOG(kError, "The lib {} does not have alloc_state symbol", lib_path);
      return false;
    }

    // Get the new state function
    info.new_state_ = (new_state_t)info.lib_.GetSymbol("new_state");
    if (!info.new_state_) {
      HELOG(kError, "The lib {} does not have new_state symbol", lib_path);
      return false;
    }

    // Get the module name function
    info.get_module_name =
        (get_module_name_t)info.lib_.GetSymbol("get_module_name");
    if (!info.get_module_name) {
      HELOG(kError, "The lib {} does not have get_module_name symbol",
            lib_path);
      return false;
    }

    // Check if the lib is already loaded
    std::string module_name = info.get_module_name();
    HILOG(kInfo, "(node {}) Finished loading the lib: {}", CHI_RPC->node_id_,
          module_name);
    info.static_state_ = info.alloc_state_();
    return true;
  }

  /** Load a module */
  bool LoadModule(const std::string &lib_name, ModuleInfo &info) {
    std::vector<std::string> lib_path = FindMatchingPathInDirs(lib_name);
    if (lib_path.empty()) {
      HELOG(kError, "Could not find the lib: {}", lib_name);
      return false;
    }
    for (const std::string &path : lib_path) {
      if (LoadModuleAtPath(lib_name, path, info)) {
        return true;
      }
    }
    return false;
  }
}
```

### Work Orchestrator
Make a work orchestrator class and singleton. It will spawn a configurable number of worker threads. There four types of worker threads:
1. Low latency: threads that execute only low-latency lanes. This includes lanes from the process queue. 
2. High latency: threads that execute only high-latency lanes.
3. Reinforcement: threads dedicated to the reinforcement of ChiMod performance models
4. Process Wreaper: detects when a process has died and frees its associated memory. For now, do not implement

Use ``HSHM_THREAD_MODEL->Spawn`` for spawning the threads.

When initially spawning the workers, the work orchestrator must also initially map the queues from the IPC Manager to each worker. It maps low-latency lanes to a subset of workers and then high-latency lanes to a different subset of workers. 



### Worker
Low-latency and high-latency workers iterate over a set of lanes and execute tasks from those lanes. Workers store an active lane queue and a cold lane queue. The active queue stores the set of lanes to iterate over. The cold queue stores lanes this worker is responsible for, but do not currently have activity. Lanes should have a setting 

When the worker executes a task, it must do the following:
1. Resolve the domain query of the task. I.e., identify the exact set of nodes to distribute the task to. There are a few cases. First, if GetDynamic was used, then get the local container and call the Monitor function using the MonitorMode kGlobalSchedule. This will replace the domain query with something more concrete. Next, if the task does not resolve to kLocal addresses, then send the task to the local remote queue container for scheduling. If the task is local, then get the container to send this task to. Call the Monitor function with the kLocalSchedule MonitorMode to route the task to a specific lane. If the lane was initially empty, then the worker processing it likely will ignore it. 
2. 

When the worker polls a task from a lane, it will create a stack space to initialize a boost fiber. 

## Clients
There should be a header-only client API. 

## ChiMods to Implement

The structure of these chimods already exists. Do not repeat method and task definitions. Just modify what is already there.

### chimaera_admin

This module contains the following methods:
```yaml
# Custom Methods
kCreatePool: 10
kDestroyPool: 11 
kStopRuntime: 12
```

A skeleton implementation is at tasks/chimaera_admin.
1. CreatePool register a local container for the module 

### remote_queue
TODO. Responsible for sending tasks across nodes.

## Hermes SHM (HSHM)

### Singleton
The header file:
```cpp
#include <hermes_shm/util/singleton.h>
/** Singleton declaration */
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(hshm::ipc::MemoryManager, hshmMemoryManager);
#define HSHM_MEMORY_MANAGER                               \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(hshm::ipc::MemoryManager, \
                                hshm::ipc::hshmMemoryManager)
#define HSHM_MEMORY_MANAGER_T hshm::ipc::MemoryManager *
```

The source file:
```cpp
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(hshm::ipc::MemoryManager,
                                    hshmMemoryManager);
```

### Thread Model
HSHM_THREAD_MODEL points to a specific threading library, such as pthreads. Below is an example
```cpp
TEST_CASE("ThreadModel") {
  auto *thread_model = HSHM_THREAD_MODEL;
  hshm::thread::ThreadGroup group = thread_model->CreateThreadGroup({});
  hshm::thread::Thread thread = thread_model->Spawn(
      group,
      [](int tid) {
        std::cout << "Hello, world! (pthread) " << tid << std::endl;
      },
      1);
  thread_model->Join(thread);
}
```

### Dynamic Library Loading

```cpp
/** Dynamically load shared libraries */
struct SharedLibrary {
  void *handle_;

  SharedLibrary() = default;
  HSHM_DLL SharedLibrary(const std::string &name);
  HSHM_DLL ~SharedLibrary();

  // Delete copy operations
  SharedLibrary(const SharedLibrary &) = delete;
  SharedLibrary &operator=(const SharedLibrary &) = delete;

  // Move operations
  HSHM_DLL SharedLibrary(SharedLibrary &&other) noexcept;
  HSHM_DLL SharedLibrary &operator=(SharedLibrary &&other) noexcept;

  HSHM_DLL void Load(const std::string &name);
  HSHM_DLL void *GetSymbol(const std::string &name);
  HSHM_DLL std::string GetError() const;

  bool IsNull() { return handle_ == nullptr; }
};
```

### HSHM Data Structure Template
This will create various data structures using the chi:: namespace prefix instead of hipc:: and hshm::. The reason is to ensure that we don't need to keep specifying the allocator type in the data structures.
```cpp
HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);
```

For example, originally I had to do:
```cpp
hipc::vector<int, CHI_MAIN_ALLOC_T>
```

Now I can do below, which is a little cleaner: 
```cpp
chi::ipc::vector<int>
```

### Memory Allocation and Backends
```cpp
#include "hermes_shm/data_structures/all.h"

using hshm::ipc::Allocator;
using hshm::ipc::AllocatorId;
using hshm::ipc::AllocatorType;
using hshm::ipc::MemoryBackend;
using hshm::ipc::MemoryBackendType;
using hshm::ipc::Pointer;
using hshm::ipc::PosixShmMmap;

using hshm::ipc::Allocator;
using hshm::ipc::AllocatorId;
using hshm::ipc::AllocatorType;
using hshm::ipc::MemoryBackend;
using hshm::ipc::MemoryBackendType;
using hshm::ipc::MemoryManager;
using hshm::ipc::Pointer;

GLOBAL_CONST AllocatorId MAIN_ALLOC_ID(1, 0);

template <typename AllocT>
void Pretest() {
  std::string shm_url = "test_allocators";
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  mem_mngr->UnregisterAllocator(MAIN_ALLOC_ID);
  mem_mngr->DestroyBackend(hipc::MemoryBackendId::GetRoot());
  mem_mngr->CreateBackend<PosixShmMmap>(hipc::MemoryBackendId::Get(0),
                                        hshm::Unit<size_t>::Megabytes(100),
                                        shm_url);
  mem_mngr->CreateAllocator<AllocT>(hipc::MemoryBackendId::Get(0),
                                    MAIN_ALLOC_ID, 0);
}
```

### Base Configuration
```cpp
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HSHM_CONFIG_PARSE_PARSER_H
#define HSHM_CONFIG_PARSE_PARSER_H

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstdlib>
#include <iomanip>
#include <list>
#include <regex>
#include <string>

#include "formatter.h"
#include "hermes_shm/constants/macros.h"
#include "logging.h"
#include "yaml-cpp/yaml.h"

namespace hshm {

class ConfigParse {
 public:
  static void rm_char(std::string &str, char ch) {
    str.erase(std::remove(str.begin(), str.end(), ch), str.end());
  }

  /**
   * parse a hostfile string
   * [] represents a range to generate
   * ; represents a new host name completely
   *
   * Example: hello[00-09,10]-40g;hello2[11-13]-40g
   * */
  static void ParseHostNameString(std::string hostname_set_str,
                                  std::vector<std::string> &list);

  /** parse the suffix of \a num_text NUMBER text */
  static std::string ParseNumberSuffix(const std::string &num_text);

  /** parse the number of \a num_text NUMBER text */
  template <typename T>
  static T ParseNumber(const std::string &num_text);

  /** 
  Converts \a size_text SIZE text into a size_t. E.g., 10g -> integer.
   */
  static hshm::u64 ParseSize(const std::string &size_text);

  /** Returns bandwidth (bytes / second). E.g., 10MBps -> integer */
  static hshm::u64 ParseBandwidth(const std::string &size_text);

  /** Returns latency (nanoseconds) */
  static hshm::u64 ParseLatency(const std::string &latency_text);

  /** Expands all environment variables in a path string */
  static std::string ExpandPath(std::string path);

  /** Parse hostfile */
  static std::vector<std::string> ParseHostfile(const std::string &path);
};

/**
 * Base class for configuration files
 * */
class BaseConfig {
 public:
  /** load configuration from a string */
  void LoadText(const std::string &config_string, bool with_default = true) {
    if (with_default) {
      LoadDefault();
    }
    if (config_string.size() == 0) {
      return;
    }
    YAML::Node yaml_conf = YAML::Load(config_string);
    ParseYAML(yaml_conf);
  }

  /** load configuration from file */
  void LoadFromFile(const std::string &path, bool with_default = true) {
    if (with_default) {
      LoadDefault();
    }
    if (path.size() == 0) {
      return;
    }
    auto real_path = hshm::ConfigParse::ExpandPath(path);
    try {
      YAML::Node yaml_conf = YAML::LoadFile(real_path);
      ParseYAML(yaml_conf);
    } catch (std::exception &e) {
      HELOG(kFatal, e.what());
    }
  }

  /** load the default configuration */
  virtual void LoadDefault() = 0;

 public:
  /** parse \a list_node vector from configuration file in YAML */
  template <typename T, typename VEC_TYPE = std::vector<T>>
  static void ParseVector(YAML::Node list_node, VEC_TYPE &list) {
    for (auto val_node : list_node) {
      list.emplace_back(val_node.as<T>());
    }
  }

  /** clear + parse \a list_node vector from configuration file in YAML */
  template <typename T, typename VEC_TYPE = std::vector<T>>
  static void ClearParseVector(YAML::Node list_node, VEC_TYPE &list) {
    list.clear();
    for (auto val_node : list_node) {
      list.emplace_back(val_node.as<T>());
    }
  }

 private:
  virtual void ParseYAML(YAML::Node &yaml_conf) = 0;
};

}  // namespace hshm

#endif  // HSHM_CONFIG_PARSE_PARSER_H
```

## MPSC Queue
An example of the MSPC queue.
```
TEST_CASE("TestMpscQueueInt") {
  auto *alloc = HSHM_DEFAULT_ALLOC;
  REQUIRE(alloc->GetCurrentlyAllocatedSize() == 0);
  ProduceThenConsume<hipc::mpsc_queue<int>, int>(1, 1, 32, 32);
  REQUIRE(alloc->GetCurrentlyAllocatedSize() == 0);
}

struct IntEntry : public hipc::list_queue_entry {
  int value;

  /** Default constructor */
  IntEntry() : value(0) {}

  /** Constructor */
  explicit IntEntry(int val) : value(val) {}
};

template <typename NewT>
class VariableMaker {
 public:
  std::vector<NewT> vars_;
  hipc::atomic<hshm::size_t> count_;

 public:
  explicit VariableMaker(size_t total_vars) : vars_(total_vars) { count_ = 0; }

  static NewT _MakeVariable(size_t num) {
    if constexpr (std::is_arithmetic_v<NewT>) {
      return static_cast<NewT>(num);
    } else if constexpr (std::is_same_v<NewT, std::string>) {
      return std::to_string(num);
    } else if constexpr (std::is_same_v<NewT, hipc::string>) {
      return hipc::string(std::to_string(num));
    } else if constexpr (std::is_same_v<NewT, IntEntry *>) {
      auto alloc = HSHM_DEFAULT_ALLOC;
      return alloc->template NewObjLocal<IntEntry>(HSHM_DEFAULT_MEM_CTX, num)
          .ptr_;
    } else {
      STATIC_ASSERT(false, "Unsupported type", NewT);
    }
  }

  NewT MakeVariable(size_t num) {
    NewT var = _MakeVariable(num);
    size_t count = count_.fetch_add(1);
    vars_[count] = var;
    return var;
  }

  size_t GetIntFromVar(NewT &var) {
    if constexpr (std::is_arithmetic_v<NewT>) {
      return static_cast<size_t>(var);
    } else if constexpr (std::is_same_v<NewT, std::string>) {
      return std::stoi(var);
    } else if constexpr (std::is_same_v<NewT, hipc::string>) {
      return std::stoi(var.str());
    } else if constexpr (std::is_same_v<NewT, IntEntry *>) {
      return var->value;
    } else {
      STATIC_ASSERT(false, "Unsupported type", NewT);
    }
  }

  void FreeVariable(NewT &var) {
    if constexpr (std::is_same_v<NewT, IntEntry *>) {
      auto alloc = HSHM_DEFAULT_ALLOC;
      alloc->DelObj(HSHM_DEFAULT_MEM_CTX, var);
    }
  }

  void FreeVariables() {
    if constexpr (std::is_same_v<NewT, IntEntry *>) {
      size_t count = count_.load();
      for (size_t i = 0; i < count; ++i) {
        auto alloc = HSHM_DEFAULT_ALLOC;
        alloc->DelObj(HSHM_DEFAULT_MEM_CTX, vars_[i]);
      }
    }
  }
};

template <typename QueueT, typename T>
class QueueTestSuite {
 public:
  QueueT &queue_;

 public:
  /** Constructor */
  explicit QueueTestSuite(QueueT &queue) : queue_(queue) {}

  /** Producer method */
  void Produce(VariableMaker<T> &var_maker, size_t count_per_rank) {
    std::vector<size_t> idxs;
    int rank = omp_get_thread_num();
    try {
      for (size_t i = 0; i < count_per_rank; ++i) {
        size_t idx = rank * count_per_rank + i;
        T var = var_maker.MakeVariable(idx);
        idxs.emplace_back(idx);
        while (queue_.emplace(var).IsNull()) {
        }
      }
    } catch (hshm::Error &e) {
      HELOG(kFatal, e.what());
    }
    REQUIRE(idxs.size() == count_per_rank);
    std::sort(idxs.begin(), idxs.end());
    for (size_t i = 0; i < count_per_rank; ++i) {
      size_t idx = rank * count_per_rank + i;
      REQUIRE(idxs[i] == idx);
    }
  }

  /** Consumer method */
  void Consume(int min_rank, VariableMaker<T> &var_maker,
               std::atomic<size_t> &count, size_t total_count,
               std::vector<size_t> &entries) {
    T entry;
    // Consume everything
    while (count < total_count) {
      auto qtok = queue_.pop(entry);
      if (qtok.IsNull()) {
        continue;
      }
      size_t entry_int = var_maker.GetIntFromVar(entry);
      size_t off = count.fetch_add(1);
      if (off >= total_count) {
        break;
      }
      entries[off] = entry_int;
      // var_maker.FreeVariable(entry);
    }

    int rank = omp_get_thread_num();
    HILOG(kInfo, "Rank {}: Consumed {} entries", rank, count.load());
    if (rank == min_rank) {
      // Ensure there's no data left in the queue
      REQUIRE(queue_.pop(entry).IsNull());
      // Ensure the data is all correct
      REQUIRE(entries.size() == total_count);
      std::sort(entries.begin(), entries.end());
      REQUIRE(entries.size() == total_count);
      for (size_t i = 0; i < total_count; ++i) {
        REQUIRE(entries[i] == i);
      }
      var_maker.FreeVariables();
    }
  }
};

template <typename QueueT, typename T>
void ProduceThenConsume(size_t nproducers, size_t nconsumers,
                        size_t count_per_rank, size_t depth) {
  QueueT queue(depth);
  QueueTestSuite<QueueT, T> q(queue);
  std::atomic<size_t> count = 0;
  std::vector<size_t> entries;
  VariableMaker<T> var_maker(nproducers * count_per_rank);
  entries.resize(count_per_rank * nproducers);

  // Produce all the data
  omp_set_dynamic(0);
#pragma omp parallel shared(var_maker, nproducers, count_per_rank, q, count, \
                                entries) num_threads(nproducers)  // NOLINT
  {                                                               // NOLINT
#pragma omp barrier
    q.Produce(var_maker, count_per_rank);
#pragma omp barrier
  }

  omp_set_dynamic(0);
#pragma omp parallel shared(var_maker, nproducers, count_per_rank, q) \
    num_threads(nconsumers)  // NOLINT
  {                          // NOLINT
#pragma omp barrier
     // Consume all the data
    q.Consume(0, var_maker, count, count_per_rank * nproducers, entries);
#pragma omp barrier
  }
}

template <typename QueueT, typename T>
void ProduceAndConsume(size_t nproducers, size_t nconsumers,
                       size_t count_per_rank, size_t depth) {
  QueueT queue(depth);
  size_t nthreads = nproducers + nconsumers;
  QueueTestSuite<QueueT, T> q(queue);
  std::atomic<size_t> count = 0;
  std::vector<size_t> entries;
  VariableMaker<T> var_maker(nproducers * count_per_rank);
  entries.resize(count_per_rank * nproducers);

  // Produce all the data
  omp_set_dynamic(0);
#pragma omp parallel shared(var_maker, nproducers, count_per_rank, q, count) \
    num_threads(nthreads)  // NOLINT
  {                        // NOLINT
#pragma omp barrier
    size_t rank = omp_get_thread_num();
    if (rank < nproducers) {
      // Producer
      q.Produce(var_maker, count_per_rank);
    } else {
      // Consumer
      q.Consume(nproducers, var_maker, count, count_per_rank * nproducers,
                entries);
    }
#pragma omp barrier
  }
}
```

## Lightbeam 

#include <hermes_shm/lightbeam/lightbeam.h>
#include <hermes_shm/lightbeam/transport_factory_impl.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace hshm::lbm;

class LightbeamTransportTest {
 public:
  LightbeamTransportTest(Transport transport, const std::string& addr,
                         const std::string& protocol, int port)
      : transport_(transport),
        addr_(addr),
        protocol_(protocol),
        port_(port) {}

  void Run() {
    std::cout << "\n==== Testing backend: " << BackendName() << " ====\n";
    auto server_ptr =
        TransportFactory::GetServer(addr_, transport_, protocol_, port_);
    std::string server_addr = server_ptr->GetAddress();
    std::unique_ptr<Client> client_ptr;
    if (transport_ == Transport::kLibfabric) {
      client_ptr = TransportFactory::GetClient(server_addr, transport_,
                                               protocol_, port_);
    } else {
      client_ptr = TransportFactory::GetClient(server_addr, transport_,
                                               protocol_, port_);
    }

    const std::string magic = "unit_test_magic";
    // Client exposes and sends data
    Bulk send_bulk = client_ptr->Expose(magic.data(), magic.size(), 0);
    Event* send_event = client_ptr->Send(send_bulk);
    while (!send_event->is_done) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(send_event->error_code == 0);
    delete send_event;

    // Server exposes buffer and receives data
    std::vector<char> recv_buf(magic.size());
    Bulk recv_bulk = server_ptr->Expose(recv_buf.data(), recv_buf.size(), 0);
    Event* recv_event = nullptr;
    while (!recv_event || !recv_event->is_done) {
      if (recv_event) delete recv_event;
      recv_event = server_ptr->Recv(recv_bulk);
      if (!recv_event->is_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    assert(recv_event->error_code == 0);
    std::string received(recv_bulk.data, recv_bulk.size);
    std::cout << "Received: " << received << std::endl;
    assert(received == magic);
    delete recv_event;
    std::cout << "[" << BackendName() << "] Test passed!\n";
  }

 private:
  std::string BackendName() const {
    switch (transport_) {
      case Transport::kZeroMq:
        return "ZeroMQ";
      case Transport::kThallium:
        return "Thallium";
      case Transport::kLibfabric:
        return "Libfabric";
      default:
        return "Unknown";
    }
  }
  Transport transport_;
  std::string addr_;
  std::string protocol_;
  int port_;
};

int main() {
  // Test ZeroMQ
#ifdef HSHM_ENABLE_ZMQ
  {
    std::string zmq_addr = "127.0.0.1";
    std::string zmq_protocol = "tcp";
    int zmq_port = 8192;
    LightbeamTransportTest test(Transport::kZeroMq, zmq_addr, zmq_protocol,
                                zmq_port);
    test.Run();
  }
#endif
  // Test Thallium
  {
    std::string thallium_addr = "127.0.0.1";
    std::string thallium_protocol = "ofi+sockets";
    int thallium_port = 8193;
    LightbeamTransportTest test(Transport::kThallium, thallium_addr,
                                thallium_protocol, thallium_port);
    test.Run();
  }
  // Test Libfabric
  {
    std::string libfabric_addr = "127.0.0.1";
    std::string libfabric_protocol = "tcp";
    int libfabric_port = 9222;
    LightbeamTransportTest test(Transport::kLibfabric, libfabric_addr,
                                libfabric_protocol, libfabric_port);
    test.Run();
  }
  std::cout << "All transport tests passed!" << std::endl;
  return 0;
} 