/**
 * Chimaera manager implementation
 */

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "chimaera/singletons.h"

// Global pointer variable definition for Chimaera manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::Chimaera, g_chimaera_manager);

namespace chi {

// HSHM Thread-local storage key definitions
hshm::ThreadLocalKey chi_cur_worker_key_;
hshm::ThreadLocalKey chi_task_counter_key_;

/**
 * Create a new TaskNode with current process/thread info and next major counter
 */
TaskNode CreateTaskNode() {
#if CHIMAERA_RUNTIME
  // In runtime mode, check if we have a current worker
  Worker* current_worker = CHI_CUR_WORKER;
  if (current_worker) {
    // Get current task from worker
    FullPtr<Task> current_task = current_worker->GetCurrentTask();
    if (!current_task.IsNull()) {
      // Copy TaskNode from current task and increment minor by 1
      TaskNode new_node = current_task->task_node_;
      new_node.minor_ += 1;
      return new_node;
    }
  }
#endif

  // Fallback: Create new TaskNode using counter (client mode or no current
  // task) Get system information singleton (avoid direct dereferencing)
  auto* system_info = HSHM_SYSTEM_INFO;
  u32 pid = system_info ? system_info->pid_ : 0;

  // Get thread ID
  u32 tid = static_cast<u32>(HSHM_THREAD_MODEL->GetTid().tid_);

  // Get thread-local task counter (should be initialized during client init)
  TaskCounter* counter =
      HSHM_THREAD_MODEL->GetTls<TaskCounter>(chi_task_counter_key_);
  if (!counter) {
    // Initialize counter if not present (should not happen in client mode)
    counter = new TaskCounter();
    HSHM_THREAD_MODEL->SetTls(chi_task_counter_key_, counter);
  }

  // Get next major number
  u32 major = counter->GetNext();

  return TaskNode(pid, tid, major, 0);  // minor starts at 0 for new tasks
}

Chimaera::~Chimaera() {
  if (is_initialized_) {
    // Always finalize client components
    ClientFinalize();

#ifdef CHIMAERA_RUNTIME
    // Only finalize server components if compiled as runtime
    ServerFinalize();
#endif
  }
}

bool Chimaera::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize configuration manager
  auto* config_manager = CHI_CONFIG_MANAGER;
  if (!config_manager->Init()) {
    return false;
  }

  // Initialize IPC manager for client
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager->ClientInit()) {
    return false;
  }

  // Pool manager is not initialized in client mode
  // It's only needed for server/runtime mode


  is_client_mode_ = true;
  is_runtime_mode_ = false;
  is_initialized_ = true;

  return true;
}

bool Chimaera::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  // Identify this host from hostfile by attempting TCP server binding
  if (!IdentifyHost()) {
    std::cerr << "CRITICAL ERROR: Unable to identify current host. No TCP "
                 "server could be started."
              << std::endl;
    std::cerr << "This usually means:" << std::endl;
    std::cerr << "1. This host is not listed in the hostfile" << std::endl;
    std::cerr << "2. Network interfaces are not available" << std::endl;
    std::cerr << "3. Port 9999 is already in use" << std::endl;
    std::cerr << "Exiting runtime..." << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Host identification successful: " << current_hostname_
            << std::endl;

  // Initialize configuration manager
  auto* config_manager = CHI_CONFIG_MANAGER;
  if (!config_manager->Init()) {
    return false;
  }

  // Initialize IPC manager for server
  auto* ipc_manager = CHI_IPC;
  if (!ipc_manager->ServerInit()) {
    return false;
  }

  // Store the 64-bit node ID in shared memory header
  ipc_manager->SetNodeId(current_hostname_);
  std::cout << "Node ID stored in shared memory: 0x" << std::hex
            << ipc_manager->GetNodeId() << std::dec << std::endl;

  // Initialize module manager first (needed for admin chimod)
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager->Init()) {
    return false;
  }

  // Initialize pool manager (server mode only) after module manager
  auto* pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager->ServerInit()) {
    return false;
  }

  // Initialize work orchestrator
  auto* work_orchestrator = CHI_WORK_ORCHESTRATOR;
  if (!work_orchestrator->Init()) {
    return false;
  }

  // Start worker threads
  if (!work_orchestrator->StartWorkers()) {
    return false;
  }

  is_client_mode_ = false;
  is_runtime_mode_ = true;
  is_initialized_ = true;

  return true;
}

void Chimaera::ClientFinalize() {
  if (!is_initialized_ || !is_client_mode_) {
    return;
  }


  // Finalize client components
  auto* pool_manager = CHI_POOL_MANAGER;
  pool_manager->Finalize();
  auto* ipc_manager = CHI_IPC;
  ipc_manager->ClientFinalize();

  is_initialized_ = false;
  is_client_mode_ = false;
}

void Chimaera::ServerFinalize() {
  if (!is_initialized_ || !is_runtime_mode_) {
    return;
  }

  // Stop workers and finalize server components
  auto* work_orchestrator = CHI_WORK_ORCHESTRATOR;
  work_orchestrator->StopWorkers();
  work_orchestrator->Finalize();
  auto* module_manager = CHI_MODULE_MANAGER;
  module_manager->Finalize();

  // Finalize shared components
  auto* pool_manager = CHI_POOL_MANAGER;
  pool_manager->Finalize();
  auto* ipc_manager = CHI_IPC;
  ipc_manager->ServerFinalize();

  is_initialized_ = false;
  is_runtime_mode_ = false;
}

bool Chimaera::IsInitialized() const { return is_initialized_; }

bool Chimaera::IsClient() const { return is_client_mode_; }

bool Chimaera::IsRuntime() const { return is_runtime_mode_; }

const std::string& Chimaera::GetCurrentHostname() const {
  return current_hostname_;
}

u64 Chimaera::GetNodeId() const {
  auto* ipc_manager = CHI_IPC;
  return ipc_manager->GetNodeId();
}

bool Chimaera::IdentifyHost(const std::string& hostfile_path) {
  std::cout << "Identifying current host from hostfile: " << hostfile_path
            << std::endl;

  // Use HSHM to parse hostfile and expand patterns
  std::vector<std::string> hosts;
  try {
    hosts = hshm::ConfigParse::ParseHostfile(hostfile_path);
  } catch (const std::exception& e) {
    std::cerr << "Warning: Could not read hostfile " << hostfile_path << " ("
              << e.what() << "), trying default hosts" << std::endl;

    // Fallback to common localhost variations
    hosts = {"localhost", "127.0.0.1", "0.0.0.0"};
  }

  if (hosts.empty()) {
    std::cerr << "Warning: Empty hostfile " << hostfile_path
              << ", trying default hosts" << std::endl;
    hosts = {"localhost", "127.0.0.1", "0.0.0.0"};
  }

  std::cout << "Attempting to identify host among " << hosts.size()
            << " candidates" << std::endl;

  // Try to start TCP server on each host
  for (const auto& hostname : hosts) {
    std::cout << "Trying to bind TCP server to: " << hostname << std::endl;

    try {
      auto server = TryStartTcpServer(hostname);
      if (server != nullptr) {
        std::cout << "SUCCESS: TCP server started on " << hostname << std::endl;
        current_hostname_ = hostname;
        return true;
      }
    } catch (const std::exception& e) {
      std::cout << "Failed to bind to " << hostname << ": " << e.what()
                << std::endl;
    } catch (...) {
      std::cout << "Failed to bind to " << hostname << ": Unknown error"
                << std::endl;
    }
  }

  std::cerr << "ERROR: Could not start TCP server on any host from hostfile"
            << std::endl;
  return false;
}

std::unique_ptr<hshm::lbm::Server> Chimaera::TryStartTcpServer(
    const std::string& hostname, u32 port) {
  try {
    std::string protocol = "tcp";
    auto server = hshm::lbm::TransportFactory::GetServer(
        hostname, hshm::lbm::Transport::kZeroMq, protocol, port);

    if (server != nullptr) {
      std::cout << "TCP server successfully bound to " << hostname << ":"
                << port << std::endl;
      return server;
    }
  } catch (const std::exception& e) {
    // Exception will be caught and handled by caller
    throw;
  } catch (...) {
    throw std::runtime_error("Unknown error starting TCP server");
  }

  return nullptr;
}

}  // namespace chi