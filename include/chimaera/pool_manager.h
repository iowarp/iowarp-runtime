#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_

#include <unordered_map>
#include "chimaera/types.h"

namespace chi {

// Forward declarations for ChiMod system
#ifndef CHIMAERA_RUNTIME
using ChiContainer = void;
#else
// When CHIMAERA_RUNTIME is defined, ChiContainer is defined in chimod_spec.h
class ChiContainer;
#endif

/**
 * Pool Manager singleton for managing ChiPools and ChiContainers
 * 
 * Maps PoolId to ChiContainers on this node and manages the lifecycle
 * of pools in the distributed system.
 * Uses HSHM global cross pointer variable singleton pattern.
 */
class PoolManager {
 public:
  /**
   * Initialize pool manager (generic wrapper)
   * Basic initialization for pool tracking
   * @return true if initialization successful, false otherwise
   */
  bool Init() { return ClientInit(); }

  /**
   * Initialize pool manager (client mode)
   * Basic initialization for pool tracking
   * @return true if initialization successful, false otherwise
   */
  bool ClientInit();

  /**
   * Initialize pool manager (server/runtime mode)  
   * Full initialization for pool management
   * @return true if initialization successful, false otherwise
   */
  bool ServerInit();

  /**
   * Finalize and cleanup pool resources
   */
  void Finalize();

  /**
   * Register a ChiContainer with a specific PoolId
   * @param pool_id Pool identifier
   * @param container Pointer to ChiContainer
   * @return true if registration successful, false otherwise
   */
  bool RegisterContainer(PoolId pool_id, ChiContainer* container);

  /**
   * Unregister a ChiContainer
   * @param pool_id Pool identifier
   * @return true if unregistration successful, false otherwise
   */
  bool UnregisterContainer(PoolId pool_id);

  /**
   * Get ChiContainer by PoolId
   * @param pool_id Pool identifier
   * @return Pointer to ChiContainer or nullptr if not found
   */
  ChiContainer* GetContainer(PoolId pool_id) const;

  /**
   * Check if pool exists on this node
   * @param pool_id Pool identifier
   * @return true if pool exists locally, false otherwise
   */
  bool HasPool(PoolId pool_id) const;

  /**
   * Get number of registered pools
   * @return Count of registered pools on this node
   */
  size_t GetPoolCount() const;

  /**
   * Get all registered pool IDs
   * @return Vector of PoolId values for all registered pools
   */
  std::vector<PoolId> GetAllPoolIds() const;

  /**
   * Check if pool manager is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

 private:
  bool is_initialized_ = false;
  
  // Map PoolId to ChiContainers on this node
  std::unordered_map<PoolId, ChiContainer*> pool_container_map_;
};

}  // namespace chi

// Macro for accessing the Pool manager singleton using HSHM singleton
#define CHI_POOL_MANAGER hshm::Singleton<PoolManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_