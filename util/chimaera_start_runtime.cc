/**
 * Chimaera runtime startup utility
 */

#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include "chimaera/chimaera.h"

namespace {
volatile bool g_keep_running = true;

void signal_handler(int signal) {
  std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
  g_keep_running = false;
}
}

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

  // Main runtime loop
  while (g_keep_running) {
    // Sleep for a short period
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "Shutting down Chimaera runtime..." << std::endl;

  // Finalize Chimaera
  chi::CHIMAERA_FINALIZE();

  std::cout << "Chimaera runtime stopped" << std::endl;
  return 0;
}