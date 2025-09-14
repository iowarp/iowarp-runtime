/**
 * Chimaera runtime startup utility
 */

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "chimaera/chimaera.h"
#include "chimaera/singletons.h"
#include "chimaera/types.h"

namespace {
volatile bool g_keep_running = true;

void signal_handler(int signal) {
  if (signal == SIGINT) {
    // Ctrl-C should exit immediately without graceful shutdown
    std::cout << "Received SIGINT (Ctrl-C), exiting immediately..." << std::endl;
    std::exit(1);
  } else if (signal == SIGTERM) {
    // SIGTERM allows graceful shutdown
    std::cout << "Received SIGTERM, shutting down gracefully..." << std::endl;
    g_keep_running = false;
  } else {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_keep_running = false;
  }
}

/**
 * Find and initialize the admin ChiMod
 * Creates a ChiPool for the admin module using PoolManager
 * @return true if successful, false on failure
 */
bool InitializeAdminChiMod() {
  std::cout << "Initializing admin ChiMod..." << std::endl;

  // Get the module manager to find the admin chimod
  auto* module_manager = CHI_MODULE_MANAGER;
  if (!module_manager) {
    std::cerr << "Module manager not available" << std::endl;
    return false;
  }

  // Check if admin chimod is available
  auto* admin_chimod = module_manager->GetChiMod("chimaera_admin");
  if (!admin_chimod) {
    std::cerr << "CRITICAL: Admin ChiMod not found! This is a required system "
                 "component."
              << std::endl;
    return false;
  }

  // Get the pool manager to register the admin pool
  auto* pool_manager = CHI_POOL_MANAGER;
  if (!pool_manager) {
    std::cerr << "Pool manager not available" << std::endl;
    return false;
  }

  try {
    // Use PoolManager to create the admin pool
    // This functionality is now handled by PoolManager::ServerInit()
    // which calls CreatePool internally with proper task and RunContext
    // No need to manually create admin pool here anymore
    std::cout << "Admin pool creation handled by PoolManager::ServerInit()" << std::endl;

    // Verify the pool was created successfully
    if (!pool_manager->HasPool(chi::kAdminPoolId)) {
      std::cerr << "Admin pool creation reported success but pool is not found"
                << std::endl;
      return false;
    }

    std::cout << "Admin ChiPool created successfully (ID: " << chi::kAdminPoolId
              << ")" << std::endl;
    return true;

  } catch (const std::exception& e) {
    std::cerr << "Exception during admin ChiMod initialization: " << e.what()
              << std::endl;
    return false;
  }
}

/**
 * Shutdown the admin ChiMod properly
 */
void ShutdownAdminChiMod() {
  std::cout << "Shutting down admin ChiMod..." << std::endl;

  try {
    // Get the pool manager to destroy the admin pool
    auto* pool_manager = CHI_POOL_MANAGER;
    if (pool_manager && pool_manager->HasPool(chi::kAdminPoolId)) {
      // Use PoolManager to destroy the admin pool locally
      if (pool_manager->DestroyLocalPool(chi::kAdminPoolId)) {
        std::cout << "Admin pool destroyed successfully" << std::endl;
      } else {
        std::cerr << "Failed to destroy admin pool" << std::endl;
      }
    }

  } catch (const std::exception& e) {
    std::cerr << "Exception during admin ChiMod shutdown: " << e.what()
              << std::endl;
  }

  std::cout << "Admin ChiMod shutdown complete" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::cout << "Starting Chimaera runtime..." << std::endl;

  // Set up signal handling
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Initialize Chimaera runtime
  if (!chi::CHIMAERA_RUNTIME_INIT()) {
    std::cerr << "Failed to initialize Chimaera runtime" << std::endl;
    return 1;
  }

  std::cout << "Chimaera runtime started successfully" << std::endl;

  // Find and initialize admin ChiMod
  if (!InitializeAdminChiMod()) {
    std::cerr << "FATAL ERROR: Failed to find or initialize admin ChiMod"
              << std::endl;
    return 1;
  }

  std::cout << "Admin ChiMod initialized successfully with pool ID "
            << chi::kAdminPoolId << std::endl;

  // Main runtime loop
  while (g_keep_running) {
    // Sleep for a short period
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "Shutting down Chimaera runtime..." << std::endl;

  // Shutdown admin pool first
  ShutdownAdminChiMod();

  std::cout << "Chimaera runtime stopped (finalization will happen automatically)" << std::endl;
  return 0;
}