/**
 * Chimaera runtime shutdown utility
 *
 * Connects to the running Chimaera runtime and sends a StopRuntimeTask
 * via the admin ChiMod client to initiate graceful shutdown.
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "chimaera/admin/admin_client.h"
#include "chimaera/chimaera.h"
#include "chimaera/pool_query.h"
#include "chimaera/types.h"

int main(int argc, char* argv[]) {
  std::cout << "Stopping Chimaera runtime..." << std::endl;

  try {
    // Initialize Chimaera client components
    std::cout << "Initializing Chimaera client..." << std::endl;
    if (!chi::CHIMAERA_CLIENT_INIT()) {
      std::cerr << "Failed to initialize Chimaera client components"
                << std::endl;
      return 1;
    }

    std::cout << "Creating admin client connection..." << std::endl;
    // Create admin client connected to admin pool
    chimaera::admin::Client admin_client(chi::kAdminPoolId);

    // Check if IPC manager is available
    auto* ipc_manager = CHI_IPC;
    if (!ipc_manager || !ipc_manager->IsInitialized()) {
      std::cerr << "IPC manager not available - is Chimaera runtime running?"
                << std::endl;
      return 1;
    }

    // Additional validation: check if TaskQueue is accessible
    auto* task_queue = ipc_manager->GetTaskQueue();
    if (!task_queue || task_queue->IsNull()) {
      std::cerr
          << "TaskQueue not available - runtime may not be properly initialized"
          << std::endl;
      return 1;
    }

    // Validate that task queue has valid lane configuration
    try {
      chi::u32 num_lanes = task_queue->GetNumLanes();
      if (num_lanes == 0) {
        std::cerr << "TaskQueue has no lanes configured - runtime "
                     "initialization incomplete"
                  << std::endl;
        return 1;
      }
      std::cout << "TaskQueue validated with " << num_lanes << " lanes"
                << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "TaskQueue validation failed: " << e.what() << std::endl;
      return 1;
    }

    // Create domain query for local execution
    chi::PoolQuery pool_query;

    // Parse command line arguments for shutdown parameters
    chi::u32 shutdown_flags = 0;
    chi::u32 grace_period_ms = 5000;  // Default 5 seconds

    if (argc >= 2) {
      grace_period_ms = static_cast<chi::u32>(std::atoi(argv[1]));
      if (grace_period_ms == 0) {
        grace_period_ms = 5000;  // Fallback to default
      }
    }

    std::cout << "Sending stop runtime task to admin pool (grace period: "
              << grace_period_ms << "ms)..." << std::endl;

    // Send StopRuntimeTask via admin client
    std::cout << "Calling admin client AsyncStopRuntime..." << std::endl;
    auto start_time = std::chrono::steady_clock::now();

    // Use the admin client's AsyncStopRuntime method - fire and forget
    hipc::FullPtr<chimaera::admin::StopRuntimeTask> stop_task;
    try {
      stop_task = admin_client.AsyncStopRuntime(
          HSHM_MCTX, pool_query, shutdown_flags, grace_period_ms);
      if (stop_task.IsNull()) {
        std::cerr
            << "Failed to create stop runtime task - runtime may not be running"
            << std::endl;
        return 1;
      }
    } catch (const std::exception& e) {
      std::cerr << "Error creating stop runtime task: " << e.what()
                << std::endl;
      return 1;
    }

    std::cout << "Stop runtime task submitted successfully (fire-and-forget)"
              << std::endl;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time)
                        .count();

    std::cout << "Runtime stop task submitted in " << duration << "ms"
              << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Error stopping runtime: " << e.what() << std::endl;
    return 1;
  }
}