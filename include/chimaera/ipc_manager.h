#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_

#include <memory>
#include "chimaera/types.h"
#include "hermes_shm/data_structures/internal/shm_archive.h"

namespace chi {

/**
 * Custom header structure for shared memory allocator
 * Contains the process queue as a delay_ar
 */
struct IpcSharedHeader {
  hipc::delay_ar<hshm::multi_mpsc_queue<hipc::Pointer>> task_queue;
};

/**
 * IPC Manager singleton for inter-process communication
 * 
 * Manages ZeroMQ server using lightbeam from HSHM, three memory segments,
 * and priority queues for task processing.
 * Uses HSHM global cross pointer variable singleton pattern.
 */
class IpcManager {
 public:
  /**
   * Initialize client components
   * @return true if initialization successful, false otherwise
   */
  bool ClientInit();

  /**
   * Initialize server/runtime components
   * @return true if initialization successful, false otherwise
   */
  bool ServerInit();

  /**
   * Finalize and cleanup IPC resources
   */
  void Finalize();

  /**
   * Create a new task in shared memory
   * @param segment Memory segment to use for allocation
   * @param args Constructor arguments for the task
   * @return FullPtr to allocated task
   */
  template<typename TaskT, typename MemCtxT, typename ...Args>
  FullPtr<TaskT> NewTask(MemorySegment segment, MemCtxT&& mem_ctx, Args&&... args) {
    auto* alloc = GetAllocatorForSegment<TaskT>(segment);
    if (!alloc) {
      return FullPtr<TaskT>();
    }
    
    hipc::CtxAllocator<std::remove_pointer_t<decltype(alloc)>> ctx_alloc(HSHM_MCTX, alloc);
    return alloc->template NewObj<TaskT>(HSHM_MCTX, ctx_alloc, std::forward<Args>(args)...);
  }

  /**
   * Delete a task from shared memory
   * @param task_ptr FullPtr to task to delete
   * @param segment Memory segment where task was allocated
   */
  template<typename TaskT>
  void DelTask(FullPtr<TaskT>& task_ptr, MemorySegment segment) {
    if (task_ptr.IsNull()) return;
    
    auto* alloc = GetAllocatorForSegment<TaskT>(segment);
    if (alloc) {
      alloc->template DelObj(HSHM_MCTX, task_ptr);
    }
  }

  /**
   * Allocate buffer in specified memory segment
   * @param size Size in bytes to allocate
   * @param segment Memory segment to use
   * @return FullPtr to allocated memory
   */
  template<typename T>
  FullPtr<T> AllocateBuffer(size_t size, MemorySegment segment) {
    auto* alloc = GetAllocatorForSegment<T>(segment);
    if (!alloc) {
      return FullPtr<T>();
    }
    
    return alloc->template AllocateObjs<T>(HSHM_MCTX, size);
  }

  /**
   * Enqueue task to process queue
   * @param task_ptr Task to enqueue  
   * @param priority Queue priority level
   */
  template<typename TaskT>
  void Enqueue(FullPtr<TaskT>& task_ptr, QueuePriority priority = kLowLatency) {
    if (shared_header_ && shared_header_->task_queue.get() && 
        !shared_header_->task_queue.get()->IsNull()) {
      // Get the shared memory pointer from the task
      hipc::Pointer shm_ptr = task_ptr.shm_;
      
      // Enqueue the pointer to the appropriate priority queue
      auto& lane = shared_header_->task_queue.get()->GetLane(0, static_cast<int>(priority));
      lane.push(shm_ptr);
    }
  }

  /**
   * Dequeue task from process queue
   * @param priority Queue priority level
   * @return hipc::Pointer to task, null if queue empty
   */
  hipc::Pointer Dequeue(QueuePriority priority = kLowLatency);

  /**
   * Get priority queue for task processing
   * @param priority Queue priority level
   * @return Pointer to the priority queue (stub)
   */
  void* GetProcessQueue(QueuePriority priority);

  /**
   * Check if IPC manager is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

 private:
  /**
   * Initialize memory segments for server
   * @return true if successful, false otherwise
   */
  bool ServerInitShm();

  /**
   * Initialize memory segments for client
   * @return true if successful, false otherwise
   */
  bool ClientInitShm();

  /**
   * Initialize priority queues for server
   * @return true if successful, false otherwise
   */
  bool ServerInitQueues();

  /**
   * Initialize priority queues for client
   * @return true if successful, false otherwise
   */
  bool ClientInitQueues();

  /**
   * Initialize ZeroMQ server
   * @return true if successful, false otherwise
   */
  bool InitializeZmqServer();

  /**
   * Get allocator pointer for the specified segment
   * @param segment Memory segment identifier
   * @return Pointer to allocator or nullptr if invalid
   */
  template<typename TaskT = void>
  auto* GetAllocatorForSegment(MemorySegment segment) {
    switch (segment) {
      case kMainSegment:
        return main_allocator_;
      case kClientDataSegment:
        return client_data_allocator_;
      case kRuntimeDataSegment:
        return runtime_data_allocator_;
      default:
        return static_cast<CHI_MAIN_ALLOC_T*>(nullptr);
    }
  }

  bool is_initialized_ = false;
  
  // Memory backends
  hipc::MemoryBackendId main_backend_id_;
  hipc::MemoryBackendId client_data_backend_id_;
  hipc::MemoryBackendId runtime_data_backend_id_;
  
  // Allocator IDs for each segment
  hipc::AllocatorId main_allocator_id_;
  hipc::AllocatorId client_data_allocator_id_;
  hipc::AllocatorId runtime_data_allocator_id_;
  
  // Cached allocator pointers for performance
  CHI_MAIN_ALLOC_T* main_allocator_ = nullptr;
  CHI_CDATA_ALLOC_T* client_data_allocator_ = nullptr;
  CHI_RDATA_ALLOC_T* runtime_data_allocator_ = nullptr;
  
  // Pointer to shared header containing the task queue
  IpcSharedHeader* shared_header_ = nullptr;
  
  // ZeroMQ server (using lightbeam)
  std::unique_ptr<hshm::lbm::Server> zmq_server_;
};

}  // namespace chi

// Macro for accessing the IPC manager singleton using HSHM singleton
#define CHI_IPC hshm::Singleton<::chi::IpcManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_