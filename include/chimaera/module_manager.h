#ifndef CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_MODULE_MANAGER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_MODULE_MANAGER_H_

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "chimaera/types.h"

namespace chi {

// Forward declarations for ChiMod system
#ifndef CHIMAERA_RUNTIME
using ChiContainer = void;
#else
// When CHIMAERA_RUNTIME is defined, ChiContainer is defined in chimod_spec.h
class ChiContainer;
#endif

// ChiMod function types
typedef ChiContainer* (*alloc_chimod_t)();
typedef ChiContainer* (*new_chimod_t)(const PoolId* pool_id, const char* pool_name);
typedef const char* (*get_chimod_name_t)(void);
typedef void (*destroy_chimod_t)(ChiContainer* container);

/**
 * ChiMod metadata and shared library wrapper
 */
struct ChiModInfo {
  std::string name;
  std::string lib_path;
  hshm::SharedLibrary lib;
  
  // Function pointers
  alloc_chimod_t alloc_func;
  new_chimod_t new_func;
  get_chimod_name_t name_func;
  destroy_chimod_t destroy_func;
  
  ChiModInfo() : alloc_func(nullptr), new_func(nullptr), 
                 name_func(nullptr), destroy_func(nullptr) {}
};

/**
 * Module Manager singleton for dynamic loading of ChiMods
 * 
 * Handles discovery and loading of ChiMod shared libraries from:
 * - LD_LIBRARY_PATH directories
 * - CHI_REPO_PATH directories
 * 
 * Each ChiMod provides functions to query name and allocate ChiContainers.
 * Uses HSHM SharedLibrary for cross-platform dynamic loading.
 */
class ModuleManager {
 public:
  /**
   * Initialize module manager (generic wrapper)
   * Scans for and loads all available ChiMods
   * @return true if initialization successful, false otherwise
   */
  bool Init() { return ServerInit(); }

  /**
   * Initialize module manager (server/runtime only)
   * Scans for and loads all available ChiMods
   * @return true if initialization successful, false otherwise
   */
  bool ServerInit();

  /**
   * Finalize and cleanup module resources
   */
  void Finalize();

  /**
   * Load a specific ChiMod from path
   * @param lib_path Path to shared library
   * @return true if loaded successfully, false otherwise
   */
  bool LoadChiMod(const std::string& lib_path);

  /**
   * Get ChiMod by name
   * @param chimod_name Name of ChiMod
   * @return Pointer to ChiModInfo or nullptr if not found
   */
  ChiModInfo* GetChiMod(const std::string& chimod_name);

  /**
   * Create ChiContainer instance from ChiMod
   * @param chimod_name Name of ChiMod to instantiate
   * @param pool_id Pool identifier for the container
   * @param pool_name Pool name for the container
   * @return Pointer to ChiContainer or nullptr if failed
   */
  ChiContainer* CreateContainer(const std::string& chimod_name, 
                                const PoolId& pool_id, 
                                const std::string& pool_name);

  /**
   * Destroy ChiContainer instance
   * @param chimod_name Name of ChiMod that created the container
   * @param container Pointer to container to destroy
   */
  void DestroyContainer(const std::string& chimod_name, ChiContainer* container);

  /**
   * Get list of loaded ChiMod names
   * @return Vector of ChiMod names
   */
  std::vector<std::string> GetLoadedChiMods() const;

  /**
   * Check if module manager is initialized
   * @return true if initialized, false otherwise
   */
  bool IsInitialized() const;

 private:
  /**
   * Scan directories for ChiMod libraries
   */
  void ScanForChiMods();

  /**
   * Get list of directories to scan from environment
   * @return Vector of directory paths
   */
  std::vector<std::string> GetScanDirectories() const;

  /**
   * Check if file is a potential ChiMod library
   * @param file_path Path to file
   * @return true if file looks like a shared library
   */
  bool IsSharedLibrary(const std::string& file_path) const;

  /**
   * Validate that library has required ChiMod entry points
   * @param lib Shared library to validate
   * @return true if library has required functions
   */
  bool ValidateChiMod(hshm::SharedLibrary& lib) const;

  bool is_initialized_ = false;
  
  // Map ChiMod name to ChiModInfo
  std::map<std::string, std::unique_ptr<ChiModInfo>> chimods_;
};

}  // namespace chi

// Macro for accessing the Module manager singleton using HSHM singleton
#define CHI_MODULE hshm::Singleton<ModuleManager>::GetInstance()

#endif  // CHIMAERA_INCLUDE_CHIMAERA_MANAGERS_MODULE_MANAGER_H_