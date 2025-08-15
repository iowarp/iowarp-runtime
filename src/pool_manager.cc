/**
 * Pool manager implementation
 */

#include "chimaera/pool_manager.h"
#include "chimaera/singletons.h"
#include <iostream>

// Global pointer variable definition for Pool manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::PoolManager, g_pool_manager);

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool PoolManager::ClientInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize pool container map
  pool_container_map_.clear();

  is_initialized_ = true;
  return true;
}

bool PoolManager::ServerInit() {
  // Pool manager needs same functionality in both client and server
  return ClientInit();
}

void PoolManager::Finalize() {
  if (!is_initialized_) {
    return;
  }

  // Clear pool container mappings
  pool_container_map_.clear();

  is_initialized_ = false;
}

bool PoolManager::RegisterContainer(PoolId pool_id, ChiContainer* container) {
  if (!is_initialized_ || container == nullptr) {
    return false;
  }

  pool_container_map_[pool_id] = container;
  return true;
}

bool PoolManager::UnregisterContainer(PoolId pool_id) {
  if (!is_initialized_) {
    return false;
  }

  auto it = pool_container_map_.find(pool_id);
  if (it != pool_container_map_.end()) {
    pool_container_map_.erase(it);
    return true;
  }
  return false;
}

ChiContainer* PoolManager::GetContainer(PoolId pool_id) const {
  if (!is_initialized_) {
    return nullptr;
  }

  auto it = pool_container_map_.find(pool_id);
  return (it != pool_container_map_.end()) ? it->second : nullptr;
}

bool PoolManager::HasPool(PoolId pool_id) const {
  if (!is_initialized_) {
    return false;
  }

  return pool_container_map_.find(pool_id) != pool_container_map_.end();
}

size_t PoolManager::GetPoolCount() const {
  return is_initialized_ ? pool_container_map_.size() : 0;
}

std::vector<PoolId> PoolManager::GetAllPoolIds() const {
  std::vector<PoolId> pool_ids;
  if (!is_initialized_) {
    return pool_ids;
  }

  pool_ids.reserve(pool_container_map_.size());
  for (const auto& pair : pool_container_map_) {
    pool_ids.push_back(pair.first);
  }
  return pool_ids;
}

bool PoolManager::IsInitialized() const {
  return is_initialized_;
}

bool PoolManager::CreateLocalPool(PoolId pool_id, const std::string& chimod_name,
                                  const std::string& pool_name, u32 num_containers) {
  if (!is_initialized_) {
    std::cerr << "PoolManager: Not initialized for pool creation" << std::endl;
    return false;
  }
  
  // Check if pool already exists
  if (HasPool(pool_id)) {
    std::cerr << "PoolManager: Pool " << pool_id << " already exists on this node" << std::endl;
    return false;
  }
  
  // Get module manager to create containers
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager) {
    std::cerr << "PoolManager: Module manager not available" << std::endl;
    return false;
  }
  
  try {
    // For now, create only one container (num_containers parameter for future use)
    auto* container = module_manager->CreateContainer(chimod_name, pool_id, pool_name);
    if (!container) {
      std::cerr << "PoolManager: Failed to create container for ChiMod: " << chimod_name << std::endl;
      return false;
    }
    
    // Initialize the container with pool information
    container->Init(pool_id, pool_name);
    
    // Register the container
    if (!RegisterContainer(pool_id, container)) {
      std::cerr << "PoolManager: Failed to register container" << std::endl;
      module_manager->DestroyContainer(chimod_name, container);
      return false;
    }
    
    std::cout << "PoolManager: Created local pool " << pool_id 
              << " with ChiMod " << chimod_name << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "PoolManager: Exception during local pool creation: " << e.what() << std::endl;
    return false;
  }
}

bool PoolManager::DestroyLocalPool(PoolId pool_id) {
  if (!is_initialized_) {
    std::cerr << "PoolManager: Not initialized for pool destruction" << std::endl;
    return false;
  }
  
  // Check if pool exists
  if (!HasPool(pool_id)) {
    std::cerr << "PoolManager: Pool " << pool_id << " not found on this node" << std::endl;
    return false;
  }
  
  // Get the container before unregistering
  auto* container = GetContainer(pool_id);
  if (!container) {
    std::cerr << "PoolManager: Container for pool " << pool_id << " is null" << std::endl;
    return false;
  }
  
  // Get module manager to destroy the container
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager) {
    std::cerr << "PoolManager: Module manager not available" << std::endl;
    return false;
  }
  
  try {
    // Unregister first
    if (!UnregisterContainer(pool_id)) {
      std::cerr << "PoolManager: Failed to unregister container for pool " << pool_id << std::endl;
      return false;
    }
    
    // TODO: Determine ChiMod name for destruction - for now assume it's stored in container
    // This would require extending ChiContainer interface to store chimod_name
    // For now, we'll skip the destruction call and rely on container cleanup
    
    std::cout << "PoolManager: Destroyed local pool " << pool_id << std::endl;
    return true;
    
  } catch (const std::exception& e) {
    std::cerr << "PoolManager: Exception during local pool destruction: " << e.what() << std::endl;
    return false;
  }
}

PoolId PoolManager::GeneratePoolId() {
  if (!is_initialized_) {
    return 0;
  }
  
  return next_pool_id_++;
}

bool PoolManager::ValidatePoolParams(const std::string& chimod_name, const std::string& pool_name) {
  if (!is_initialized_) {
    return false;
  }
  
  // Check for empty or invalid names
  if (chimod_name.empty() || pool_name.empty()) {
    std::cerr << "PoolManager: ChiMod name and pool name cannot be empty" << std::endl;
    return false;
  }
  
  // Check if the ChiMod exists
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager) {
    std::cerr << "PoolManager: Module manager not available for validation" << std::endl;
    return false;
  }
  
  auto* chimod = module_manager->GetChiMod(chimod_name);
  if (!chimod) {
    std::cerr << "PoolManager: ChiMod '" << chimod_name << "' not found" << std::endl;
    return false;
  }
  
  return true;
}

DomainTable PoolManager::CreateDomainTable(PoolId pool_id, u32 num_containers) {
  DomainTable domain_table;
  
  if (!is_initialized_) {
    return domain_table;
  }
  
  // Create local domain mappings for containers
  for (u32 i = 0; i < num_containers; ++i) {
    // Create a domain ID for local containers using kLocal subdomain
    SubDomainId local_sub_id(SubDomain::kLocal, i);
    DomainId local_domain(pool_id, local_sub_id);
    
    domain_table.AddLocalMapping(i, local_domain);
  }
  
  // Create global domain mapping for kGlobal subdomain (maps to local node for now)
  SubDomainId global_sub_id(SubDomain::kGlobal, 0);
  DomainId global_domain(pool_id, global_sub_id);
  
  std::vector<u32> physical_nodes = {0}; // Just local node for now
  domain_table.AddGlobalMapping(global_domain, physical_nodes);
  
  return domain_table;
}

bool PoolManager::CreatePool(const std::string& chimod_name, const std::string& pool_name,
                            const std::string& chimod_params, u32 num_containers, PoolId& new_pool_id) {
  // Use the new overloaded method with auto-generated ID
  bool was_created;
  return CreatePool(chimod_name, pool_name, chimod_params, num_containers, 
                   PoolId::GetNull(), new_pool_id, was_created);
}

bool PoolManager::CreatePool(const std::string& chimod_name, const std::string& pool_name,
                            const std::string& chimod_params, u32 num_containers, 
                            const PoolId& requested_pool_id, PoolId& result_pool_id, bool& was_created) {
  if (!is_initialized_) {
    std::cerr << "PoolManager: Not initialized for pool creation" << std::endl;
    return false;
  }
  
  // Validate pool parameters
  if (!ValidatePoolParams(chimod_name, pool_name)) {
    return false;
  }
  
  // Determine the target pool ID
  PoolId target_pool_id;
  if (requested_pool_id.IsNull()) {
    // Generate new pool ID
    target_pool_id = GeneratePoolId();
    if (target_pool_id.IsNull()) {
      std::cerr << "PoolManager: Failed to generate pool ID" << std::endl;
      return false;
    }
  } else {
    target_pool_id = requested_pool_id;
  }
  
  // Check if pool already exists
  if (HasPool(target_pool_id)) {
    // Pool already exists, return existing pool ID
    result_pool_id = target_pool_id;
    was_created = false;
    std::cout << "PoolManager: Pool " << target_pool_id << " already exists, returning existing pool" << std::endl;
    return true;
  }
  
  // Create domain table for the pool
  DomainTable domain_table = CreateDomainTable(target_pool_id, num_containers);
  
  // Create pool metadata
  PoolInfo pool_info(target_pool_id, pool_name, chimod_name, chimod_params, num_containers);
  pool_info.domain_table_ = domain_table;
  
  // Store pool metadata
  UpdatePoolMetadata(target_pool_id, pool_info);
  
  // Create local pool with containers
  if (!CreateLocalPool(target_pool_id, chimod_name, pool_name, num_containers)) {
    // Clean up metadata on failure
    pool_metadata_.erase(target_pool_id);
    std::cerr << "PoolManager: Failed to create local pool components" << std::endl;
    return false;
  }
  
  // Set success results
  result_pool_id = target_pool_id;
  was_created = true;
  
  std::cout << "PoolManager: Created complete pool " << target_pool_id 
            << " with " << num_containers << " containers" << std::endl;
  return true;
}

bool PoolManager::DestroyPool(PoolId pool_id) {
  if (!is_initialized_) {
    std::cerr << "PoolManager: Not initialized for pool destruction" << std::endl;
    return false;
  }
  
  // Check if pool exists in metadata
  auto metadata_it = pool_metadata_.find(pool_id);
  if (metadata_it == pool_metadata_.end()) {
    std::cerr << "PoolManager: Pool " << pool_id << " metadata not found" << std::endl;
    return false;
  }
  
  // Destroy local pool components
  if (!DestroyLocalPool(pool_id)) {
    std::cerr << "PoolManager: Failed to destroy local pool components for pool " << pool_id << std::endl;
    return false;
  }
  
  // Remove pool metadata
  pool_metadata_.erase(metadata_it);
  
  std::cout << "PoolManager: Destroyed complete pool " << pool_id << std::endl;
  return true;
}

const PoolInfo* PoolManager::GetPoolInfo(PoolId pool_id) const {
  if (!is_initialized_) {
    return nullptr;
  }
  
  auto it = pool_metadata_.find(pool_id);
  return (it != pool_metadata_.end()) ? &it->second : nullptr;
}

void PoolManager::UpdatePoolMetadata(PoolId pool_id, const PoolInfo& info) {
  if (!is_initialized_) {
    return;
  }
  
  pool_metadata_[pool_id] = info;
}

}  // namespace chi