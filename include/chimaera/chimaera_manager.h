#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_

#include "chimaera/types.h"

namespace chi {

/**
 * Main Chimaera manager singleton class
 * 
 * Central coordinator for the distributed task execution framework.
 * Manages initialization and coordination between client and runtime modes.
 * Uses HSHM global cross pointer variable singleton pattern.
 */
class Chimaera {
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
   * Finalize and cleanup all components
   */
  void Finalize();

  /**
   * Check if Chimaera is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

  /**
   * Check if running in client mode
   * @return true if client mode, false otherwise
   */
  bool IsClient() const;

  /**
   * Check if running in runtime/server mode
   * @return true if runtime mode, false otherwise
   */
  bool IsRuntime() const;

 private:
  bool is_initialized_ = false;
  bool is_client_mode_ = false;
  bool is_runtime_mode_ = false;
};

}  // namespace chi

// Macro for accessing the Chimaera manager singleton using HSHM singleton
#define CHI_CHIMAERA hshm::Singleton<Chimaera>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_