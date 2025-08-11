/**
 * Pool manager implementation
 */

#include "chimaera/pool_manager.h"

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

}  // namespace chi