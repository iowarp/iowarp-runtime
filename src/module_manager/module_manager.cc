#include "chimaera/module_manager/module_manager.h"

#ifdef CHIMAERA_RUNTIME

#include <fstream>

namespace chi {

ModuleManager::ModuleManager() {
  InitializeLibDirs();
}

void ModuleManager::InitializeLibDirs() {
  lib_dirs_.push_back("./build/tasks");
  lib_dirs_.push_back("./tasks");
  lib_dirs_.push_back("/usr/lib/chimaera");
  lib_dirs_.push_back("/usr/local/lib/chimaera");
  
  const char* chimaera_lib_path = std::getenv("CHIMAERA_LIB_PATH");
  if (chimaera_lib_path) {
    std::string paths(chimaera_lib_path);
    size_t start = 0;
    size_t end = paths.find(':');
    
    while (end != std::string::npos) {
      lib_dirs_.push_back(paths.substr(start, end - start));
      start = end + 1;
      end = paths.find(':', start);
    }
    lib_dirs_.push_back(paths.substr(start));
  }
}

std::string ModuleManager::FindExistingPath(const std::vector<std::string>& potential_paths) {
  for (const std::string& path : potential_paths) {
    std::ifstream file(path);
    if (file.good()) {
      return path;
    }
  }
  return "";
}

std::vector<std::string> ModuleManager::FindMatchingPathInDirs(const std::string& lib_name) {
  std::vector<std::string> variants = {"_gpu", "_host", ""};
  std::vector<std::string> prefixes = {"", "lib", "libchimaera_"};
  std::vector<std::string> suffixes = {"", "_runtime"};
  std::vector<std::string> extensions = {".so", ".dll"};
  std::vector<std::string> concrete_libs;

  for (const std::string& lib_dir : lib_dirs_) {
    std::vector<std::string> potential_paths;
    for (const std::string& variant : variants) {
      for (const std::string& prefix : prefixes) {
        for (const std::string& suffix : suffixes) {
          for (const std::string& extension : extensions) {
            potential_paths.emplace_back(hshm::Formatter::format(
                "{}/{}{}{}{}{}", lib_dir, prefix, lib_name, variant, suffix,
                extension));
          }
        }
      }
    }
    
    std::string lib_path = FindExistingPath(potential_paths);
    if (!lib_path.empty()) {
      concrete_libs.emplace_back(lib_path);
    }
  }
  return concrete_libs;
}

bool ModuleManager::LoadModuleAtPath(const std::string& lib_name,
                      const std::string& lib_path, ModuleInfo& info) {
  info.lib_.Load(lib_path);
  if (info.lib_.IsNull()) {
    HELOG(kError, "Could not open the lib library: {}. Reason: {}", 
          lib_path, info.lib_.GetError());
    return false;
  }

  info.alloc_state_ = (alloc_state_t)info.lib_.GetSymbol("alloc_state");
  if (!info.alloc_state_) {
    HELOG(kError, "The lib {} does not have alloc_state symbol", lib_path);
    return false;
  }

  info.new_state_ = (new_state_t)info.lib_.GetSymbol("new_state");
  if (!info.new_state_) {
    HELOG(kError, "The lib {} does not have new_state symbol", lib_path);
    return false;
  }

  info.get_module_name_ = (get_module_name_t)info.lib_.GetSymbol("get_module_name");
  if (!info.get_module_name_) {
    HELOG(kError, "The lib {} does not have get_module_name symbol", lib_path);
    return false;
  }

  info.module_name_ = info.get_module_name_();
  info.lib_path_ = lib_path;
  info.static_state_ = info.alloc_state_();
  
  HILOG(kInfo, "Finished loading the lib: {}", info.module_name_);
  return true;
}

bool ModuleManager::LoadModule(const std::string& lib_name) {
  if (modules_.find(lib_name) != modules_.end()) {
    HILOG(kInfo, "Module {} already loaded", lib_name);
    return true;
  }

  std::vector<std::string> lib_paths = FindMatchingPathInDirs(lib_name);
  if (lib_paths.empty()) {
    HELOG(kError, "Could not find the lib: {}", lib_name);
    return false;
  }

  ModuleInfo info;
  for (const std::string& path : lib_paths) {
    if (LoadModuleAtPath(lib_name, path, info)) {
      modules_[lib_name] = std::move(info);
      return true;
    }
  }
  return false;
}

bool ModuleManager::LoadDefaultModules() {
  std::vector<std::string> default_modules = {"chimaera_admin"};
  auto registry = CHI_CONFIG_MANAGER->GetModuleRegistry();
  
  for (const std::string& module : default_modules) {
    if (!LoadModule(module)) {
      HELOG(kError, "Failed to load default module: {}", module);
      return false;
    }
  }

  for (const std::string& module : registry) {
    if (!LoadModule(module)) {
      HELOG(kWarning, "Failed to load configured module: {}", module);
    }
  }

  return true;
}

ModuleInfo* ModuleManager::GetModuleInfo(const std::string& module_name) {
  auto it = modules_.find(module_name);
  if (it != modules_.end()) {
    return &it->second;
  }
  return nullptr;
}

Container* ModuleManager::CreateModuleInstance(const std::string& module_name,
                                const PoolId& pool_id,
                                const std::string& pool_name) {
  auto it = modules_.find(module_name);
  if (it == modules_.end()) {
    HELOG(kError, "Module {} not found", module_name);
    return nullptr;
  }

  return it->second.new_state_(&pool_id, pool_name.c_str());
}

const std::unordered_map<std::string, ModuleInfo>& ModuleManager::GetLoadedModules() const {
  return modules_;
}

void ModuleManager::UnloadModule(const std::string& lib_name) {
  auto it = modules_.find(lib_name);
  if (it != modules_.end()) {
    modules_.erase(it);
    HILOG(kInfo, "Unloaded module: {}", lib_name);
  }
}

void ModuleManager::UnloadAllModules() {
  modules_.clear();
  HILOG(kInfo, "Unloaded all modules");
}

}  // namespace chi

#endif  // CHIMAERA_RUNTIME

HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(chi::ModuleManager, chiModuleManager);