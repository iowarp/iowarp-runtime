/**
 * Module manager implementation with dynamic ChiMod loading
 */

#include "chimaera/module_manager.h"
#include <iostream>
#include <filesystem>
#include <cstring>

// Global pointer variable definition for Module manager singleton
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(chi::ModuleManager, g_module_manager);

namespace chi {

// Constructor and destructor removed - handled by HSHM singleton pattern

bool ModuleManager::ServerInit() {
  if (is_initialized_) {
    return true;
  }

  std::cout << "Initializing Module Manager..." << std::endl;
  
  // Scan for and load ChiMods
  ScanForChiMods();
  
  std::cout << "Module Manager initialized with " << chimods_.size() 
            << " ChiMods loaded" << std::endl;
  
  is_initialized_ = true;
  return true;
}

void ModuleManager::Finalize() {
  if (!is_initialized_) {
    return;
  }

  std::cout << "Finalizing Module Manager..." << std::endl;
  
  // Clear all loaded ChiMods - SharedLibrary destructor handles cleanup
  chimods_.clear();

  is_initialized_ = false;
}

bool ModuleManager::LoadChiMod(const std::string& lib_path) {
  std::cout << "Loading ChiMod from: " << lib_path << std::endl;
  
  auto chimod_info = std::make_unique<ChiModInfo>();
  chimod_info->lib_path = lib_path;
  
  // Load the shared library
  chimod_info->lib.Load(lib_path);
  if (chimod_info->lib.IsNull()) {
    std::cerr << "Failed to load library: " << chimod_info->lib.GetError() << std::endl;
    return false;
  }
  
  // Validate ChiMod entry points
  if (!ValidateChiMod(chimod_info->lib)) {
    std::cerr << "Library " << lib_path << " is not a valid ChiMod" << std::endl;
    return false;
  }
  
  // Get function pointers
  chimod_info->alloc_func = (alloc_chimod_t)chimod_info->lib.GetSymbol("alloc_chimod");
  chimod_info->new_func = (new_chimod_t)chimod_info->lib.GetSymbol("new_chimod");
  chimod_info->name_func = (get_chimod_name_t)chimod_info->lib.GetSymbol("get_chimod_name");
  chimod_info->destroy_func = (destroy_chimod_t)chimod_info->lib.GetSymbol("destroy_chimod");
  
  // Get ChiMod name
  if (chimod_info->name_func) {
    chimod_info->name = chimod_info->name_func();
    std::cout << "Loaded ChiMod: " << chimod_info->name << std::endl;
  } else {
    std::cerr << "ChiMod missing get_chimod_name function" << std::endl;
    return false;
  }
  
  // Store in map
  chimods_[chimod_info->name] = std::move(chimod_info);
  return true;
}

ChiModInfo* ModuleManager::GetChiMod(const std::string& chimod_name) {
  auto it = chimods_.find(chimod_name);
  return (it != chimods_.end()) ? it->second.get() : nullptr;
}

ChiContainer* ModuleManager::CreateContainer(const std::string& chimod_name, 
                                           const PoolId& pool_id, 
                                           const std::string& pool_name) {
  ChiModInfo* chimod = GetChiMod(chimod_name);
  if (!chimod || !chimod->new_func) {
    return nullptr;
  }
  
  return chimod->new_func(&pool_id, pool_name.c_str());
}

void ModuleManager::DestroyContainer(const std::string& chimod_name, ChiContainer* container) {
  ChiModInfo* chimod = GetChiMod(chimod_name);
  if (chimod && chimod->destroy_func && container) {
    chimod->destroy_func(container);
  }
}

std::vector<std::string> ModuleManager::GetLoadedChiMods() const {
  std::vector<std::string> names;
  for (const auto& pair : chimods_) {
    names.push_back(pair.first);
  }
  return names;
}

bool ModuleManager::IsInitialized() const {
  return is_initialized_;
}

void ModuleManager::ScanForChiMods() {
  std::vector<std::string> scan_dirs = GetScanDirectories();
  
  for (const std::string& dir : scan_dirs) {
    std::cout << "Scanning directory for ChiMods: " << dir << std::endl;
    
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
      continue;
    }
    
    try {
      for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
          std::string file_path = entry.path().string();
          if (IsSharedLibrary(file_path)) {
            LoadChiMod(file_path);
          }
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "Error scanning directory " << dir << ": " << e.what() << std::endl;
    }
  }
}

std::vector<std::string> ModuleManager::GetScanDirectories() const {
  std::vector<std::string> directories;
  
  // Get CHI_REPO_PATH
  const char* chi_repo_path = std::getenv("CHI_REPO_PATH");
  if (chi_repo_path) {
    std::string path_str(chi_repo_path);
    // Split by colon (Unix) or semicolon (Windows)
    char delimiter = ':';
#ifdef _WIN32
    delimiter = ';';
#endif
    
    size_t start = 0;
    size_t end = path_str.find(delimiter);
    while (end != std::string::npos) {
      directories.push_back(path_str.substr(start, end - start));
      start = end + 1;
      end = path_str.find(delimiter, start);
    }
    directories.push_back(path_str.substr(start));
  }
  
  // Get LD_LIBRARY_PATH
  const char* ld_path = std::getenv("LD_LIBRARY_PATH");
  if (ld_path) {
    std::string path_str(ld_path);
    size_t start = 0;
    size_t end = path_str.find(':');
    while (end != std::string::npos) {
      directories.push_back(path_str.substr(start, end - start));
      start = end + 1;
      end = path_str.find(':', start);
    }
    directories.push_back(path_str.substr(start));
  }
  
  // Add default directories
  directories.push_back("./lib");
  directories.push_back("../lib");
  directories.push_back("/usr/local/lib");
  
  return directories;
}

bool ModuleManager::IsSharedLibrary(const std::string& file_path) const {
  // Check file extension
#ifdef _WIN32
  const std::string ext = ".dll";
#elif __APPLE__
  const std::string ext = ".dylib";
#else
  const std::string ext = ".so";
#endif
  return file_path.length() >= ext.length() && 
         file_path.compare(file_path.length() - ext.length(), ext.length(), ext) == 0;
}

bool ModuleManager::ValidateChiMod(hshm::SharedLibrary& lib) const {
  // Check for required ChiMod functions
  void* alloc_func = lib.GetSymbol("alloc_chimod");
  void* new_func = lib.GetSymbol("new_chimod");
  void* name_func = lib.GetSymbol("get_chimod_name");
  void* destroy_func = lib.GetSymbol("destroy_chimod");
  
  return (alloc_func != nullptr && new_func != nullptr && 
          name_func != nullptr && destroy_func != nullptr);
}

}  // namespace chi