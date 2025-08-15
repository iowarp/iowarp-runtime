/**
 * Chimaera manager implementation
 */

#include "chimaera/singletons.h"

// Global pointer variable definition for Chimaera manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::Chimaera, g_chimaera_manager);

namespace chi {

// HSHM Thread-local storage key definition
hshm::ThreadLocalKey chi_cur_worker_key_;

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
  if (!CHI_CONFIG_MANAGER->Init()) {
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

  // Initialize configuration manager
  if (!CHI_CONFIG_MANAGER->Init()) {
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
  if (!CHI_MODULE_MANAGER->Init()) {
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


void Chimaera::ClientFinalize() {
  if (!is_initialized_ || !is_client_mode_) {
    return;
  }

  // Finalize client components
  CHI_POOL_MANAGER->Finalize();
  CHI_IPC->ClientFinalize();

  is_initialized_ = false;
  is_client_mode_ = false;
}

void Chimaera::ServerFinalize() {
  if (!is_initialized_ || !is_runtime_mode_) {
    return;
  }

  // Stop workers and finalize server components
  CHI_WORK_ORCHESTRATOR->StopWorkers();
  CHI_WORK_ORCHESTRATOR->Finalize();
  CHI_MODULE_MANAGER->Finalize();

  // Finalize shared components
  CHI_POOL_MANAGER->Finalize();
  CHI_IPC->ServerFinalize();

  is_initialized_ = false;
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