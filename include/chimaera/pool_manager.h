#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_

#include <unordered_map>
#include <string>
#include <vector>
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
 * Domain mapping table for pool management
 * Maps local containers to global domain IDs and vice versa
 */
struct DomainTable {
  // Local domain: Maps containers on this node to global DomainId
  std::unordered_map<u32, DomainId> local_to_global_map_;
  
  // Global domain: Maps DomainId to physical DomainIds (node IDs)
  std::unordered_map<DomainId, std::vector<u32>> global_to_physical_map_;
  
  /**
   * Add local container mapping
   */
  void AddLocalMapping(u32 container_id, const DomainId& global_domain) {
    local_to_global_map_[container_id] = global_domain;
  }
  
  /**
   * Add global domain mapping
   */
  void AddGlobalMapping(const DomainId& global_domain, const std::vector<u32>& physical_nodes) {
    global_to_physical_map_[global_domain] = physical_nodes;
  }
  
  /**
   * Get global domain for local container
   */
  DomainId GetGlobalDomain(u32 container_id) const {
    auto it = local_to_global_map_.find(container_id);
    return (it != local_to_global_map_.end()) ? it->second : DomainId();
  }
  
  /**
   * Get physical nodes for global domain
   */
  std::vector<u32> GetPhysicalNodes(const DomainId& global_domain) const {
    auto it = global_to_physical_map_.find(global_domain);
    return (it != global_to_physical_map_.end()) ? it->second : std::vector<u32>();
  }
};

/**
 * Pool metadata containing domain tables and configuration
 */
struct PoolInfo {
  PoolId pool_id_;
  std::string pool_name_;
  std::string chimod_name_;
  std::string chimod_params_;
  u32 num_containers_;
  DomainTable domain_table_;
  bool is_active_;
  
  PoolInfo() : pool_id_(0), num_containers_(0), is_active_(false) {}
  
  PoolInfo(PoolId pool_id, const std::string& pool_name, 
           const std::string& chimod_name, const std::string& chimod_params,
           u32 num_containers)
      : pool_id_(pool_id), pool_name_(pool_name), chimod_name_(chimod_name),
        chimod_params_(chimod_params), num_containers_(num_containers), is_active_(true) {}
};

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
   * Generate a new unique pool ID
   * @return New pool ID
   */
  PoolId GeneratePoolId();

  /**
   * Validate pool creation parameters
   * @param chimod_name ChiMod name
   * @param pool_name Pool name  
   * @return true if parameters are valid, false otherwise
   */
  bool ValidatePoolParams(const std::string& chimod_name, const std::string& pool_name);

  /**
   * Create domain table for a pool
   * @param pool_id Pool identifier
   * @param num_containers Number of containers in the pool
   * @return Domain table for the pool
   */
  DomainTable CreateDomainTable(PoolId pool_id, u32 num_containers);

  /**
   * Create a complete pool with metadata, domain tables, and local containers
   * @param chimod_name ChiMod name for the pool
   * @param pool_name Pool name
   * @param chimod_params ChiMod parameters
   * @param num_containers Number of containers to create
   * @param[out] new_pool_id Generated pool ID
   * @return true if pool creation successful, false otherwise
   */
  bool CreatePool(const std::string& chimod_name, const std::string& pool_name,
                  const std::string& chimod_params, u32 num_containers, PoolId& new_pool_id);

  /**
   * Create or get a complete pool with specific PoolId
   * @param chimod_name ChiMod name for the pool
   * @param pool_name Pool name
   * @param chimod_params ChiMod parameters
   * @param num_containers Number of containers to create
   * @param requested_pool_id Specific pool ID to use (if GetNull(), generates new ID)
   * @param[out] result_pool_id The pool ID (existing or newly created)
   * @param[out] was_created True if pool was created, false if it already existed
   * @return true if operation successful, false otherwise
   */
  bool CreatePool(const std::string& chimod_name, const std::string& pool_name,
                  const std::string& chimod_params, u32 num_containers, 
                  const PoolId& requested_pool_id, PoolId& result_pool_id, bool& was_created);

  /**
   * Create a local pool with containers on this node (simple version)
   * @param pool_id Pool identifier
   * @param chimod_name ChiMod name for the pool
   * @param pool_name Pool name
   * @param num_containers Number of containers to create locally
   * @return true if pool creation successful, false otherwise
   */
  bool CreateLocalPool(PoolId pool_id, const std::string& chimod_name, 
                       const std::string& pool_name, u32 num_containers = 1);

  /**
   * Destroy a complete pool including metadata and local containers
   * @param pool_id Pool identifier
   * @return true if pool destruction successful, false otherwise
   */
  bool DestroyPool(PoolId pool_id);

  /**
   * Destroy a local pool and its containers on this node (simple version)
   * @param pool_id Pool identifier
   * @return true if pool destruction successful, false otherwise
   */
  bool DestroyLocalPool(PoolId pool_id);

  /**
   * Get pool information
   * @param pool_id Pool identifier
   * @return Pointer to PoolInfo or nullptr if not found
   */
  const PoolInfo* GetPoolInfo(PoolId pool_id) const;

  /**
   * Update pool metadata
   * @param pool_id Pool identifier
   * @param info Pool information to store
   */
  void UpdatePoolMetadata(PoolId pool_id, const PoolInfo& info);

  /**
   * Check if pool manager is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

 private:
  bool is_initialized_ = false;
  
  // Map PoolId to ChiContainers on this node
  std::unordered_map<PoolId, ChiContainer*> pool_container_map_;
  
  // Map PoolId to pool metadata
  std::unordered_map<PoolId, PoolInfo> pool_metadata_;
  
  // Pool ID counter for generating unique IDs
  PoolId next_pool_id_ = 2; // Start at 2, since 1 is reserved for admin
};

}  // namespace chi

// Global pointer variable declaration for Pool manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_H(chi::PoolManager, g_pool_manager);

// Macro for accessing the Pool manager singleton using global pointer variable
#define CHI_POOL_MANAGER HSHM_GET_GLOBAL_PTR_VAR(::chi::PoolManager, g_pool_manager)

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_POOL_MANAGER_H_