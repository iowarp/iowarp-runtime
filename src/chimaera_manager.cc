/**
 * Chimaera manager implementation
 */

#include "chimaera/singletons.h"

namespace chi {

// HSHM Thread-local storage key definitions
hshm::ThreadLocalKey chi_cur_rctx_key_;
hshm::ThreadLocalKey chi_cur_worker_key_;

bool Chimaera::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize HSHM TLS keys
  HSHM_THREAD_MODEL->CreateTls<struct RunContext>(chi_cur_rctx_key_, nullptr);
  HSHM_THREAD_MODEL->CreateTls<class Worker>(chi_cur_worker_key_, nullptr);

  // Initialize configuration manager
  if (!CHI_CONFIG->Init()) {
    return false;
  }

  // Initialize IPC manager for client
  if (!CHI_IPC->ClientInit()) {
    return false;
  }

  // Initialize pool manager
  if (!CHI_POOL_MANAGER->Init()) {
    return false;
  }

  is_client_mode_ = true;
  is_runtime_mode_ = false;
  is_initialized_ = true;

  return true;
}

bool Chimaera::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize HSHM TLS keys
  HSHM_THREAD_MODEL->CreateTls<struct RunContext>(chi_cur_rctx_key_, nullptr);
  HSHM_THREAD_MODEL->CreateTls<class Worker>(chi_cur_worker_key_, nullptr);

  // Initialize configuration manager
  if (!CHI_CONFIG->Init()) {
    return false;
  }

  // Initialize IPC manager for server
  if (!CHI_IPC->ServerInit()) {
    return false;
  }

  // Initialize pool manager
  if (!CHI_POOL_MANAGER->Init()) {
    return false;
  }

  // Initialize module manager
  if (!CHI_MODULE->Init()) {
    return false;
  }

  // Initialize work orchestrator
  if (!CHI_WORK_ORCHESTRATOR->Init()) {
    return false;
  }

  // Start worker threads
  if (!CHI_WORK_ORCHESTRATOR->StartWorkers()) {
    return false;
  }

  is_client_mode_ = false;
  is_runtime_mode_ = true;
  is_initialized_ = true;

  return true;
}

void Chimaera::Finalize() {
  if (!is_initialized_) {
    return;
  }

  // Stop workers if running
  if (is_runtime_mode_) {
    CHI_WORK_ORCHESTRATOR->StopWorkers();
    CHI_WORK_ORCHESTRATOR->Finalize();
    CHI_MODULE->Finalize();
  }

  // Finalize managers
  CHI_POOL_MANAGER->Finalize();
  CHI_IPC->Finalize();

  is_initialized_ = false;
  is_client_mode_ = false;
  is_runtime_mode_ = false;
}

bool Chimaera::IsInitialized() const {
  return is_initialized_;
}

bool Chimaera::IsClient() const {
  return is_client_mode_;
}

bool Chimaera::IsRuntime() const {
  return is_runtime_mode_;
}

}  // namespace chi