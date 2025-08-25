#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_

#include "chimaera/types.h"
#include <memory>
#include <string>

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
   * Destructor - handles automatic finalization
   */
  ~Chimaera();

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
   * Finalize client components only
   */
  void ClientFinalize();

  /**
   * Finalize server/runtime components only
   */
  void ServerFinalize();

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

  /**
   * Get the current hostname identified during initialization
   * @return hostname string
   */
  const std::string& GetCurrentHostname() const;

  /**
   * Get the 64-bit node ID stored in shared memory
   * @return 64-bit node ID, 0 if not available
   */
  u64 GetNodeId() const;

 private:
  bool is_initialized_ = false;
  bool is_client_mode_ = false;
  bool is_runtime_mode_ = false;
  std::string current_hostname_;

  /**
   * Identify current host from hostfile by attempting TCP server binding
   * @param hostfile_path Path to the hostfile containing host patterns
   * @return true if host identification successful, false otherwise
   */
  bool IdentifyHost(const std::string& hostfile_path = "/etc/chimaera/hostfile");
  
  
  /**
   * Try to start TCP server on specified hostname and port
   * @param hostname Hostname to bind to
   * @param port Port number to bind to
   * @return unique_ptr to server if successful, nullptr if failed
   */
  std::unique_ptr<hshm::lbm::Server> TryStartTcpServer(const std::string& hostname, u32 port = 9999);
};

}  // namespace chi

// Global pointer variable declaration for Chimaera manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_H(chi::Chimaera, g_chimaera_manager);

// Macro for accessing the Chimaera manager singleton using global pointer variable
#define CHI_CHIMAERA_MANAGER HSHM_GET_GLOBAL_PTR_VAR(::chi::Chimaera, g_chimaera_manager)

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_CHIMAERA_MANAGER_H_