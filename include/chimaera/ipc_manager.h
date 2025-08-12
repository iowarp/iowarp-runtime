#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_

#include <memory>
#include "chimaera/types.h"
#include "chimaera/task_queue.h"

namespace chi {

/**
 * Custom header structure for shared memory allocator
 * Contains shared data structures using delay_ar for better type safety
 */
struct IpcSharedHeader {
  hipc::delay_ar<TaskQueue> external_queue; // External/Process TaskQueue in shared memory
  hipc::delay_ar<hipc::vector<hipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>>> worker_queues; // Vector of worker active queues
  u32 num_workers; // Number of workers for which queues are allocated
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
   * Create a new task in shared memory (always uses main segment)
   * @param args Constructor arguments for the task
   * @return FullPtr to allocated task
   */
  template<typename TaskT, typename ...Args>
  FullPtr<TaskT> NewTask(Args&&... args) {
    if (!main_allocator_) {
      return FullPtr<TaskT>();
    }
    
    hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);
    return main_allocator_->template NewObj<TaskT>(HSHM_MCTX, ctx_alloc, std::forward<Args>(args)...);
  }

  /**
   * Delete a task from shared memory (always uses main segment)
   * @param task_ptr FullPtr to task to delete
   */
  template<typename TaskT>
  void DelTask(FullPtr<TaskT>& task_ptr) {
    if (task_ptr.IsNull() || !main_allocator_) return;
    
    main_allocator_->template DelObj(HSHM_MCTX, task_ptr);
  }

  /**
   * Allocate buffer in appropriate memory segment
   * Client uses cdata segment, runtime uses rdata segment
   * @param size Size in bytes to allocate
   * @return FullPtr to allocated memory
   */
  template<typename T>
  FullPtr<T> AllocateBuffer(size_t size) {
    #ifdef CHIMAERA_RUNTIME
    // Runtime uses rdata segment
    if (!runtime_data_allocator_) {
      return FullPtr<T>();
    }
    return runtime_data_allocator_->template AllocateObjs<T>(size);
    #else
    // Client uses cdata segment
    if (!client_data_allocator_) {
      return FullPtr<T>();
    }
    return client_data_allocator_->template AllocateObjs<T>(size);
    #endif
  }

  /**
   * Enqueue task to process queue
   * @param task_ptr Task to enqueue  
   * @param priority Queue priority level
   */
  template<typename TaskT>
  void Enqueue(FullPtr<TaskT>& task_ptr, QueuePriority priority = kLowLatency) {
    if (!process_external_queue_.IsNull()) {
      // Create TypedPointer from the task FullPtr
      hipc::TypedPointer<Task> typed_ptr(task_ptr.shm_);
      
      // Enqueue the TypedPointer using round-robin across lanes
      auto& lane = process_external_queue_->GetLane(0, static_cast<u32>(priority));
      lane.push(typed_ptr);
    }
  }

  /**
   * Dequeue task from process queue
   * @param priority Queue priority level
   * @return hipc::TypedPointer to task, null if queue empty
   */
  hipc::TypedPointer<Task> Dequeue(QueuePriority priority = kLowLatency);

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

  /**
   * Get main allocator for creating worker queues
   * @return Pointer to main allocator or nullptr if not available
   */
  CHI_MAIN_ALLOC_T* GetMainAllocator() { return main_allocator_; }

  /**
   * Initialize worker queues in shared memory
   * @param num_workers Number of worker queues to create
   * @return true if initialization successful, false otherwise
   */
  bool InitializeWorkerQueues(u32 num_workers);

  /**
   * Get worker active queue by worker ID
   * @param worker_id Worker identifier (0-based)
   * @return FullPtr to worker's active queue or null if invalid
   */
  hipc::FullPtr<hipc::mpsc_queue<hipc::FullPtr<TaskQueue::TaskLane>>> GetWorkerQueue(u32 worker_id);

  /**
   * Get number of workers from shared memory header
   * @return Number of workers, 0 if not initialized
   */
  u32 GetWorkerCount();

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
  
  // The actual external TaskQueue instance 
  hipc::FullPtr<TaskQueue> process_external_queue_;
  
  // TaskQueue header (stored separately from queue)
  TaskQueueHeader process_queue_header_;
  
  // ZeroMQ server (using lightbeam)
  std::unique_ptr<hshm::lbm::Server> zmq_server_;
};

}  // namespace chi

// Macro for accessing the IPC manager singleton using HSHM singleton
#define CHI_IPC hshm::Singleton<::chi::IpcManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_