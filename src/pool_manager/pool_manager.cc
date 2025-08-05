#include "chimaera/pool_manager/pool_manager.h"

#ifdef CHIMAERA_RUNTIME

#include "chimaera/module_manager/module_manager.h"
#include <mutex>

namespace chi {

bool PoolManager::CreatePool(PoolId pool_id, const std::string& pool_name, const std::string& module_name) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  if (pools_.find(pool_id) != pools_.end()) {
    HELOG(kWarning, "Pool {} already exists", pool_id);
    return false;
  }

  pools_.emplace(pool_id, PoolInfo(pool_id, pool_name, module_name));
  HILOG(kInfo, "Created pool {} with name '{}' using module '{}'", pool_id, pool_name, module_name);
  return true;
}

bool PoolManager::DestroyPool(PoolId pool_id) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto it = pools_.find(pool_id);
  if (it == pools_.end()) {
    HELOG(kWarning, "Pool {} does not exist", pool_id);
    return false;
  }

  pools_.erase(it);
  HILOG(kInfo, "Destroyed pool {}", pool_id);
  return true;
}

Container* PoolManager::GetContainer(PoolId pool_id, ContainerId container_id) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto pool_it = pools_.find(pool_id);
  if (pool_it == pools_.end()) {
    return nullptr;
  }

  auto& containers = pool_it->second.containers_;
  auto container_it = containers.find(container_id);
  return (container_it != containers.end()) ? container_it->second : nullptr;
}

Container* PoolManager::CreateContainer(PoolId pool_id, ContainerId container_id) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto pool_it = pools_.find(pool_id);
  if (pool_it == pools_.end()) {
    HELOG(kError, "Cannot create container {}: pool {} does not exist", container_id, pool_id);
    return nullptr;
  }

  auto& pool_info = pool_it->second;
  auto& containers = pool_info.containers_;
  
  if (containers.find(container_id) != containers.end()) {
    HELOG(kWarning, "Container {} already exists in pool {}", container_id, pool_id);
    return containers[container_id];
  }

  Container* container = CreateContainerInternal(pool_info, container_id);
  if (container) {
    containers[container_id] = container;
    HILOG(kInfo, "Created container {} in pool {}", container_id, pool_id);
  }
  
  return container;
}

bool PoolManager::DestroyContainer(PoolId pool_id, ContainerId container_id) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto pool_it = pools_.find(pool_id);
  if (pool_it == pools_.end()) {
    HELOG(kWarning, "Pool {} does not exist", pool_id);
    return false;
  }

  auto& containers = pool_it->second.containers_;
  auto container_it = containers.find(container_id);
  if (container_it == containers.end()) {
    HELOG(kWarning, "Container {} does not exist in pool {}", container_id, pool_id);
    return false;
  }

  delete container_it->second;
  containers.erase(container_it);
  HILOG(kInfo, "Destroyed container {} in pool {}", container_id, pool_id);
  return true;
}

bool PoolManager::PoolExists(PoolId pool_id) const {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  return pools_.find(pool_id) != pools_.end();
}

PoolInfo* PoolManager::GetPoolInfo(PoolId pool_id) {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto it = pools_.find(pool_id);
  return (it != pools_.end()) ? &it->second : nullptr;
}

const std::unordered_map<PoolId, PoolInfo>& PoolManager::GetAllPools() const {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  return pools_;
}

size_t PoolManager::GetPoolCount() const {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  return pools_.size();
}

size_t PoolManager::GetContainerCount(PoolId pool_id) const {
  std::lock_guard<std::mutex> lock(pools_mutex_);
  
  auto it = pools_.find(pool_id);
  return (it != pools_.end()) ? it->second.containers_.size() : 0;
}

Container* PoolManager::CreateContainerInternal(PoolInfo& pool_info, ContainerId container_id) {
  auto module_manager = CHI_MODULE_MANAGER;
  Container* container = module_manager->CreateModuleInstance(
      pool_info.module_name_, pool_info.pool_id_, pool_info.pool_name_);
  
  if (container) {
    container->container_id_ = container_id;
  }
  
  return container;
}

}  // namespace chi

#endif  // CHIMAERA_RUNTIME

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::PoolManager, chiPoolManager);