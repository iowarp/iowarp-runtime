#ifndef CHI_IPC_MANAGER_H_
#define CHI_IPC_MANAGER_H_

#include "../chimaera_task.h"
#include "../chimaera_types.h"
#include "../config/config_manager.h"
#include <hermes_shm/data_structures/all.h>
#include <hermes_shm/lightbeam/lightbeam.h>
#include <hermes_shm/util/singleton.h>
#include <thread>
#include <chrono>

namespace chi {

using hshm::ipc::Allocator;
using hshm::ipc::AllocatorId;
using hshm::ipc::MemoryBackend;
using hshm::ipc::MemoryManager;
using hshm::ipc::PosixShmMmap;
using hshm::ipc::MemoryBackendType;

class ProcessQueue {
public:
  chi::ipc::multi_mpsc_queue<TaskPointer> *queue_;
  u32 num_lanes_;
  u32 num_priorities_;

  ProcessQueue() : queue_(nullptr), num_lanes_(0), num_priorities_(2) {}

  void Init(u32 num_lanes, u32 queue_depth) {
    num_lanes_ = num_lanes;
    
    auto alloc = HSHM_MEMORY_MANAGER->GetAllocator<CHI_MAIN_ALLOC_T>(AllocatorId(1, 0));
    queue_ = alloc->NewObjLocal<chi::ipc::multi_mpsc_queue<TaskPointer>>(
        hipc::MemContext(),
        num_lanes,       // number of lanes
        num_priorities_, // number of priorities (low/high latency)
        queue_depth      // depth per queue
    ).ptr_;
  }

  void EnqueueTask(u32 priority, u32 lane_id, const TaskPointer &task) {
    if (queue_ && priority < num_priorities_ && lane_id < num_lanes_) {
      queue_->GetLane(lane_id, priority).emplace(task);
    }
  }

  bool DequeueTask(u32 priority, u32 lane_id, TaskPointer &task) {
    if (queue_ && priority < num_priorities_ && lane_id < num_lanes_) {
      auto qtok = queue_->GetLane(lane_id, priority).pop(task);
      return !qtok.IsNull();
    }
    return false;
  }

  u32 GetNumLanes() const { 
    return num_lanes_; 
  }

  ~ProcessQueue() {
    if (queue_) {
      auto alloc = HSHM_MEMORY_MANAGER->GetAllocator<CHI_MAIN_ALLOC_T>(AllocatorId(1, 0));
      alloc->DelObj(hipc::MemContext(), queue_);
      queue_ = nullptr;
    }
  }
};

class IpcManager {
private:
  std::unique_ptr<hshm::lbm::Server> zmq_server_;
  std::unique_ptr<hshm::lbm::Client> zmq_client_;
  MemoryManager *mem_manager_;
  AllocatorId main_alloc_id_;
  ProcessQueue *process_queue_;
  bool is_initialized_;
  bool is_server_;

public:
  IpcManager() 
      : mem_manager_(nullptr), process_queue_(nullptr), 
        is_initialized_(false), is_server_(false) {
    main_alloc_id_ = AllocatorId(1, 0);
  }

#ifdef CHIMAERA_RUNTIME
  void ServerInit() {
    if (is_initialized_) {
      return;
    }

    auto config = CHI_CONFIG_MANAGER->GetConfig();
    is_server_ = true;

    InitializeSharedMemoryServer(config);
    InitializeProcessQueue(config);
    InitializeZmqServer(config);

    is_initialized_ = true;
    HILOG(kInfo, "IPC Manager (Server) initialized");
  }
#endif  // CHIMAERA_RUNTIME

  void ClientInit() {
    if (is_initialized_) {
      return;
    }

    auto config = CHI_CONFIG_MANAGER->GetConfig();
    is_server_ = false;

    InitializeSharedMemoryClient(config);
    InitializeZmqClient(config);

    is_initialized_ = true;
    HILOG(kInfo, "IPC Manager (Client) initialized");
  }

  void Shutdown() {
    if (is_server_ && process_queue_) {
      auto alloc = mem_manager_->GetAllocator<CHI_MAIN_ALLOC_T>(main_alloc_id_);
      alloc->DelObj(hipc::MemContext(), process_queue_);
      process_queue_ = nullptr;
    }

    if (is_server_ && mem_manager_) {
      mem_manager_->UnregisterAllocator(main_alloc_id_);
      mem_manager_->DestroyBackend(hipc::MemoryBackendId::GetRoot());
    }

    zmq_server_.reset();
    zmq_client_.reset();
    is_initialized_ = false;
    
    if (is_server_) {
      HILOG(kInfo, "IPC Manager (Server) shut down");
    } else {
      HILOG(kInfo, "IPC Manager (Client) shut down");
    }
  }

  bool IsInitialized() const { return is_initialized_; }
  
#ifdef CHIMAERA_RUNTIME
  ProcessQueue *GetProcessQueue() { return process_queue_; }
  hshm::lbm::Server *GetZmqServer() { return zmq_server_.get(); }
#endif  // CHIMAERA_RUNTIME
  
  hshm::lbm::Client *GetZmqClient() { return zmq_client_.get(); }
  MemoryManager *GetMemoryManager() { return mem_manager_; }
  AllocatorId GetMainAllocatorId() const { return main_alloc_id_; }

  // Task creation and management methods
  template<typename TaskT>
  TaskT* NewTask(const hipc::MemContext& mctx) {
    auto alloc = GetDefaultAllocator();
    hipc::FullPtr<TaskT> task_ptr = alloc->NewObjLocal<TaskT>(mctx);
    return task_ptr.ptr_;
  }

  template<typename TaskT>
  TaskT* NewEmptyTask(const hipc::MemContext& mctx, hipc::Pointer& shm_ptr) {
    auto alloc = GetDefaultAllocator();
    hipc::FullPtr<TaskT> task_ptr = alloc->NewObjLocal<TaskT>(mctx);
    shm_ptr = task_ptr.shm_;
    return task_ptr.ptr_;
  }

  template<typename TaskT>
  void DelTask(const hipc::MemContext& mctx, TaskT* task) {
    auto alloc = GetDefaultAllocator();
    alloc->DelObj(mctx, task);
  }

  template<typename TaskT>
  void SubmitTask(TaskT* task, u32 priority = QueuePriority::kLowLatency) {
    if (!is_initialized_ || (!is_server_ && !zmq_client_)) {
      HELOG(kError, "IPC Manager not properly initialized");
      return;
    }

    try {
      hshm::BinaryOutputArchive<true> ar;
      ar << *task;
      
      std::vector<char> data = ar.Get();
      
      if (zmq_client_) {
        // Client mode: send via ZeroMQ client
        hshm::lbm::Bulk bulk = zmq_client_->Expose(data.data(), data.size(), 0);
        hshm::lbm::Event* event = zmq_client_->Send(bulk);
        
        while (!event->is_done) {
          std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        
        if (event->error_code != 0) {
          HELOG(kError, "Failed to submit task: error code {}", event->error_code);
        }
        
        delete event;
      }
#ifdef CHIMAERA_RUNTIME
      else if (process_queue_) {
        // Runtime mode: submit directly to process queue
        TaskPointer task_ptr;
        task_ptr.ptr_ = reinterpret_cast<Task*>(task);
        task_ptr.pool_id_ = task->pool_id_;
        
        // Use default lane 0 for now, could be made configurable
        process_queue_->EnqueueTask(priority, 0, task_ptr);
      }
#endif
    } catch (const std::exception& e) {
      HELOG(kError, "Exception while submitting task: {}", e.what());
    }
  }

  template<typename TaskT>
  TaskT* SubmitTaskSync(TaskT* task, u32 priority = QueuePriority::kLowLatency) {
    SubmitTask(task, priority);
    task->Wait();
    return task;
  }

  PoolId CreatePool(const std::string& pool_name, const std::string& module_name) {
    if (!is_initialized_) {
      HELOG(kError, "IPC Manager not initialized");
      return 0;
    }

    // This would typically create a CreatePoolTask and submit it
    // For now, return a dummy pool ID
    static PoolId next_pool_id = 1;
    HILOG(kInfo, "Created pool '{}' with module '{}', ID: {}", 
          pool_name, module_name, next_pool_id);
    return next_pool_id++;
  }

  bool DestroyPool(PoolId pool_id) {
    if (!is_initialized_) {
      HELOG(kError, "IPC Manager not initialized");
      return false;
    }

    HILOG(kInfo, "Destroyed pool ID: {}", pool_id);
    return true;
  }

  void StopRuntime() {
    if (!is_initialized_) {
      HELOG(kError, "IPC Manager not initialized");
      return;
    }

    HILOG(kInfo, "Stopping Chimaera runtime");
  }

private:
  hipc::Allocator* GetDefaultAllocator() {
    if (mem_manager_) {
      return mem_manager_->GetAllocator<CHI_MAIN_ALLOC_T>(main_alloc_id_);
    }
    return HSHM_DEFAULT_ALLOC;
  }

private:
#ifdef CHIMAERA_RUNTIME
  void InitializeSharedMemoryServer(const ChimaeraConfig &config) {
    mem_manager_ = HSHM_MEMORY_MANAGER;
    
    size_t shm_size = config.queue_manager_.shm_size_;
    if (shm_size == 0) {
      shm_size = hshm::Unit<size_t>::Gigabytes(1);
    }

    // Server: Create the backend (similar to PretestRank0)
    mem_manager_->UnregisterAllocator(main_alloc_id_);
    mem_manager_->DestroyBackend(hipc::MemoryBackendId::GetRoot());

    mem_manager_->CreateBackend<PosixShmMmap>(
        hipc::MemoryBackendId::Get(0),
        shm_size,
        config.queue_manager_.shm_name_
    );

    mem_manager_->CreateAllocator<CHI_MAIN_ALLOC_T>(
        hipc::MemoryBackendId::Get(0),
        main_alloc_id_,
        0
    );

    HILOG(kInfo, "Shared memory created: {} bytes", shm_size);
  }
#endif  // CHIMAERA_RUNTIME

  void InitializeSharedMemoryClient(const ChimaeraConfig &config) {
    mem_manager_ = HSHM_MEMORY_MANAGER;
    
    // Client: Connect to existing backend (similar to PretestRankN)
    try {
      mem_manager_->AttachBackend(
          MemoryBackendType::kPosixShmMmap, 
          config.queue_manager_.shm_name_
      );

      HILOG(kInfo, "Connected to shared memory: {}", config.queue_manager_.shm_name_);
    } catch (const std::exception& e) {
      HELOG(kError, "Failed to connect to shared memory: {}", e.what());
      throw;
    }
  }

#ifdef CHIMAERA_RUNTIME
  void InitializeProcessQueue(const ChimaeraConfig &config) {
    auto work_config = config.work_orchestrator_;
    u32 num_workers = work_config.cpus_.size();
    u32 queue_depth = config.queue_manager_.proc_queue_depth_;

    auto alloc = mem_manager_->GetAllocator<CHI_MAIN_ALLOC_T>(main_alloc_id_);
    process_queue_ = alloc->NewObjLocal<ProcessQueue>(hipc::MemContext()).ptr_;

    process_queue_->Init(num_workers, queue_depth);

    HILOG(kInfo, "Process queue initialized: {} workers, {} depth", num_workers, queue_depth);
  }

  void InitializeZmqServer(const ChimaeraConfig &config) {
    auto rpc_config = config.rpc_;
    std::string current_hostname = rpc_config.GetCurrentHostname();

    try {
      zmq_server_ = hshm::lbm::TransportFactory::GetServer(
          current_hostname, hshm::lbm::Transport::kZeroMq, "tcp", rpc_config.port_);
      HILOG(kInfo, "ZeroMQ server started on {}:{}", current_hostname, rpc_config.port_);
    } catch (const std::exception &e) {
      HELOG(kError, "Failed to start ZeroMQ server: {}", e.what());
      throw;
    }
  }
#endif  // CHIMAERA_RUNTIME

  void InitializeZmqClient(const ChimaeraConfig &config) {
    auto rpc_config = config.rpc_;
    
    // Connect to all resolved hosts
    const auto& hosts = rpc_config.GetResolvedHosts();
    
    for (const std::string& host : hosts) {
      try {
        zmq_client_ = hshm::lbm::TransportFactory::GetClient(
            host, hshm::lbm::Transport::kZeroMq, "tcp", rpc_config.port_);
        HILOG(kInfo, "ZeroMQ client connected to {}:{}", host, rpc_config.port_);
        return;  // Successfully connected to one host
      } catch (const std::exception &e) {
        HELOG(kWarning, "Failed to connect to {}: {}", host, e.what());
      }
    }
    
    HELOG(kError, "Failed to connect to any ZeroMQ server");
    throw std::runtime_error("No available ZeroMQ servers");
  }
};

// Client API convenience functions
namespace api {

template<typename TaskT>
TaskT* NewTask() {
  return CHI_IPC_MANAGER->NewTask<TaskT>(HSHM_DEFAULT_MEM_CTX);
}

template<typename TaskT>
void DelTask(TaskT* task) {
  CHI_IPC_MANAGER->DelTask(HSHM_DEFAULT_MEM_CTX, task);
}

template<typename TaskT>
void SubmitTask(TaskT* task, u32 priority = QueuePriority::kLowLatency) {
  CHI_IPC_MANAGER->SubmitTask(task, priority);
}

template<typename TaskT>
TaskT* SubmitTaskSync(TaskT* task, u32 priority = QueuePriority::kLowLatency) {
  return CHI_IPC_MANAGER->SubmitTaskSync(task, priority);
}

inline PoolId CreatePool(const std::string& pool_name, const std::string& module_name) {
  return CHI_IPC_MANAGER->CreatePool(pool_name, module_name);
}

inline bool DestroyPool(PoolId pool_id) {
  return CHI_IPC_MANAGER->DestroyPool(pool_id);
}

inline void StopRuntime() {
  CHI_IPC_MANAGER->StopRuntime();
}

}  // namespace api
}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::IpcManager, chiIpcManager);
#define CHI_IPC_MANAGER \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::IpcManager, chiIpcManager)
#define CHI_IPC_MANAGER_T chi::IpcManager*

#endif  // CHI_IPC_MANAGER_H_