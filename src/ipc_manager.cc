/**
 * IPC manager implementation
 */

#include "chimaera/ipc_manager.h"

#include "chimaera/singletons.h"
#include "chimaera/task.h"
#include "chimaera/task_queue.h"

// Global pointer variable definition for IPC manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::IpcManager, g_ipc_manager);

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


void IpcManager::ClientFinalize() {
  // Client finalize does nothing for now
  // Clients should not destroy shared resources
}

void IpcManager::ServerFinalize() {
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

TaskQueue* IpcManager::GetTaskQueue() { return external_queue_.ptr_; }

void* IpcManager::GetProcessQueue(QueuePriority priority) {
  // For compatibility, return the TaskQueue as void*
  (void)priority;  // Suppress unused parameter warning
  return static_cast<void*>(GetTaskQueue());
}

bool IpcManager::IsInitialized() const { return is_initialized_; }

bool IpcManager::InitializeWorkerQueues(u32 num_workers) {
  if (!main_allocator_ || !shared_header_) {
    return false;
  }

  try {
    hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);

    // Initialize worker queues vector in shared header using delay_ar
    // Single call to initialize vector with num_workers queues, each with depth
    // 1024
    shared_header_->worker_queues.shm_init(ctx_alloc, num_workers, 1024);

    // Store worker count
    shared_header_->num_workers = num_workers;

    return true;
  } catch (const std::exception& e) {
    return false;
  }
}

hipc::FullPtr<chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>
IpcManager::GetWorkerQueue(u32 worker_id) {
  if (!shared_header_) {
    return hipc::FullPtr<
        chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>();
  }

  if (worker_id >= shared_header_->num_workers) {
    return hipc::FullPtr<
        chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>();
  }

  // Get the vector of worker queues from delay_ar
  auto& worker_queues_vector = shared_header_->worker_queues;

  if (worker_id >= worker_queues_vector->size()) {
    return hipc::FullPtr<
        chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>();
  }

  // Return FullPtr reference to the specific worker's queue in the vector
  return hipc::FullPtr<
      chi::ipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>(
      &(*worker_queues_vector)[worker_id]);
}

u32 IpcManager::GetWorkerCount() {
  if (!shared_header_) {
    return 0;
  }
  return shared_header_->num_workers;
}

bool IpcManager::ServerInitShm() {
  auto mem_manager = HSHM_MEMORY_MANAGER;
  ConfigManager* config = CHI_CONFIG_MANAGER;

  try {
    // Set backend and allocator IDs
    main_backend_id_ = hipc::MemoryBackendId::Get(0);
    client_data_backend_id_ = hipc::MemoryBackendId::Get(1);
    runtime_data_backend_id_ = hipc::MemoryBackendId::Get(2);

    main_allocator_id_ = hipc::AllocatorId(1, 0);
    client_data_allocator_id_ = hipc::AllocatorId(2, 0);
    runtime_data_allocator_id_ = hipc::AllocatorId(3, 0);

    // Create memory backends using configurable segment names
    std::string main_segment_name =
        config->GetSharedMemorySegmentName(kMainSegment);
    std::string client_data_segment_name =
        config->GetSharedMemorySegmentName(kClientDataSegment);
    std::string runtime_data_segment_name =
        config->GetSharedMemorySegmentName(kRuntimeDataSegment);

    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        main_backend_id_,
        hshm::Unit<size_t>::Bytes(config->GetMemorySegmentSize(kMainSegment)),
        main_segment_name);

    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        client_data_backend_id_,
        hshm::Unit<size_t>::Bytes(
            config->GetMemorySegmentSize(kClientDataSegment)),
        client_data_segment_name);

    mem_manager->CreateBackend<hipc::PosixShmMmap>(
        runtime_data_backend_id_,
        hshm::Unit<size_t>::Bytes(
            config->GetMemorySegmentSize(kRuntimeDataSegment)),
        runtime_data_segment_name);

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
  ConfigManager* config = CHI_CONFIG_MANAGER;

  try {
    // Set backend and allocator IDs (must match server)
    main_backend_id_ = hipc::MemoryBackendId::Get(0);
    client_data_backend_id_ = hipc::MemoryBackendId::Get(1);
    runtime_data_backend_id_ = hipc::MemoryBackendId::Get(2);

    main_allocator_id_ = hipc::AllocatorId(1, 0);
    client_data_allocator_id_ = hipc::AllocatorId(2, 0);
    runtime_data_allocator_id_ = hipc::AllocatorId(3, 0);

    // Get configurable segment names with environment variable expansion
    std::string main_segment_name =
        config->GetSharedMemorySegmentName(kMainSegment);
    std::string client_data_segment_name =
        config->GetSharedMemorySegmentName(kClientDataSegment);
    std::string runtime_data_segment_name =
        config->GetSharedMemorySegmentName(kRuntimeDataSegment);

    // Attach to existing shared memory segments created by server
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               main_segment_name);
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               client_data_segment_name);
    mem_manager->AttachBackend(hipc::MemoryBackendType::kPosixShmMmap,
                               runtime_data_segment_name);

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

    // Initialize shared header
    shared_header_->num_workers = 0;

    // Server creates the TaskQueue using delay_ar
    hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);

    // Get configured number of lanes from ConfigManager
    ConfigManager* config = CHI_CONFIG_MANAGER;
    u32 num_lanes = config->GetTaskQueueLanes();

    // Initialize TaskQueue in shared header
    shared_header_->external_queue.shm_init(
        ctx_alloc, ctx_alloc,
        num_lanes,  // num_lanes for concurrency from config
        1,          // num_priorities (single priority)
        1024);      // depth_per_queue

    // Create FullPtr reference to the shared TaskQueue
    external_queue_ =
        hipc::FullPtr<TaskQueue>(&shared_header_->external_queue.get_ref());

    // Note: WorkOrchestrator scheduling is handled by WorkOrchestrator itself,
    // not here

    return !external_queue_.IsNull();
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

    // Client accesses the server's shared TaskQueue via delay_ar
    // Create FullPtr reference to the shared TaskQueue
    external_queue_ =
        hipc::FullPtr<TaskQueue>(&shared_header_->external_queue.get_ref());

    return !external_queue_.IsNull();
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::InitializeZmqServer() {
  ConfigManager* config = CHI_CONFIG_MANAGER;

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