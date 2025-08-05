#ifndef CHI_MODULE_MANAGER_H_
#define CHI_MODULE_MANAGER_H_

#include "../chimaera_module.h"
#include "../chimaera_types.h"
#include "../config/config_manager.h"
#include <hermes_shm/data_structures/all.h>
#include <hermes_shm/introspect/system_info.h>
#include <hermes_shm/util/config_parse.h>
#include <hermes_shm/util/singleton.h>

namespace chi {

#ifdef CHIMAERA_RUNTIME

struct ModuleInfo {
  hshm::SharedLibrary lib_;
  alloc_state_t alloc_state_;
  new_state_t new_state_;
  get_module_name_t get_module_name_;
  Container *static_state_;
  std::string module_name_;
  std::string lib_path_;

  ModuleInfo()
      : alloc_state_(nullptr), new_state_(nullptr), get_module_name_(nullptr),
        static_state_(nullptr) {}

  ~ModuleInfo() {
    if (static_state_) {
      delete static_state_;
      static_state_ = nullptr;
    }
  }

  ModuleInfo(const ModuleInfo &) = delete;
  ModuleInfo &operator=(const ModuleInfo &) = delete;

  ModuleInfo(ModuleInfo &&other) noexcept
      : lib_(std::move(other.lib_)), alloc_state_(other.alloc_state_),
        new_state_(other.new_state_), get_module_name_(other.get_module_name_),
        static_state_(other.static_state_),
        module_name_(std::move(other.module_name_)),
        lib_path_(std::move(other.lib_path_)) {
    other.alloc_state_ = nullptr;
    other.new_state_ = nullptr;
    other.get_module_name_ = nullptr;
    other.static_state_ = nullptr;
  }

  ModuleInfo &operator=(ModuleInfo &&other) noexcept {
    if (this != &other) {
      if (static_state_) {
        delete static_state_;
      }

      lib_ = std::move(other.lib_);
      alloc_state_ = other.alloc_state_;
      new_state_ = other.new_state_;
      get_module_name_ = other.get_module_name_;
      static_state_ = other.static_state_;
      module_name_ = std::move(other.module_name_);
      lib_path_ = std::move(other.lib_path_);

      other.alloc_state_ = nullptr;
      other.new_state_ = nullptr;
      other.get_module_name_ = nullptr;
      other.static_state_ = nullptr;
    }
    return *this;
  }
};

class ModuleManager {
private:
  std::unordered_map<std::string, ModuleInfo> modules_;
  std::vector<std::string> lib_dirs_;

  void InitializeLibDirs();
  std::string FindExistingPath(const std::vector<std::string> &potential_paths);

public:
  ModuleManager();
  ~ModuleManager() = default;

  std::vector<std::string> FindMatchingPathInDirs(const std::string &lib_name);
  bool LoadModuleAtPath(const std::string &lib_name,
                        const std::string &lib_path, ModuleInfo &info);
  bool LoadModule(const std::string &lib_name);
  bool LoadDefaultModules();
  ModuleInfo *GetModuleInfo(const std::string &module_name);
  Container *CreateModuleInstance(const std::string &module_name,
                                  const PoolId &pool_id,
                                  const std::string &pool_name);
  const std::unordered_map<std::string, ModuleInfo> &GetLoadedModules() const;
  void UnloadModule(const std::string &lib_name);
  void UnloadAllModules();
};

#endif // CHIMAERA_RUNTIME

} // namespace chi

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(chi::ModuleManager, chiModuleManager);
#define CHI_MODULE_MANAGER                                                     \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(chi::ModuleManager, chiModuleManager)
#define CHI_MODULE_MANAGER_T chi::ModuleManager *

#endif // CHI_MODULE_MANAGER_H_