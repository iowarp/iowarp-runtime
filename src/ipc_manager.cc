/**
 * IPC manager implementation
 */

#include "chimaera/ipc_manager.h"

#include "chimaera/config_manager.h"
#include "chimaera/task_queue.h"
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <endian.h>

// Global pointer variable definition for IPC manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::IpcManager, g_ipc_manager);

namespace chi {

// Host struct methods

u64 Host::IpToNodeId(const std::string& ip_str) {
  // Handle empty string case
  if (ip_str.empty()) {
    return 0;
  }
  
  // Step 1: Resolve hostname to IP address using portable getaddrinfo
  std::string resolved_ip = ResolveHostnameToIp(ip_str);
  if (resolved_ip.empty()) {
    // If resolution fails, fall back to string hashing for consistency
    std::cout << "Warning: Failed to resolve hostname '" << ip_str 
              << "', falling back to string hash" << std::endl;
    std::hash<std::string> hasher;
    return static_cast<u64>(hasher(ip_str));
  }
  
  // Step 2: Convert resolved IP address to 64-bit numeric representation
  return ConvertIpToNumeric64(resolved_ip);
}

std::string Host::ResolveHostnameToIp(const std::string& hostname) {
  struct addrinfo hints, *result;
  std::memset(&hints, 0, sizeof(hints));
  
  // Allow both IPv4 and IPv6, prefer IPv4 for deterministic results
  hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP socket type
  hints.ai_flags = AI_ADDRCONFIG;  // Return addresses only if interfaces configured
  
  int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
  if (status != 0) {
    // getaddrinfo failed - hostname resolution failed
    return std::string();
  }
  
  std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> 
      result_guard(result, freeaddrinfo);
  
  // Iterate through results, prefer IPv4 for deterministic ordering
  struct addrinfo* ipv4_addr = nullptr;
  struct addrinfo* ipv6_addr = nullptr;
  
  for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    if (rp->ai_family == AF_INET && !ipv4_addr) {
      ipv4_addr = rp;  // Store first IPv4 address
    } else if (rp->ai_family == AF_INET6 && !ipv6_addr) {
      ipv6_addr = rp;  // Store first IPv6 address  
    }
  }
  
  // Prefer IPv4 for deterministic results, fall back to IPv6
  struct addrinfo* chosen_addr = ipv4_addr ? ipv4_addr : ipv6_addr;
  if (!chosen_addr) {
    return std::string();
  }
  
  // Convert socket address to string representation
  char ip_str[INET6_ADDRSTRLEN];
  if (chosen_addr->ai_family == AF_INET) {
    struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(chosen_addr->ai_addr);
    inet_ntop(AF_INET, &sin->sin_addr, ip_str, INET_ADDRSTRLEN);
  } else if (chosen_addr->ai_family == AF_INET6) {
    struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(chosen_addr->ai_addr);
    inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, INET6_ADDRSTRLEN);
  } else {
    return std::string();
  }
  
  return std::string(ip_str);
}

u64 Host::ConvertIpToNumeric64(const std::string& ip_str) {
  // Try IPv4 first
  struct in_addr ipv4_addr;
  if (inet_pton(AF_INET, ip_str.c_str(), &ipv4_addr) == 1) {
    // IPv4 address: use 32-bit network byte order value as lower 32 bits
    // Upper 32 bits are 0, ensuring IPv4 addresses don't collide with IPv6
    u32 ipv4_numeric = ntohl(ipv4_addr.s_addr);
    return static_cast<u64>(ipv4_numeric);
  }
  
  // Try IPv6
  struct in6_addr ipv6_addr;
  if (inet_pton(AF_INET6, ip_str.c_str(), &ipv6_addr) == 1) {
    // IPv6 address: use lower 64 bits of the 128-bit address
    // This provides good distribution while fitting in 64 bits
    u64 result = 0;
    std::memcpy(&result, &ipv6_addr.s6_addr[8], 8);  // Copy bytes 8-15 (lower 64 bits)
    
    // Convert from network byte order to host byte order
    return be64toh(result);
  }
  
  // If neither IPv4 nor IPv6 parsing succeeded, fall back to string hash
  std::cout << "Warning: IP address '" << ip_str 
            << "' is not valid IPv4 or IPv6, using string hash" << std::endl;
  std::hash<std::string> hasher;
  return static_cast<u64>(hasher(ip_str));
}

// IpcManager methods

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

  // Retrieve node ID from shared header and store in this_host_
  if (shared_header_) {
    this_host_.node_id = shared_header_->node_id;
    std::cout << "Retrieved node ID from shared memory: 0x" << std::hex 
              << this_host_.node_id << std::dec << std::endl;
  } else {
    std::cerr << "Warning: Could not access shared header during ClientInit" << std::endl;
    this_host_ = Host(); // Default constructor gives node_id = 0
  }

  // Test connection to local server - critical for client functionality
  if (!TestLocalServer()) {
    std::cerr << "CRITICAL ERROR: Cannot connect to local server." << std::endl;
    std::cerr << "This usually means:" << std::endl;
    std::cerr << "1. Chimaera runtime is not running" << std::endl;
    std::cerr << "2. Local server failed to start" << std::endl;
    std::cerr << "3. Network connectivity issues" << std::endl;
    std::cerr << "Client initialization failed. Exiting." << std::endl;
    return false;
  }

  // Initialize HSHM TLS key for task counter
  HSHM_THREAD_MODEL->CreateTls<TaskCounter>(chi_task_counter_key_, nullptr);

  // Initialize thread-local task counter for this client thread
  auto* counter = new TaskCounter();
  HSHM_THREAD_MODEL->SetTls(chi_task_counter_key_, counter);

  // Set current worker to null for client-only mode
  HSHM_THREAD_MODEL->SetTls(chi_cur_worker_key_, static_cast<Worker*>(nullptr));

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

  // Identify this host and store node ID in shared header
  if (!IdentifyThisHost()) {
    std::cerr << "Warning: Could not identify host, using default node ID" << std::endl;
    this_host_ = Host(); // Default constructor gives node_id = 0
    if (shared_header_) {
      shared_header_->node_id = this_host_.node_id;
    }
  } else {
    // Store the identified host's node ID in shared header
    if (shared_header_) {
      shared_header_->node_id = this_host_.node_id;
    }
    
    std::cout << "Node ID stored in shared memory: 0x" << std::hex 
              << this_host_.node_id << std::dec << std::endl;
  }

  // Initialize HSHM TLS key for task counter (needed for CreateTaskNode in runtime)
  HSHM_THREAD_MODEL->CreateTls<TaskCounter>(chi_task_counter_key_, nullptr);

  // Start local ZeroMQ server (optional - failure is non-fatal)
  StartLocalServer();

  is_initialized_ = true;
  return true;
}


void IpcManager::ClientFinalize() {
  // Clean up thread-local task counter
  TaskCounter* counter =
      HSHM_THREAD_MODEL->GetTls<TaskCounter>(chi_task_counter_key_);
  if (counter) {
    delete counter;
    HSHM_THREAD_MODEL->SetTls(chi_task_counter_key_,
                              static_cast<TaskCounter*>(nullptr));
  }

  // Clients should not destroy shared resources
}

void IpcManager::ServerFinalize() {
  if (!is_initialized_) {
    return;
  }

  auto mem_manager = HSHM_MEMORY_MANAGER;

  // Cleanup servers
  local_server_.reset();
  main_server_.reset();

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

hipc::FullPtr<WorkQueue>
IpcManager::GetWorkerQueue(u32 worker_id) {
  if (!shared_header_) {
    return hipc::FullPtr<WorkQueue>::GetNull();
  }

  if (worker_id >= shared_header_->num_workers) {
    return hipc::FullPtr<WorkQueue>::GetNull();
  }

  // Get the vector of worker queues from delay_ar
  auto& worker_queues_vector = shared_header_->worker_queues;

  if (worker_id >= worker_queues_vector->size()) {
    return hipc::FullPtr<WorkQueue>::GetNull();
  }

  // Return FullPtr reference to the specific worker's queue in the vector
  return hipc::FullPtr<WorkQueue>(&(*worker_queues_vector)[worker_id]);
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
    shared_header_->node_id = 0; // Will be set after host identification

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

bool IpcManager::StartLocalServer() {
  ConfigManager* config = CHI_CONFIG_MANAGER;

  try {
    // Start local ZeroMQ server using HSHM Lightbeam
    std::string addr = "127.0.0.1";
    std::string protocol = "tcp";
    u32 port = config->GetZmqPort() + 1; // Use ZMQ port + 1 for local server

    local_server_ = hshm::lbm::TransportFactory::GetServer(
        addr, hshm::lbm::Transport::kZeroMq, protocol, port);

    return local_server_ != nullptr;
  } catch (const std::exception& e) {
    return false;
  }
}

bool IpcManager::TestLocalServer() {
  ConfigManager* config = CHI_CONFIG_MANAGER;
  
  try {
    // Create lightbeam client to test connection to local server
    std::string addr = "127.0.0.1";
    std::string protocol = "tcp";
    u32 port = config->GetZmqPort() + 1; // Use ZMQ port + 1 to match local server
    
    auto client = hshm::lbm::TransportFactory::GetClient(
        addr, hshm::lbm::Transport::kZeroMq, protocol, port);
    
    if (client != nullptr) {
      std::cout << "Successfully connected to local server at " << addr << ":" << port << std::endl;
      return true;
    } else {
      std::cerr << "Failed to create client connection to local server" << std::endl;
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception while testing local server connection: " << e.what() << std::endl;
    return false;
  } catch (...) {
    std::cerr << "Unknown error while testing local server connection" << std::endl;
    return false;
  }
}

void IpcManager::SetNodeId(const std::string& hostname) {
  if (!shared_header_) {
    return;
  }
  
  shared_header_->node_id = ComputeNodeIdHash(hostname);
}

u64 IpcManager::GetNodeId() const {
  // Return the node ID from the identified host
  return this_host_.node_id;
}

u64 IpcManager::ComputeNodeIdHash(const std::string& hostname) {
  // Use std::hash<std::string> to generate a 64-bit hash of the hostname
  std::hash<std::string> hasher;
  return static_cast<u64>(hasher(hostname));
}

bool IpcManager::LoadHostfile() {
  ConfigManager* config = CHI_CONFIG_MANAGER;
  std::string hostfile_path = config->GetHostfilePath();
  
  // Clear existing hostfile map
  hostfile_map_.clear();
  
  if (hostfile_path.empty()) {
    // No hostfile configured - assume localhost
    std::cout << "No hostfile configured, using localhost" << std::endl;
    std::vector<std::string> default_hosts = {"localhost", "127.0.0.1", "0.0.0.0"};
    for (const auto& ip : default_hosts) {
      Host host(ip);
      hostfile_map_[host.node_id] = host;
    }
    return true;
  }
  
  try {
    // Use HSHM to parse hostfile
    std::vector<std::string> host_ips = hshm::ConfigParse::ParseHostfile(hostfile_path);
    
    // Create Host structs and populate map
    for (const auto& ip : host_ips) {
      Host host(ip);
      hostfile_map_[host.node_id] = host;
    }
    
    std::cout << "Loaded " << hostfile_map_.size() << " hosts from hostfile: " 
              << hostfile_path << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "Error loading hostfile " << hostfile_path << ": " 
              << e.what() << std::endl;
    return false;
  }
}

const Host* IpcManager::GetHost(u64 node_id) const {
  auto it = hostfile_map_.find(node_id);
  return (it != hostfile_map_.end()) ? &it->second : nullptr;
}

const Host* IpcManager::GetHostByIp(const std::string& ip_address) const {
  u64 node_id = Host::IpToNodeId(ip_address);
  return GetHost(node_id);
}

std::vector<Host> IpcManager::GetAllHosts() const {
  std::vector<Host> hosts;
  hosts.reserve(hostfile_map_.size());
  
  for (const auto& pair : hostfile_map_) {
    hosts.push_back(pair.second);
  }
  
  return hosts;
}

bool IpcManager::IdentifyThisHost() {
  std::cout << "Identifying current host" << std::endl;
  
  // Load hostfile if not already loaded
  if (hostfile_map_.empty()) {
    if (!LoadHostfile()) {
      std::cerr << "Error: Failed to load hostfile" << std::endl;
      return false;
    }
  }
  
  if (hostfile_map_.empty()) {
    std::cerr << "ERROR: No hosts available for identification" << std::endl;
    return false;
  }
  
  std::cout << "Attempting to identify host among " << hostfile_map_.size() 
            << " candidates" << std::endl;
  
  // Try to start TCP server on each host IP
  for (const auto& pair : hostfile_map_) {
    const Host& host = pair.second;
    std::cout << "Trying to bind TCP server to: " << host.ip_address << std::endl;
    
    try {
      if (TryStartMainServer(host.ip_address)) {
        std::cout << "SUCCESS: Main server started on " << host.ip_address << std::endl;
        this_host_ = host;
        return true;
      }
    } catch (const std::exception& e) {
      std::cout << "Failed to bind to " << host.ip_address << ": " << e.what() << std::endl;
    } catch (...) {
      std::cout << "Failed to bind to " << host.ip_address << ": Unknown error" << std::endl;
    }
  }
  
  std::cerr << "ERROR: Could not start TCP server on any host from hostfile" << std::endl;
  return false;
}

const std::string& IpcManager::GetCurrentHostname() const {
  return this_host_.ip_address;
}

bool IpcManager::TryStartMainServer(const std::string& hostname) {
  ConfigManager* config = CHI_CONFIG_MANAGER;
  
  try {
    // Try to start main server using HSHM Lightbeam
    std::string protocol = "tcp";
    u32 port = config->GetZmqPort(); // Use ZMQ port for main server
    
    main_server_ = hshm::lbm::TransportFactory::GetServer(
        hostname, hshm::lbm::Transport::kZeroMq, protocol, port);
    
    if (main_server_ != nullptr) {
      std::cout << "Main server successfully bound to " << hostname << ":" << port << std::endl;
      return true;
    }
    
    return false;
  } catch (const std::exception& e) {
    // Exception will be caught and handled by caller
    throw;
  } catch (...) {
    throw std::runtime_error("Unknown error starting main server");
  }
}

// No template instantiations needed - all templates are inline in header

}  // namespace chi