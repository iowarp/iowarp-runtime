#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_

#include <memory>
#include "chimaera/types.h"
#include "chimaera/task_queue.h"

namespace chi {

/**
 * Custom header structure for shared memory allocator
 * Contains a pointer to the process TaskQueue
 */
struct IpcSharedHeader {
  hipc::Pointer task_queue_ptr; // Pointer to TaskQueue in shared memory
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
    if (!process_task_queue_.IsNull()) {
      // Get the shared memory pointer from the task
      hipc::Pointer shm_ptr = task_ptr.shm_;
      
      // Enqueue the pointer using round-robin across lanes
      auto& lane = process_task_queue_->GetLane(0, static_cast<u32>(priority));
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
   * Get TaskQueue for task processing
   * @return Pointer to the TaskQueue or nullptr if not available
   */
  TaskQueue* GetTaskQueue();
  
  /**
   * Get priority queue for task processing (compatibility)
   * @param priority Queue priority level
   * @return Pointer to the TaskQueue (cast as void*)
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
  
  // Pointer to shared header containing the task queue pointer
  IpcSharedHeader* shared_header_ = nullptr;
  
  // The actual TaskQueue instance 
  hipc::FullPtr<TaskQueue> process_task_queue_;
  
  // TaskQueue header (stored separately from queue)
  TaskQueueHeader process_queue_header_;
  
  // ZeroMQ server (using lightbeam)
  std::unique_ptr<hshm::lbm::Server> zmq_server_;
};

}  // namespace chi

// Macro for accessing the IPC manager singleton using HSHM singleton
#define CHI_IPC hshm::Singleton<::chi::IpcManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_