#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_

#include <atomic>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>
#include "chimaera/types.h"
#include "chimaera/task_queue.h"
#include "chimaera/chimaera_manager.h"

namespace chi {

/**
 * Typedef for worker queue type to simplify usage
 */
using WorkQueue = chi::ipc::mpsc_queue<hipc::TypedPointer<TaskLane>>;

/**
 * Custom header structure for shared memory allocator
 * Contains shared data structures using delay_ar for better type safety
 */
struct IpcSharedHeader {
  hipc::delay_ar<TaskQueue> external_queue; // External/Process TaskQueue in shared memory
  hipc::delay_ar<chi::ipc::vector<WorkQueue>> worker_queues; // Vector of worker active queues
  u32 num_workers; // Number of workers for which queues are allocated
  u64 node_id; // 64-bit hash of the hostname for node identification
};

/**
 * Host structure for hostfile management
 * Contains IP address and corresponding 64-bit node ID
 */
struct Host {
  std::string ip_address; // IP address as string (IPv4 or IPv6)
  u64 node_id;           // 64-bit representation of IP address

  /**
   * Default constructor
   */
  Host() : node_id(0) {}

  /**
   * Constructor with IP address
   * @param ip IP address string
   */
  explicit Host(const std::string& ip) : ip_address(ip), node_id(IpToNodeId(ip)) {}

  /**
   * Constructor with IP address and node ID
   * @param ip IP address string
   * @param id Pre-computed node ID
   */
  Host(const std::string& ip, u64 id) : ip_address(ip), node_id(id) {}

  /**
   * Convert IP address to 64-bit node ID
   * Performs DNS resolution to convert hostnames to IP addresses,
   * then converts the IP to its numeric representation
   * @param ip_str IP address string or hostname
   * @return 64-bit node ID
   */
  static u64 IpToNodeId(const std::string& ip_str);

private:
  /**
   * Resolve hostname to IP address using DNS
   * Uses getaddrinfo for portable hostname resolution
   * Prefers IPv4 over IPv6 for deterministic results
   * @param hostname Hostname or IP address string
   * @return Resolved IP address string, empty if resolution fails
   */
  static std::string ResolveHostnameToIp(const std::string& hostname);

  /**
   * Convert IP address string to 64-bit numeric representation
   * For IPv4: uses the 32-bit value as lower 32 bits of result
   * For IPv6: uses the lower 64 bits of the 128-bit address
   * @param ip_str IP address string (IPv4 or IPv6)
   * @return 64-bit numeric representation
   */
  static u64 ConvertIpToNumeric64(const std::string& ip_str);
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
   * Client finalize - does nothing for now
   */
  void ClientFinalize();

  /**
   * Server finalize - cleanup all IPC resources 
   */
  void ServerFinalize();

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
  void DelTask(FullPtr<TaskT> task_ptr) {
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
    auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
    if (chimaera_manager && chimaera_manager->IsRuntime()) {
      // Runtime uses rdata segment
      if (!runtime_data_allocator_) {
        return FullPtr<T>();
      }
      return runtime_data_allocator_->template AllocateObjs<T>(HSHM_MCTX, size);
    } else {
      // Client uses cdata segment
      if (!client_data_allocator_) {
        return FullPtr<T>();
      }
      return client_data_allocator_->template AllocateObjs<T>(HSHM_MCTX, size);
    }
  }

  /**
   * Free buffer from appropriate memory segment
   * Client uses cdata segment, runtime uses rdata segment
   * @param buffer_ptr FullPtr to buffer to free
   */
  template<typename T>
  void FreeBuffer(FullPtr<T> buffer_ptr) {
    if (buffer_ptr.IsNull()) {
      return;
    }

    auto* chimaera_manager = CHI_CHIMAERA_MANAGER;
    if (chimaera_manager->IsRuntime()) {
      // Runtime uses rdata segment
      runtime_data_allocator_->Free(HSHM_MCTX, buffer_ptr);
    } else {
      // Client uses cdata segment
      client_data_allocator_->Free(HSHM_MCTX, buffer_ptr);
    }
  }

  /**
   * Free buffer from appropriate memory segment (hipc::Pointer overload)
   * Converts hipc::Pointer to FullPtr<char> and calls the main FreeBuffer
   * @param buffer_ptr hipc::Pointer to buffer to free
   */
  void FreeBuffer(hipc::Pointer buffer_ptr) {
    if (buffer_ptr.IsNull()) {
      return;
    }
    // Convert hipc::Pointer to FullPtr<char> and call main FreeBuffer
    hipc::FullPtr<char> full_ptr(buffer_ptr);
    FreeBuffer(full_ptr);
  }

  /**
   * Enqueue task to process queue (priority 0 - normal tasks)
   * Uses the configured lane mapping policy to select the target lane
   * @param task_ptr Task to enqueue
   */
  template<typename TaskT>
  void Enqueue(FullPtr<TaskT>& task_ptr) {
    if (!external_queue_.IsNull() && external_queue_.ptr_) {
      // Create TypedPointer from the task FullPtr
      hipc::TypedPointer<Task> typed_ptr(task_ptr.shm_);

      u32 num_lanes = external_queue_->GetNumLanes();
      if (num_lanes == 0) return; // Avoid division by zero

      // Map task to lane using configured policy
      LaneId lane_id = MapTaskToLane(num_lanes);

      // Get lane as FullPtr and use TaskQueue's EmplaceTask method
      // Priority 0 for normal task submission
      auto& lane_ref = external_queue_->GetLane(lane_id, 0);
      hipc::FullPtr<TaskLane> lane_ptr(&lane_ref);
      ::chi::TaskQueue::EmplaceTask(lane_ptr, typed_ptr);
    }
  }

  /**
   * Get TaskQueue for task processing
   * @return Pointer to the TaskQueue or nullptr if not available
   */
  TaskQueue* GetTaskQueue();
  
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
  hipc::FullPtr<WorkQueue> GetWorkerQueue(u32 worker_id);

  /**
   * Get number of workers from shared memory header
   * @return Number of workers, 0 if not initialized
   */
  u32 GetWorkerCount();

  /**
   * Set the node ID in the shared memory header
   * @param hostname Hostname string to hash and store
   */
  void SetNodeId(const std::string& hostname);

  /**
   * Get the node ID from the shared memory header
   * @return 64-bit node ID, 0 if not initialized
   */
  u64 GetNodeId() const;

  /**
   * Load hostfile and populate hostfile map
   * Uses hostfile path from ConfigManager
   * @return true if loaded successfully, false otherwise
   */
  bool LoadHostfile();

  /**
   * Get Host struct by node ID
   * @param node_id 64-bit node ID
   * @return Pointer to Host struct if found, nullptr otherwise
   */
  const Host* GetHost(u64 node_id) const;

  /**
   * Get Host struct by IP address
   * @param ip_address IP address string
   * @return Pointer to Host struct if found, nullptr otherwise
   */
  const Host* GetHostByIp(const std::string& ip_address) const;

  /**
   * Get all hosts from hostfile
   * @return Vector of all Host structs
   */
  std::vector<Host> GetAllHosts() const;

  /**
   * Identify current host from hostfile by attempting TCP server binding
   * Uses hostfile path from ConfigManager
   * @return true if host identified successfully, false otherwise
   */
  bool IdentifyThisHost();

  /**
   * Get current hostname identified during host identification
   * @return Current hostname string
   */
  const std::string& GetCurrentHostname() const;

  /**
   * Set lane mapping policy for task distribution
   * @param policy Lane mapping policy to use
   */
  void SetLaneMapPolicy(LaneMapPolicy policy);

  /**
   * Get current lane mapping policy
   * @return Current lane mapping policy
   */
  LaneMapPolicy GetLaneMapPolicy() const;

  /**
   * Get the main ZeroMQ server for network communication
   * @return Pointer to main server or nullptr if not initialized
   */
  hshm::lbm::Server* GetMainServer() const;

  /**
   * Get the underlying ZeroMQ socket from the main server
   * @return ZeroMQ socket handle or nullptr if not initialized
   */
  void* GetMainZmqSocket() const;

  /**
   * Get the underlying ZeroMQ context from the main server
   * @return ZeroMQ context handle or nullptr if not initialized
   */
  void* GetMainZmqContext() const;

 private:
  /**
   * Map task to lane ID using the configured policy
   * Dispatches to the appropriate policy-specific function
   * @param num_lanes Number of available lanes
   * @return Lane ID to use
   */
  LaneId MapTaskToLane(u32 num_lanes);

  /**
   * Map task to lane by PID+TID hash
   * @param num_lanes Number of available lanes
   * @return Lane ID to use
   */
  LaneId MapByPidTid(u32 num_lanes);

  /**
   * Map task to lane using round-robin
   * @param num_lanes Number of available lanes
   * @return Lane ID to use
   */
  LaneId MapRoundRobin(u32 num_lanes);

  /**
   * Map task to lane randomly
   * @param num_lanes Number of available lanes
   * @return Lane ID to use
   */
  LaneId MapRandom(u32 num_lanes);

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
   * Start local ZeroMQ server
   * Uses ZMQ port + 1 for local server operations
   * @return true if successful, false otherwise
   */
  bool StartLocalServer();

  /**
   * Test connection to local server
   * Creates lightbeam client and attempts connection to local server
   * @return true if connection successful, false otherwise
   */
  bool TestLocalServer();

  /**
   * Compute 64-bit hash of hostname string
   * @param hostname Hostname string to hash
   * @return 64-bit hash value
   */
  static u64 ComputeNodeIdHash(const std::string& hostname);

  /**
   * Try to start main server on given hostname
   * Helper method for host identification
   * Uses ZMQ port from ConfigManager and sets main_server_
   * @param hostname Hostname to bind to
   * @return true if server started successfully, false otherwise
   */
  bool TryStartMainServer(const std::string& hostname);


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
  hipc::FullPtr<TaskQueue> external_queue_;
  
  // Local ZeroMQ server (using lightbeam)
  std::unique_ptr<hshm::lbm::Server> local_server_;
  
  // Main ZeroMQ server for distributed communication
  std::unique_ptr<hshm::lbm::Server> main_server_;

  // Direct ZeroMQ access (for multi-part message support)
  void* zmq_main_context_ = nullptr;  // ZeroMQ context for main server
  void* zmq_main_socket_ = nullptr;   // ZeroMQ socket for main server

  // Hostfile management
  std::unordered_map<u64, Host> hostfile_map_; // Map node_id -> Host
  Host this_host_; // Identified host for this node

  // Lane mapping policy
  LaneMapPolicy lane_map_policy_ = LaneMapPolicy::kRoundRobin;
  std::atomic<u32> round_robin_counter_{0};  // Counter for round-robin policy
};

}  // namespace chi

// Global pointer variable declaration for IPC manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_H(chi::IpcManager, g_ipc_manager);

// Macro for accessing the IPC manager singleton using global pointer variable
#define CHI_IPC HSHM_GET_GLOBAL_PTR_VAR(::chi::IpcManager, g_ipc_manager)

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_IPC_MANAGER_H_