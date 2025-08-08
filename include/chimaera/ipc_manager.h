#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_

#include <memory>
#include "chimaera/types.h"

namespace chi {

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
   * @param args Constructor arguments for the task
   * @param segment Memory segment to use for allocation
   * @return FullPtr to allocated task
   */
  template<typename TaskT, typename ...Args>
  FullPtr<TaskT> NewTask(MemorySegment segment, Args&&... args);

  /**
   * Delete a task from shared memory
   * @param task_ptr FullPtr to task to delete
   * @param segment Memory segment where task was allocated
   */
  template<typename TaskT>
  void DelTask(FullPtr<TaskT>& task_ptr, MemorySegment segment);

  /**
   * Allocate buffer in specified memory segment
   * @param size Size in bytes to allocate
   * @param segment Memory segment to use
   * @return FullPtr to allocated memory
   */
  template<typename T>
  FullPtr<T> AllocateBuffer(size_t size, MemorySegment segment);

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
   * Initialize memory segments
   * @return true if successful, false otherwise
   */
  bool InitializeMemorySegments();

  /**
   * Initialize priority queues
   * @return true if successful, false otherwise
   */
  bool InitializePriorityQueues();

  /**
   * Initialize ZeroMQ server
   * @return true if successful, false otherwise
   */
  bool InitializeZmqServer();

  bool is_initialized_ = false;
  bool is_client_mode_ = false;
  
  // Memory backends
  hipc::MemoryBackendId main_backend_id_;
  hipc::MemoryBackendId client_data_backend_id_;
  hipc::MemoryBackendId runtime_data_backend_id_;
  
  // Allocator IDs for each segment
  hipc::AllocatorId main_allocator_id_;
  hipc::AllocatorId client_data_allocator_id_;
  hipc::AllocatorId runtime_data_allocator_id_;
  
  // Priority queues using HSHM multi-queue
  FullPtr<hshm::multi_mpsc_queue<u32>> task_queue_;
  
  // ZeroMQ server (using lightbeam)
  std::unique_ptr<hshm::lbm::Server> zmq_server_;
};

}  // namespace chi

// Macro for accessing the IPC manager singleton using HSHM singleton
#define CHI_IPC hshm::Singleton<IpcManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_