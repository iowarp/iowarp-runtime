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

  // Initialize memory segments for client
  if (!ClientInitShm()) {
    return false;
  }

  // Initialize priority queues
  if (!ClientInitQueues()) {
    return false;
  }

  is_initialized_ = true;
  return true;
}

bool IpcManager::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize memory segments for server
  if (!ServerInitShm()) {
    return false;
  }

  // Initialize priority queues
  if (!ServerInitQueues()) {
    return false;
  }

  // Initialize ZeroMQ server (optional - failure is non-fatal)
  InitializeZmqServer();

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

  // Cleanup task queue in shared header (queue handles cleanup automatically)
  // Only the last process to detach will actually destroy shared data
  shared_header_ = nullptr;

  // Clear cached allocator pointers
  main_allocator_ = nullptr;
  client_data_allocator_ = nullptr;
  runtime_data_allocator_ = nullptr;

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

// Template methods (NewTask, DelTask, AllocateBuffer, Enqueue) are implemented
// inline in the header

hipc::Pointer IpcManager::Dequeue(QueuePriority priority) {
  if (!shared_header_ || !shared_header_->task_queue.get() ||
      shared_header_->task_queue.get()->IsNull()) {
    return hipc::Pointer();
  }

  hipc::Pointer task_ptr;
  auto& lane =
      shared_header_->task_queue.get()->GetLane(0, static_cast<int>(priority));
  auto token = lane.pop(task_ptr);
  if (!token.IsNull()) {
    return task_ptr;
  }
  return hipc::Pointer();
}

void* IpcManager::GetProcessQueue(QueuePriority priority) {
  if (!shared_header_ || !shared_header_->task_queue.get() ||
      shared_header_->task_queue.get()->IsNull()) {
    return nullptr;
  }

  // Return specific lane of multi-queue based on priority
  auto& lane =
      shared_header_->task_queue.get()->GetLane(0, static_cast<int>(priority));
  return &lane;
}

bool IpcManager::IsInitialized() const { return is_initialized_; }

bool IpcManager::ServerInitShm() {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  ConfigManager* config = CHI_CONFIG;

  try {
    // Set backend and allocator IDs
    main_backend_id_ = hipc::MemoryBackendId::Get(0);
    client_data_backend_id_ = hipc::MemoryBackendId::Get(1);
    runtime_data_backend_id_ = hipc::MemoryBackendId::Get(2);

    main_allocator_id_ = hipc::AllocatorId(1, 0);
    client_data_allocator_id_ = hipc::AllocatorId(2, 0);
    runtime_data_allocator_id_ = hipc::AllocatorId(3, 0);

    // Create memory backends
    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        main_backend_id_,
        hshm::Unit<size_t>::Bytes(config->GetMemorySegmentSize(kMainSegment)),
        "chi_main_segment");

    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        client_data_backend_id_,
        hshm::Unit<size_t>::Bytes(
            config->GetMemorySegmentSize(kClientDataSegment)),
        "chi_client_data_segment");

    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        runtime_data_backend_id_,
        hshm::Unit<size_t>::Bytes(
            config->GetMemorySegmentSize(kRuntimeDataSegment)),
        "chi_runtime_data_segment");

    // Create allocators with custom header for main allocator
    size_t custom_header_size = sizeof(IpcSharedHeader);
    mem_manager->CreateAllocator<CHI_MAIN_ALLOC_T>(
        main_backend_id_, main_allocator_id_, custom_header_size);

    mem_manager->CreateAllocator<CHI_CDATA_ALLOC_T>(
        client_data_backend_id_, client_data_allocator_id_, 0);

    mem_manager->CreateAllocator<CHI_RDATA_ALLOC_T>(
        runtime_data_backend_id_, runtime_data_allocator_id_, 0);

    // Cache allocator pointers
    main_allocator_ =
        mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(main_allocator_id_);
    client_data_allocator_ =
        mem_manager->GetAllocator<CHI_CDATA_ALLOC_T>(client_data_allocator_id_);
    runtime_data_allocator_ = mem_manager->GetAllocator<CHI_RDATA_ALLOC_T>(
        runtime_data_allocator_id_);

    return main_allocator_ && client_data_allocator_ && runtime_data_allocator_;
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::ClientInitShm() {
  auto mem_manager = HSHM_MEMORY_MANAGER;

  try {
    // Set backend and allocator IDs (must match server)
    main_backend_id_ = hipc::MemoryBackendId::Get(0);
    client_data_backend_id_ = hipc::MemoryBackendId::Get(1);
    runtime_data_backend_id_ = hipc::MemoryBackendId::Get(2);

    main_allocator_id_ = hipc::AllocatorId(1, 0);
    client_data_allocator_id_ = hipc::AllocatorId(2, 0);
    runtime_data_allocator_id_ = hipc::AllocatorId(3, 0);

    // Clean up any existing local state first
    mem_manager->UnregisterAllocator(main_allocator_id_);
    mem_manager->UnregisterAllocator(client_data_allocator_id_);
    mem_manager->UnregisterAllocator(runtime_data_allocator_id_);
    mem_manager->DestroyBackend(main_backend_id_);
    mem_manager->DestroyBackend(client_data_backend_id_);
    mem_manager->DestroyBackend(runtime_data_backend_id_);

    // Attach to existing shared memory segments
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               "chi_main_segment");
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               "chi_client_data_segment");
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               "chi_runtime_data_segment");

    // Cache allocator pointers
    main_allocator_ =
        mem_manager->GetAllocator<CHI_MAIN_ALLOC_T>(main_allocator_id_);
    client_data_allocator_ =
        mem_manager->GetAllocator<CHI_CDATA_ALLOC_T>(client_data_allocator_id_);
    runtime_data_allocator_ = mem_manager->GetAllocator<CHI_RDATA_ALLOC_T>(
        runtime_data_allocator_id_);

    return main_allocator_ && client_data_allocator_ && runtime_data_allocator_;
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::ServerInitQueues() {
  if (!main_allocator_) {
    return false;
  }

  try {
    // Get the custom header from allocator
    shared_header_ =
        main_allocator_->template GetCustomHeader<IpcSharedHeader>();

    // Server initializes the queue in the custom header
    // Create multi-queue with 1 lane, 2 priorities (low/high latency), depth
    // 1024 Queue stores hipc::Pointer which represents .shm component of
    // FullPtr<Task>
    hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);
    shared_header_->task_queue.shm_init(ctx_alloc, 1, 2, 1024);

    // Check if queue was initialized
    return shared_header_ != nullptr &&
           shared_header_->task_queue.get() != nullptr &&
           !shared_header_->task_queue.get()->IsNull();
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::ClientInitQueues() {
  if (!main_allocator_) {
    return false;
  }

  try {
    // Get the custom header from allocator
    shared_header_ =
        main_allocator_->template GetCustomHeader<IpcSharedHeader>();

    // Client just accesses the already initialized queue
    // Client doesn't check IsNull as queue should already be initialized by
    // server
    return shared_header_ != nullptr &&
           shared_header_->task_queue.get() != nullptr;
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
        addr, hshm::lbm::Transport::kZeroMq, protocol, port);

    return zmq_server_ != nullptr;
  } catch (const std::exception& e) {
    return false;
  }
}

// No template instantiations needed - all templates are inline in header

}  // namespace chi