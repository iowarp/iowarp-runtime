#ifndef CHI_POOL_MANAGER_H_
#define CHI_POOL_MANAGER_H_

#include <hermes_shm/util/singleton.h>
#include <unordered_map>
#include <string>
#include "../chimaera_types.h"
#include "../chimaera_module.h"

namespace chi {

#ifdef CHIMAERA_RUNTIME

struct PoolInfo {
  PoolId pool_id_;
  std::string pool_name_;
  std::string module_name_;
  std::unordered_map<ContainerId, Container*> containers_;

  PoolInfo() = default;
  PoolInfo(PoolId pool_id, const std::string& pool_name, const std::string& module_name)
      : pool_id_(pool_id), pool_name_(pool_name), module_name_(module_name) {}

  ~PoolInfo() {
    for (auto& pair : containers_) {
      delete pair.second;
    }
    containers_.clear();
  }

  PoolInfo(const PoolInfo&) = delete;
  PoolInfo& operator=(const PoolInfo&) = delete;

  PoolInfo(PoolInfo&& other) noexcept
      : pool_id_(other.pool_id_),
        pool_name_(std::move(other.pool_name_)),
        module_name_(std::move(other.module_name_)),
        containers_(std::move(other.containers_)) {
    other.pool_id_ = 0;
  }

  PoolInfo& operator=(PoolInfo&& other) noexcept {
    if (this != &other) {
      // Clean up existing containers
      for (auto& pair : containers_) {
        delete pair.second;
      }
      
      pool_id_ = other.pool_id_;
      pool_name_ = std::move(other.pool_name_);
      module_name_ = std::move(other.module_name_);
      containers_ = std::move(other.containers_);
      other.pool_id_ = 0;
    }
    return *this;
  }
};

class PoolManager {
private:
  std::unordered_map<PoolId, PoolInfo> pools_;
  mutable std::mutex pools_mutex_;

public:
  PoolManager() = default;
  ~PoolManager() = default;

  bool CreatePool(PoolId pool_id, const std::string& pool_name, const std::string& module_name);
  bool DestroyPool(PoolId pool_id);
  Container* GetContainer(PoolId pool_id, ContainerId container_id);
  Container* CreateContainer(PoolId pool_id, ContainerId container_id);
  bool DestroyContainer(PoolId pool_id, ContainerId container_id);
  bool PoolExists(PoolId pool_id) const;
  PoolInfo* GetPoolInfo(PoolId pool_id);
  const std::unordered_map<PoolId, PoolInfo>& GetAllPools() const;
  size_t GetPoolCount() const;
  size_t GetContainerCount(PoolId pool_id) const;

private:
  Container* CreateContainerInternal(PoolInfo& pool_info, ContainerId container_id);
};

#endif  // CHIMAERA_RUNTIME

}  // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::PoolManager, chiPoolManager);
#define CHI_POOL_MANAGER \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::PoolManager, chiPoolManager)
#define CHI_POOL_MANAGER_T chi::PoolManager*

#endif  // CHI_POOL_MANAGER_H_