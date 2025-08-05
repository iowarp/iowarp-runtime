#include "chimaera/chimaera.h"

namespace chi {

bool Chimaera::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  try {
    HILOG(kInfo, "Initializing Chimaera Client...");

    auto config_manager = CHI_CONFIG_MANAGER;
    config_manager->LoadConfigFromEnv();
    HILOG(kInfo, "Configuration loaded");

    auto ipc_manager = CHI_IPC_MANAGER;
    ipc_manager->ClientInit();
    HILOG(kInfo, "IPC Manager (Client) initialized");

    is_initialized_ = true;
    is_server_ = false;
    HILOG(kInfo, "Chimaera Client initialized successfully");
    return true;

  } catch (const std::exception& e) {
    HELOG(kError, "Failed to initialize Chimaera Client: {}", e.what());
    return false;
  }
}

#ifdef CHIMAERA_RUNTIME
bool Chimaera::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  try {
    HILOG(kInfo, "Initializing Chimaera Runtime...");

    auto config_manager = CHI_CONFIG_MANAGER;
    config_manager->LoadConfigFromEnv();
    HILOG(kInfo, "Configuration loaded");

    auto ipc_manager = CHI_IPC_MANAGER;
    ipc_manager->ServerInit();
    HILOG(kInfo, "IPC Manager initialized");

    auto module_manager = CHI_MODULE_MANAGER;
    if (!module_manager->LoadDefaultModules()) {
      HELOG(kError, "Failed to load default modules");
      return false;
    }
    HILOG(kInfo, "Module Manager initialized");

    auto pool_manager = CHI_POOL_MANAGER;
    HILOG(kInfo, "Pool Manager initialized");

    auto work_orchestrator = CHI_WORK_ORCHESTRATOR;
    work_orchestrator->Initialize();
    HILOG(kInfo, "Work Orchestrator initialized");

    is_initialized_ = true;
    is_server_ = true;
    HILOG(kInfo, "Chimaera Runtime initialized successfully");
    return true;

  } catch (const std::exception& e) {
    HELOG(kError, "Failed to initialize Chimaera Runtime: {}", e.what());
    return false;
  }
}
#else
bool Chimaera::ServerInit() {
  HELOG(kError, "ServerInit() not available in client build");
  return false;
}
#endif  // CHIMAERA_RUNTIME

void Chimaera::Shutdown() {
  if (!is_initialized_) {
    return;
  }

  if (is_server_) {
#ifdef CHIMAERA_RUNTIME
    HILOG(kInfo, "Shutting down Chimaera Runtime...");

    try {
      auto work_orchestrator = CHI_WORK_ORCHESTRATOR;
      work_orchestrator->Shutdown();

      auto module_manager = CHI_MODULE_MANAGER;
      module_manager->UnloadAllModules();

      auto ipc_manager = CHI_IPC_MANAGER;
      ipc_manager->Shutdown();

      HILOG(kInfo, "Chimaera Runtime shut down successfully");

    } catch (const std::exception& e) {
      HELOG(kError, "Error during Chimaera Runtime shutdown: {}", e.what());
    }
#endif  // CHIMAERA_RUNTIME
  } else {
    HILOG(kInfo, "Shutting down Chimaera Client...");

    try {
      auto ipc_manager = CHI_IPC_MANAGER;
      ipc_manager->Shutdown();

      HILOG(kInfo, "Chimaera Client shut down successfully");

    } catch (const std::exception& e) {
      HELOG(kError, "Error during Chimaera Client shutdown: {}", e.what());
    }
  }

  is_initialized_ = false;
  is_server_ = false;
}

}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::Chimaera, chiChimaera);