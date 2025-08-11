/**
 * Central header for Chimaera singleton access macros
 * 
 * This header provides convenient macros for accessing all Chimaera singletons
 * using HSHM's global cross pointer variable pattern. Include this header to
 * get access to all singleton macros in one place.
 */

#ifndef CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_
#define CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_

#include "chimaera/chimaera_manager.h"
#include "chimaera/config_manager.h"
#include "chimaera/ipc_manager.h"
#include "chimaera/pool_manager.h"
#include "chimaera/module_manager.h"
#include "chimaera/work_orchestrator.h"

/**
 * Convenience macros for accessing Chimaera singletons
 * 
 * These macros provide easy access to all Chimaera singleton managers
 * using HSHM's global cross pointer variable pattern.
 */

// Core Framework Singleton Access
// CHI_CHIMAERA - Main Chimaera framework coordinator
// CHI_CONFIG   - Configuration manager for YAML parsing
// CHI_IPC      - IPC manager for shared memory and networking
// CHI_POOL_MANAGER - Pool manager for ChiPools and ChiContainers  
// CHI_MODULE   - Module manager for dynamic loading
// CHI_WORK_ORCHESTRATOR - Work orchestrator for thread management

// All macros are defined in their respective header files:
// - CHI_CHIMAERA defined in chimaera/chimaera_manager.h
// - CHI_CONFIG defined in chimaera/config_manager.h
// - CHI_IPC defined in chimaera/ipc_manager.h
// - CHI_POOL_MANAGER defined in chimaera/pool_manager.h
// - CHI_MODULE defined in chimaera/module_manager.h
// - CHI_WORK_ORCHESTRATOR defined in chimaera/work_orchestrator.h

/**
 * Example usage:
 * 
 * // Initialize the configuration manager
 * CHI_CONFIG->Init();
 * 
 * // Get worker thread count from config
 * u32 workers = CHI_CONFIG->GetWorkerThreadCount(ThreadType::kLowLatency);
 * 
 * // Initialize IPC components
 * CHI_IPC->ServerInit();
 * 
 * // Start worker threads
 * CHI_WORK_ORCHESTRATOR->Init();
 * CHI_WORK_ORCHESTRATOR->StartWorkers();
 * 
 * // Register a pool
 * CHI_POOL_MANAGER->RegisterContainer(pool_id, container);
 */

#endif  // CHIMAERA_INCLUDE_CHIMAERA_SINGLETONS_H_