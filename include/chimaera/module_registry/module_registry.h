/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of chi. The full chi copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef CHI_INCLUDE_CHI_TASK_TASK_REGISTRY_H_
#define CHI_INCLUDE_CHI_TASK_TASK_REGISTRY_H_

#ifdef CHIMAERA_RUNTIME

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

#include "chimaera/config/config_server.h"
#include "module.h"
// #include "chimaera_admin/chimaera_admin_client.h"
#include "chimaera/network/rpc.h"

namespace stdfs = std::filesystem;

namespace chi {

/** Forward declaration of CreatePoolTask */
namespace Admin {
class CreateTaskParams;
template <typename TaskParamsT> class CreatePoolBaseTask;
} // namespace Admin

/** All information needed to create a trait */
struct ModuleInfo {
  hshm::SharedLibrary lib_;          /**< The dlfcn library */
  alloc_state_t alloc_state_;        /**< The static create function */
  new_state_t new_state_;            /**< The non-static create function */
  get_module_name_t get_module_name; /**< The get task name function */
  Container *static_state_;          /**< An allocation for static functions */
  ibitfield flags_;
  CLS_CONST int kPlugged = BIT_OPT(chi::IntFlag, 0);

  void SetPlugged() { flags_.SetBits(kPlugged); }

  void UnsetPlugged() { flags_.UnsetBits(kPlugged); }

  bool IsPlugged() { return flags_.All(kPlugged); }

  /** Default constructor */
  ModuleInfo() = default;

  /** Move constructor */
  ModuleInfo(ModuleInfo &&other) noexcept {
    lib_ = std::move(other.lib_);
    alloc_state_ = other.alloc_state_;
    new_state_ = other.new_state_;
    get_module_name = other.get_module_name;
    static_state_ = other.static_state_;
  }

  /** Move assignment operator */
  ModuleInfo &operator=(ModuleInfo &&other) noexcept {
    if (this != &other) {
      lib_ = std::move(other.lib_);
      alloc_state_ = other.alloc_state_;
      new_state_ = other.new_state_;
      get_module_name = other.get_module_name;
      static_state_ = other.static_state_;
    }
    return *this;
  }
};

struct PoolInfo {
  ModuleInfo *module_;
  std::string lib_name_;
  std::unordered_map<ContainerId, Container *> containers_;
};

/**
 * Stores the registered set of Modules and Containers
 * */
class ModuleRegistry {
public:
  /** The node the registry is on */
  NodeId node_id_;
  /** The dirs to search for modules */
  std::vector<std::string> lib_dirs_;
  /** Map of a semantic lib name to lib info */
  std::unordered_map<std::string, ModuleInfo> libs_;
  /** Map of a semantic exec name to exec id */
  std::unordered_map<std::string, PoolId> pool_ids_;
  /** Map of a semantic exec id to state */
  std::unordered_map<PoolId, PoolInfo> pools_;
  /** A unique identifier counter */
  hipc::atomic<hshm::min_u64> *unique_;
  Mutex lock_;
  CoRwLock upgrade_lock_;

public:
  /** Default constructor */
  ModuleRegistry() { lock_.Init(); }

  /** Initialize the Task Registry */
  void ServerInit(ServerConfig *config, NodeId node_id,
                  hipc::atomic<hshm::min_u64> &unique) {
    node_id_ = node_id;
    unique_ = &unique;

    // Load the LD_LIBRARY_PATH variable
    auto ld_lib_path_env = getenv("LD_LIBRARY_PATH");
    std::string ld_lib_path;
    if (ld_lib_path_env) {
      ld_lib_path = ld_lib_path_env;
    }

    // Load the CHI_TASK_PATH variable
    std::string chi_lib_path;
    auto chi_lib_path_env = getenv("CHI_TASK_PATH");
    if (chi_lib_path_env) {
      chi_lib_path = chi_lib_path_env;
    }

    // Combine LD_LIBRARY_PATH and CHI_TASK_PATH
    std::string paths = chi_lib_path + ":" + ld_lib_path;
    std::stringstream ss(paths);
    std::string lib_dir;
    while (std::getline(ss, lib_dir, ':')) {
      lib_dirs_.emplace_back(lib_dir);
    }

    // Find each lib in LD_LIBRARY_PATH
    for (const std::string &lib_name : config->modules_) {
      if (!RegisterModule(lib_name)) {
        HELOG(kWarning, "Failed to load the lib: {}", lib_name);
      }
    }
  }

  /** Check if any path matches */
  std::string FindExistingPath(const std::vector<std::string> lib_paths) {
    for (const std::string &lib_path : lib_paths) {
      if (stdfs::exists(lib_path)) {
        return lib_path;
      }
    }
    return "";
  }

  /**
    Check if any path matches. It checks for GPU variants first, since the
    runtime supports GPU if enabled. */
  std::vector<std::string> FindMatchingPathInDirs(const std::string &lib_name) {
    std::vector<std::string> variants = {"_gpu", "_host", ""};
    std::vector<std::string> prefixes = {"", "lib", "libchimaera_"};
    std::vector<std::string> suffixes = {"", "_runtime"};
    std::vector<std::string> extensions = {".so", ".dll"};
    std::vector<std::string> concrete_libs;
    for (const std::string &lib_dir : lib_dirs_) {
      // Determine if this directory contains the library
      std::vector<std::string> potential_paths;
      for (const std::string &variant : variants) {
        for (const std::string &prefix : prefixes) {
          for (const std::string &suffix : suffixes) {
            for (const std::string &extension : extensions) {
              potential_paths.emplace_back(hshm::Formatter::format(
                  "{}/{}{}{}{}{}", lib_dir, prefix, lib_name, variant, suffix,
                  extension));
            }
          }
        }
      }
      std::string lib_path = FindExistingPath(potential_paths);
      if (lib_path.empty()) {
        continue;
      };
      concrete_libs.emplace_back(lib_path);
    }
    return concrete_libs;
  }

  /** Load a module at path */
  bool LoadModuleAtPath(const std::string &lib_name,
                        const std::string &lib_path, ModuleInfo &info) {
    // Load the library
    info.lib_.Load(lib_path);
    if (info.lib_.IsNull()) {
      HELOG(kError, "Could not open the lib library: {}. Reason: {}", lib_path,
            info.lib_.GetError());
      return false;
    }

    // Get the allocate state function
    info.alloc_state_ = (alloc_state_t)info.lib_.GetSymbol("alloc_state");
    if (!info.alloc_state_) {
      HELOG(kError, "The lib {} does not have alloc_state symbol", lib_path);
      return false;
    }

    // Get the new state function
    info.new_state_ = (new_state_t)info.lib_.GetSymbol("new_state");
    if (!info.new_state_) {
      HELOG(kError, "The lib {} does not have new_state symbol", lib_path);
      return false;
    }

    // Get the module name function
    info.get_module_name =
        (get_module_name_t)info.lib_.GetSymbol("get_module_name");
    if (!info.get_module_name) {
      HELOG(kError, "The lib {} does not have get_module_name symbol",
            lib_path);
      return false;
    }

    // Check if the lib is already loaded
    std::string module_name = info.get_module_name();
    HILOG(kInfo, "(node {}) Finished loading the lib: {}", CHI_RPC->node_id_,
          module_name);
    info.static_state_ = info.alloc_state_();
    return true;
  }

  /** Load a module */
  bool LoadModule(const std::string &lib_name, ModuleInfo &info) {
    std::vector<std::string> lib_path = FindMatchingPathInDirs(lib_name);
    if (lib_path.empty()) {
      HELOG(kError, "Could not find the lib: {}", lib_name);
      return false;
    }
    for (const std::string &path : lib_path) {
      if (LoadModuleAtPath(lib_name, path, info)) {
        return true;
      }
    }
    return false;
  }

  /** Load a module */
  bool RegisterModule(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    if (libs_.find(lib_name) != libs_.end()) {
      return true;
    }
    ModuleInfo info;
    if (!LoadModule(lib_name, info)) {
      return false;
    }
    libs_.emplace(lib_name, std::move(info));
    return true;
  }

  /** Replace a module */
  void ReplaceModule(ModuleInfo &info) {
    ScopedMutex lock(lock_, 0);
    std::string module_name = info.get_module_name();
    auto it = libs_.find(module_name);
    if (it == libs_.end()) {
      HELOG(kError, "Could not find the module: {}", module_name);
      return;
    }
    it->second = std::move(info);
  }

  /** Destroy a module */
  void DestroyModule(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    auto it = libs_.find(lib_name);
    if (it == libs_.end()) {
      HELOG(kError, "Could not find the module: {}", lib_name);
      return;
    }
    libs_.erase(it);
  }

  /** Allocate a pool ID */
  HSHM_INLINE
  PoolId CreatePoolId() { return PoolId(node_id_, unique_->fetch_add(1)); }

  /**
   * Create a pool
   * pool_id must not be NULL.
   * */
  bool CreatePool(const char *lib_name, const char *pool_name,
                  const PoolId &pool_id,
                  Admin::CreatePoolBaseTask<Admin::CreateTaskParams> *task,
                  const std::vector<SubDomainId> &containers);

  /** Replace a container */
  void ReplaceContainer(Container *new_container) {
    ScopedMutex lock(lock_, 0);
    PoolId pool_id = new_container->pool_id_;
    auto it = pools_.find(pool_id);
    if (it == pools_.end()) {
      HELOG(kError, "Could not find the pool: {}", pool_id);
      return;
    }
    PoolInfo &pool = it->second;
    ContainerId container_id = new_container->container_id_;
    Container *old = pool.containers_[container_id];
    pool.containers_[container_id] = new_container;
    delete old;
  }

  /** Get or create a pool's ID */
  PoolId GetOrCreatePoolId(const std::string &pool_name) {
    ScopedMutex lock(lock_, 0);
    auto it = pool_ids_.find(pool_name);
    if (it == pool_ids_.end()) {
      PoolId pool_id = CreatePoolId();
      pool_ids_.emplace(pool_name, pool_id);
      return pool_id;
    }
    return it->second;
  }

  /** Get a pool's ID */
  PoolId GetPoolId(const std::string &pool_name) {
    ScopedMutex lock(lock_, 0);
    auto it = pool_ids_.find(pool_name);
    if (it == pool_ids_.end()) {
      return PoolId::GetNull();
    }
    return it->second;
  }

  /** Get the static state instance */
  Container *GetStaticContainer(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    auto it = libs_.find(lib_name);
    if (it == libs_.end()) {
      return nullptr;
    }
    ModuleInfo &info = it->second;
    if (info.IsPlugged()) {
      return nullptr;
    }
    return info.static_state_;
  }

  /** Get the static state instance */
  Container *GetStaticContainer(const PoolId &pool_id) {
    ScopedMutex lock(lock_, 0);
    auto pool_it = pools_.find(pool_id);
    if (pool_it == pools_.end()) {
      return nullptr;
    }
    PoolInfo &pool = pool_it->second;
    if (pool.module_->IsPlugged()) {
      return nullptr;
    }
    auto it = libs_.find(pool.lib_name_);
    if (it == libs_.end()) {
      return nullptr;
    }
    return it->second.static_state_;
  }

  /** Get pool instance by name OR by ID */
  PoolId PoolExists(const std::string &pool_name, const PoolId &pool_id) {
    PoolId id = GetPoolId(pool_name);
    ScopedMutex lock(lock_, 0);
    if (id.IsNull()) {
      id = pool_id;
    }
    auto it = pools_.find(id);
    if (it == pools_.end()) {
      return PoolId::GetNull();
    }
    return id;
  }

  /** Get module name from pool */
  std::string GetModuleName(const PoolId &pool_id) {
    ScopedMutex lock(lock_, 0);
    auto it = pools_.find(pool_id);
    if (it == pools_.end()) {
      return "";
    }
    return it->second.lib_name_;
  }

  /** Get a pool instance */
  Container *GetContainer(const PoolId &pool_id,
                          const ContainerId &container_id) {
    // ScopedMutex lock(lock_, 0);
    auto pool_it = pools_.find(pool_id);
    if (pool_it == pools_.end()) {
      // HELOG(kFatal, "Could not find pool {}", pool_id);
      return nullptr;
    }
    PoolInfo &pool = pool_it->second;
    if (pool.module_->IsPlugged()) {
      return nullptr;
    }
    Container *exec = pool.containers_[container_id];
    if (!exec) {
      //      CHI_RPC->PrintDomain(DomainId{pool_id,
      //      SubDomain::kContainerSet}); for (auto &kv :
      //      pool_it->second.containers_) {
      //        HILOG(kInfo, "Container ID: {} {}", kv.first, kv.second)
      //      }
      HELOG(kError, "Could not find container {} in pool {}", container_id,
            pool_id);
    }
    return exec;
  }

  /** Destroy a pool */
  void DestroyContainer(const PoolId &pool_id) {
    ScopedMutex lock(lock_, 0);
    auto it = pools_.find(pool_id);
    if (it == pools_.end()) {
      HELOG(kWarning, "Could not find the pool");
      return;
    }
    PoolInfo &pool = it->second;
    std::string pool_name = pool.containers_[0]->name_;
    // TODO(llogan): Iterate over shared_state + states and destroy them
    pool_ids_.erase(pool_name);
    pools_.erase(it);
  }

  /** Get all ChiContainers matching the module name */
  std::vector<Container *> GetContainers(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    std::vector<Container *> containers;
    for (auto &kv : pools_) {
      PoolInfo &pool = kv.second;
      if (pool.lib_name_ == lib_name) {
        for (auto &kv2 : pool.containers_) {
          containers.emplace_back(kv2.second);
        }
      }
    }
    return containers;
  }

  /** Plug Module */
  void PlugModule(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    auto it = libs_.find(lib_name);
    if (it == libs_.end()) {
      HELOG(kError, "Could not find the module: {}", lib_name);
      return;
    }
    ModuleInfo &info = it->second;
    info.SetPlugged();
  }

  /** Unplug Module */
  void UnplugModule(const std::string &lib_name) {
    ScopedMutex lock(lock_, 0);
    auto it = libs_.find(lib_name);
    if (it == libs_.end()) {
      HELOG(kError, "Could not find the module: {}", lib_name);
      return;
    }
    ModuleInfo &info = it->second;
    info.UnsetPlugged();
  }
};

/** Singleton macro for task registry */
#define CHI_MOD_REGISTRY hshm::Singleton<chi::ModuleRegistry>::GetInstance()
} // namespace chi

#endif // CHIMAERA_RUNTIME

#endif // CHI_INCLUDE_CHI_TASK_TASK_REGISTRY_H_
