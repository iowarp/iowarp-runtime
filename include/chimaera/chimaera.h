#ifndef CHI_CHIMAERA_H_
#define CHI_CHIMAERA_H_

#include "chimaera_types.h"
#include "chimaera_task.h"
#include "chimaera_module.h"
#include "config/config_manager.h"
#include "ipc/ipc_manager.h"
#include "module_manager/module_manager.h"
#include "pool_manager/pool_manager.h"
#include "work_orchestrator/work_orchestrator.h"
#include <hermes_shm/util/singleton.h>

namespace chi {

class Chimaera {
private:
  bool is_initialized_;
  bool is_server_;

public:
  Chimaera() : is_initialized_(false), is_server_(false) {}

  bool ClientInit();
  bool ServerInit();
  void Shutdown();
  bool IsInitialized() const { return is_initialized_; }
  bool IsServer() const { return is_server_; }
};

}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::Chimaera, chiChimaera);
#define CHI_CHIMAERA \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::Chimaera, chiChimaera)
#define CHI_CHIMAERA_T chi::Chimaera*

#ifdef CHIMAERA_RUNTIME
inline bool CHIMAERA_RUNTIME_INIT() {
  return CHI_CHIMAERA->ServerInit();
}

inline void CHIMAERA_RUNTIME_SHUTDOWN() {
  CHI_CHIMAERA->Shutdown();
}
#endif  // CHIMAERA_RUNTIME

inline bool CHIMAERA_CLIENT_INIT() {
  return CHI_CHIMAERA->ClientInit();
}

namespace chi {
namespace api {

#ifdef CHIMAERA_RUNTIME
inline bool InitializeRuntime() {
  return CHIMAERA_RUNTIME_INIT();
}

inline void ShutdownRuntime() {
  CHIMAERA_RUNTIME_SHUTDOWN();
}
#endif  // CHIMAERA_RUNTIME

inline bool InitializeClient() {
  return CHIMAERA_CLIENT_INIT();
}

inline void Shutdown() {
  CHI_CHIMAERA->Shutdown();
}

inline bool IsInitialized() {
  return CHI_CHIMAERA->IsInitialized();
}

}  // namespace api
}  // namespace chi

#endif  // CHI_CHIMAERA_H_