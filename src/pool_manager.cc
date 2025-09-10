/**
 * Pool manager implementation
 */

#include "chimaera/pool_manager.h"
#include "chimaera/container.h"
#include "chimaera/singletons.h"
#include "chimaera/task.h"
#include "chimaera/admin/admin_tasks.h"
#include <iostream>

// Global pointer variable definition for Pool manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::PoolManager, g_pool_manager);

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool PoolManager::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  // Initialize pool container map and metadata
  pool_container_map_.clear();
  pool_metadata_.clear();

  is_initialized_ = true;

  // Create the admin chimod pool (kAdminPoolId = 1)
  // This is required for flush operations and other admin tasks
  u32 num_nodes = 1; // TODO: Get actual node count from configuration
  PoolId admin_pool_id;
  bool was_created;
  if (!CreatePool("chimaera_admin", "admin", "", num_nodes, kAdminPoolId, admin_pool_id, was_created, FullPtr<Task>(), nullptr)) {
    std::cerr << "PoolManager: Failed to create admin chimod pool during ServerInit" << std::endl;
    return false;
  }

  HILOG(kInfo, "PoolManager: Admin chimod pool created successfully with PoolId {}", admin_pool_id);
  return true;
}

void PoolManager::Finalize() {
  if (!is_initialized_) {
    return;
  }

  // Clear pool container mappings
  pool_container_map_.clear();

  is_initialized_ = false;
}

bool PoolManager::RegisterContainer(PoolId pool_id, Container* container) {
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

Container* PoolManager::GetContainer(PoolId pool_id) const {
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
                                  const std::string& pool_name, u32 num_containers,
                                  FullPtr<Task> task, RunContext* run_ctx) {
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
    container->Init(pool_id);
    
    // Run create method on container
    // Create proper task and run context if not provided
    FullPtr<Task> actual_task;
    RunContext* actual_run_ctx;
    bool allocated_task = false;
    bool allocated_run_ctx = false;
    
    if (!task.IsNull()) {
      actual_task = task;
    } else {
      // Allocate Admin CreateTask using NewTask
      auto* ipc_manager = CHI_IPC;
      if (ipc_manager) {
        auto create_task = ipc_manager->NewTask<chimaera::admin::CreateTask>(
          CreateTaskNode(), 
          kAdminPoolId,  // Use admin pool for creation
          PoolQuery::Local(),
          chimod_name,
          pool_name
        );
        actual_task = create_task.Cast<Task>();
        allocated_task = true;
      }
    }
    
    if (run_ctx != nullptr) {
      actual_run_ctx = run_ctx;
    } else {
      // Allocate RunContext with new
      actual_run_ctx = new RunContext();
      allocated_run_ctx = true;
    }
    
    if (!actual_task.IsNull() && actual_run_ctx) {
      // Call container->Run with Method::kCreate (which is 0)
      try {
        container->Run(0, actual_task, *actual_run_ctx); // Method::kCreate = 0
        
        // Check if Create method succeeded by examining the result code
        // Cast to BaseCreateTask to access result_code_ field
        auto* create_task = reinterpret_cast<chimaera::admin::BaseCreateTask<chimaera::admin::CreateParams>*>(actual_task.ptr_);
        if (create_task && create_task->result_code_ != 0) {
          std::cerr << "PoolManager: Create method failed with result code: " << create_task->result_code_ << std::endl;
          // Cleanup allocated resources
          if (allocated_task && CHI_IPC) CHI_IPC->DelTask(actual_task);
          if (allocated_run_ctx) delete actual_run_ctx;
          // Cleanup container since creation failed
          module_manager->DestroyContainer(chimod_name, container);
          return false;
        }
        
        HILOG(kInfo, "PoolManager: Executed Create method on container for pool {}", pool_id);
      } catch (const std::exception& e) {
        std::cerr << "PoolManager: Exception during Create method on container: " << e.what() << std::endl;
        // Cleanup allocated resources
        if (allocated_task && CHI_IPC) CHI_IPC->DelTask(actual_task);
        if (allocated_run_ctx) delete actual_run_ctx;
        // Cleanup container since creation failed
        module_manager->DestroyContainer(chimod_name, container);
        return false;
      }
    }
    
    // Cleanup allocated resources
    if (allocated_task && CHI_IPC) CHI_IPC->DelTask(actual_task);
    if (allocated_run_ctx) delete actual_run_ctx;
    
    // Register the container
    if (!RegisterContainer(pool_id, container)) {
      std::cerr << "PoolManager: Failed to register container" << std::endl;
      module_manager->DestroyContainer(chimod_name, container);
      return false;
    }
    
    // Initialize client after registration (when system is more stable)
    try {
      container->InitClient(pool_id);
    } catch (const std::exception& e) {
      std::cerr << "PoolManager: Warning - failed to initialize client: " << e.what() << std::endl;
      // Continue anyway, client initialization is not critical for basic container functionality
    }
    
    HILOG(kInfo, "PoolManager: Created local pool {} with ChiMod {}", pool_id, chimod_name);
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
    
    HILOG(kInfo, "PoolManager: Destroyed local pool {}", pool_id);
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

AddressTable PoolManager::CreateAddressTable(PoolId pool_id, u32 num_containers) {
  AddressTable address_table;
  
  if (!is_initialized_) {
    return address_table;
  }
  
  // Create one address per container in the global table
  for (u32 container_idx = 0; container_idx < num_containers; ++container_idx) {
    Address global_address(pool_id, Group::kGlobal, container_idx);
    Address physical_address(pool_id, Group::kPhysical, container_idx);
    
    // Map each global address to its corresponding physical address
    address_table.AddGlobalToPhysicalMapping(global_address, physical_address);
  }
  
  // Create exactly one local address that maps to the global address of the container on this node
  // Assuming this node gets container 0 (this could be made configurable)
  Address local_address(pool_id, Group::kLocal, 0); // One local address for this node
  Address global_address(pool_id, Group::kGlobal, 0); // Maps to container 0 globally
  
  // Map the single local address to its global counterpart
  address_table.AddLocalToGlobalMapping(local_address, global_address);
  
  return address_table;
}

bool PoolManager::CreatePool(const std::string& chimod_name, const std::string& pool_name,
                            const std::string& chimod_params, u32 num_containers, PoolId& new_pool_id,
                            FullPtr<Task> task, RunContext* run_ctx) {
  // Use the new overloaded method with auto-generated ID
  bool was_created;
  return CreatePool(chimod_name, pool_name, chimod_params, num_containers, 
                   PoolId::GetNull(), new_pool_id, was_created, task, run_ctx);
}

bool PoolManager::CreatePool(const std::string& chimod_name, const std::string& pool_name,
                            const std::string& chimod_params, u32 num_containers, 
                            const PoolId& requested_pool_id, PoolId& result_pool_id, bool& was_created,
                            FullPtr<Task> task, RunContext* run_ctx) {
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
    HILOG(kInfo, "PoolManager: Pool {} already exists, returning existing pool", target_pool_id);
    return true;
  }
  
  // Create address table for the pool
  AddressTable address_table = CreateAddressTable(target_pool_id, num_containers);
  
  // Create pool metadata
  PoolInfo pool_info(target_pool_id, pool_name, chimod_name, chimod_params, num_containers);
  pool_info.address_table_ = address_table;
  
  // Store pool metadata
  UpdatePoolMetadata(target_pool_id, pool_info);
  
  // Create local pool with containers
  if (!CreateLocalPool(target_pool_id, chimod_name, pool_name, num_containers, task, run_ctx)) {
    // Clean up metadata on failure
    pool_metadata_.erase(target_pool_id);
    std::cerr << "PoolManager: Failed to create local pool components" << std::endl;
    return false;
  }
  
  // Set success results
  result_pool_id = target_pool_id;
  was_created = true;
  
  HILOG(kInfo, "PoolManager: Created complete pool {} with {} containers", target_pool_id, num_containers);
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
  
  HILOG(kInfo, "PoolManager: Destroyed complete pool {}", pool_id);
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

u32 PoolManager::GetContainerNodeId(PoolId pool_id, ContainerId container_id) const {
  if (!is_initialized_) {
    return 0;  // Default to local node
  }
  
  // Get pool metadata
  const PoolInfo* pool_info = GetPoolInfo(pool_id);
  if (!pool_info) {
    return 0;  // Pool not found, assume local
  }
  
  // Create global address for the container
  Address global_address(pool_id, Group::kGlobal, container_id);
  
  // Look up physical address from the address table
  Address physical_address;
  if (pool_info->address_table_.GlobalToPhysical(global_address, physical_address)) {
    // Return the minor_id which represents the node ID
    return physical_address.minor_id_;
  }
  
  // Default to local node if mapping not found
  return 0;
}


}  // namespace chi