/**
 * IPC manager implementation
 */

#include "chimaera/ipc_manager.h"
#include "chimaera/singletons.h"
#include "chimaera/task.h"

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool IpcManager::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  is_client_mode_ = true;

  // Initialize memory segments for client
  if (!InitializeMemorySegments()) {
    return false;
  }

  // Initialize priority queues
  if (!InitializePriorityQueues()) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

bool IpcManager::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  is_client_mode_ = false;

  // Initialize memory segments for server
  if (!InitializeMemorySegments()) {
    return false;
  }

  // Initialize priority queues
  if (!InitializePriorityQueues()) {
    return false;
  }

  // Initialize ZeroMQ server
  if (!InitializeZmqServer()) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

void IpcManager::Finalize() {
  if (!is_initialized_) {
    return;
  }

  auto mem_manager = HSHM_MEMORY_MANAGER;

  // Cleanup ZeroMQ server
  zmq_server_.reset();

  // Cleanup task queues
  if (!task_queue_.IsNull()) {
    auto alloc = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(main_allocator_id_);
    alloc->DelObj(HSHM_MCTX, task_queue_);
  }

  // Cleanup allocators
  if (!main_allocator_id_.IsNull()) {
    mem_manager->UnregisterAllocator(main_allocator_id_);
  }
  if (!client_data_allocator_id_.IsNull()) {
    mem_manager->UnregisterAllocator(client_data_allocator_id_);
  }
  if (!runtime_data_allocator_id_.IsNull()) {
    mem_manager->UnregisterAllocator(runtime_data_allocator_id_);
  }

  // Cleanup memory backends (always try to destroy if they were created)
  if (is_initialized_) {
    mem_manager->DestroyBackend(main_backend_id_);
    mem_manager->DestroyBackend(client_data_backend_id_);
    mem_manager->DestroyBackend(runtime_data_backend_id_);
  }

  is_initialized_ = false;
}

template<typename TaskT, typename ...Args>
FullPtr<TaskT> IpcManager::NewTask(MemorySegment segment, Args&&... args) {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  hipc::AllocatorId alloc_id;
  
  switch (segment) {
    case kMainSegment:
      alloc_id = main_allocator_id_;
      break;
    case kClientDataSegment:
      alloc_id = client_data_allocator_id_;
      break;
    case kRuntimeDataSegment:
      alloc_id = runtime_data_allocator_id_;
      break;
    default:
      return FullPtr<TaskT>();
  }
  
  auto alloc = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(alloc_id);
  hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, alloc);
  return alloc->NewObj<TaskT>(HSHM_MCTX, ctx_alloc, std::forward<Args>(args)...);
}

template<typename TaskT>
void IpcManager::DelTask(FullPtr<TaskT>& task_ptr, MemorySegment segment) {
  if (task_ptr.IsNull()) return;
  
  auto mem_manager = HSHM_MEMORY_MANAGER;
  hipc::AllocatorId alloc_id;
  
  switch (segment) {
    case kMainSegment:
      alloc_id = main_allocator_id_;
      break;
    case kClientDataSegment:
      alloc_id = client_data_allocator_id_;
      break;
    case kRuntimeDataSegment:
      alloc_id = runtime_data_allocator_id_;
      break;
    default:
      return;
  }
  
  auto alloc = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(alloc_id);
  alloc->DelObj(HSHM_MCTX, task_ptr);
}

template<typename T>
FullPtr<T> IpcManager::AllocateBuffer(size_t size, MemorySegment segment) {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  hipc::AllocatorId alloc_id;
  
  switch (segment) {
    case kMainSegment:
      alloc_id = main_allocator_id_;
      break;
    case kClientDataSegment:
      alloc_id = client_data_allocator_id_;
      break;
    case kRuntimeDataSegment:
      alloc_id = runtime_data_allocator_id_;
      break;
    default:
      return FullPtr<T>();
  }
  
  auto alloc = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(alloc_id);
  return alloc->template Allocate<T>(HSHM_MCTX, size);
}

void* IpcManager::GetProcessQueue(QueuePriority priority) {
  if (task_queue_.IsNull()) {
    return nullptr;
  }
  
  // Return specific lane of multi-queue based on priority
  auto& lane = task_queue_.ptr_->GetLane(0, static_cast<int>(priority));
  return &lane;
}

bool IpcManager::IsInitialized() const {
  return is_initialized_;
}

bool IpcManager::InitializeMemorySegments() {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  ConfigManager* config = CHI_CONFIG;
  
  try {
    // Initialize main memory backend
    main_backend_id_ = hipc::MemoryBackendId::Get(0);
    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        main_backend_id_,
        hshm::Unit<size_t>::Bytes(config->GetMemorySegmentSize(kMainSegment)),
        "chi_main_segment"
    );
    
    // Initialize client data backend
    client_data_backend_id_ = hipc::MemoryBackendId::Get(1);
    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        client_data_backend_id_,
        hshm::Unit<size_t>::Bytes(config->GetMemorySegmentSize(kClientDataSegment)),
        "chi_client_data_segment"
    );
    
    // Initialize runtime data backend
    runtime_data_backend_id_ = hipc::MemoryBackendId::Get(2);
    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        runtime_data_backend_id_,
        hshm::Unit<size_t>::Bytes(config->GetMemorySegmentSize(kRuntimeDataSegment)),
        "chi_runtime_data_segment"
    );
    
    // Create allocators
    main_allocator_id_ = hipc::AllocatorId(1, 0);
    mem_manager->CreateAllocator<CHI_MAIN_ALLOC_T>(
        main_backend_id_, main_allocator_id_, 0
    );
    
    client_data_allocator_id_ = hipc::AllocatorId(1, 1);
    mem_manager->CreateAllocator<CHI_CDATA_ALLOC_T>(
        client_data_backend_id_, client_data_allocator_id_, 0
    );
    
    runtime_data_allocator_id_ = hipc::AllocatorId(1, 2);
    mem_manager->CreateAllocator<CHI_RDATA_ALLOC_T>(
        runtime_data_backend_id_, runtime_data_allocator_id_, 0
    );
    
    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::InitializePriorityQueues() {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  auto alloc = mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(main_allocator_id_);
  
  try {
    // Create multi-queue with 1 lane, 2 priorities (low/high latency), depth 1024
    task_queue_ = alloc->NewObj<hshm::multi_mpsc_queue<u32>>(
        HSHM_MCTX, alloc, 1, 2, 1024
    );
    
    return !task_queue_.IsNull();
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::InitializeZmqServer() {
  ConfigManager* config = CHI_CONFIG;
  
  try {
    // Initialize ZeroMQ server using HSHM Lightbeam
    std::string addr = "127.0.0.1";
    std::string protocol = "tcp";
    u32 port = config->GetZmqPort();
    
    zmq_server_ = hshm::lbm::TransportFactory::GetServer(
        addr, hshm::lbm::Transport::kZeroMq, protocol, port
    );
    
    return zmq_server_ != nullptr;
  } catch (const std::exception& e) {
    return false;
  }
}

// Explicit template instantiations
template FullPtr<Task> IpcManager::NewTask<Task>(MemorySegment);
template void IpcManager::DelTask<Task>(FullPtr<Task>&, MemorySegment);
template FullPtr<char> IpcManager::AllocateBuffer<char>(size_t, MemorySegment);

}  // namespace chi