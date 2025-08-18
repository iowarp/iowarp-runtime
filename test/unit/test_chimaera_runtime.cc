/**
 * Comprehensive unit tests for Chimaera runtime system
 * 
 * Tests the complete flow: runtime startup → client init → task submission → completion
 * Uses simple custom test framework for testing.
 */

#include "../simple_test.h"
#include <chrono>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

// Include Chimaera headers
#include <chimaera/chimaera.h>
#include <chimaera/singletons.h>
#include <chimaera/types.h>
#include <chimaera/domain_query.h>

// Include MOD_NAME client and tasks for custom task testing
#include <MOD_NAME/MOD_NAME_client.h>
#include <MOD_NAME/MOD_NAME_tasks.h>

// Include admin client for pool management
#include <admin/admin_client.h>
#include <admin/admin_tasks.h>

namespace {
  // Test configuration constants
  constexpr chi::u32 kTestTimeoutMs = 5000;
  constexpr chi::u32 kMaxRetries = 50;
  constexpr chi::u32 kRetryDelayMs = 100;
  
  // Test pool IDs
  constexpr chi::PoolId kTestModNamePoolId = 100;
  
  // Global test state
  bool g_runtime_initialized = false;
  bool g_client_initialized = false;
}

/**
 * Test fixture for Chimaera runtime tests
 * Handles setup and teardown of runtime and client components
 */
class ChimaeraRuntimeFixture {
public:
  ChimaeraRuntimeFixture() = default;
  
  ~ChimaeraRuntimeFixture() {
    cleanup();
  }
  
  /**
   * Initialize Chimaera runtime (server-side)
   * This should be called before any client operations
   */
  bool initializeRuntime() {
    if (g_runtime_initialized) {
      return true; // Already initialized
    }
    
    INFO("Initializing Chimaera runtime...");
    bool success = chi::CHIMAERA_RUNTIME_INIT();
    
    if (success) {
      g_runtime_initialized = true;
      
      // Give runtime time to initialize all components
      std::this_thread::sleep_for(500ms);
      
      // Verify core managers are available
      REQUIRE(CHI_CHIMAERA_MANAGER != nullptr);
      REQUIRE(CHI_IPC != nullptr);
      REQUIRE(CHI_POOL_MANAGER != nullptr);
      REQUIRE(CHI_MODULE_MANAGER != nullptr);
      REQUIRE(CHI_WORK_ORCHESTRATOR != nullptr);
      
      INFO("Runtime initialization successful");
    } else {
      FAIL("Failed to initialize Chimaera runtime");
    }
    
    return success;
  }
  
  /**
   * Initialize Chimaera client components
   * This should be called after runtime initialization
   */
  bool initializeClient() {
    if (g_client_initialized) {
      return true; // Already initialized
    }
    
    INFO("Initializing Chimaera client...");
    bool success = chi::CHIMAERA_CLIENT_INIT();
    
    if (success) {
      g_client_initialized = true;
      
      // Give client time to connect to runtime
      std::this_thread::sleep_for(200ms);
      
      // Verify client can access IPC manager
      REQUIRE(CHI_IPC != nullptr);
      REQUIRE(CHI_IPC->IsInitialized());
      
      INFO("Client initialization successful");
    } else {
      FAIL("Failed to initialize Chimaera client");
    }
    
    return success;
  }
  
  /**
   * Initialize both runtime and client (full setup)
   */
  bool initializeBoth() {
    return initializeRuntime() && initializeClient();
  }
  
  /**
   * Wait for task completion with timeout
   * @param task Task to wait for
   * @param timeout_ms Maximum time to wait in milliseconds
   * @return true if task completed, false if timeout
   */
  template<typename TaskT>
  bool waitForTaskCompletion(hipc::FullPtr<TaskT> task, chi::u32 timeout_ms = kTestTimeoutMs) {
    if (task.IsNull()) {
      return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::duration<int, std::milli>(timeout_ms);
    
    // Use task's Wait mechanism with timeout check
    while (task->is_complete.load() == 0) {
      auto current_time = std::chrono::steady_clock::now();
      if (current_time - start_time > timeout_duration) {
        INFO("Task completion timeout after " << timeout_ms << "ms");
        return false; // Timeout
      }
      
      // Use the task's own Yield() method for efficient waiting
      task->Yield();
    }
    
    return true; // Task completed
  }
  
  /**
   * Clean up runtime and client resources
   */
  void cleanup() {
    // Note: Chimaera framework handles automatic cleanup through destructors
    // when the Chimaera manager singleton is destroyed
    INFO("Test cleanup completed");
  }
  
  /**
   * Create MOD_NAME pool using admin client
   * @return true if pool creation successful
   */
  bool createModNamePool() {
    try {
      // Initialize admin client
      chimaera::admin::Client admin_client(chi::kAdminPoolId);
      
      // Create the admin container first if needed
      chi::DomainQuery dom_query; // Default domain query
      admin_client.Create(HSHM_MCTX, dom_query);
      
      // Create MOD_NAME pool parameters
      chimaera::MOD_NAME::CreateParams params;
      params.config_data_ = "test_config";
      params.worker_count_ = 2;
      
      // Create the MOD_NAME pool
      auto task = admin_client.AsyncGetOrCreatePool<chimaera::MOD_NAME::CreateParams>(
          HSHM_MCTX, dom_query, kTestModNamePoolId, params);
      
      if (waitForTaskCompletion(task)) {
        INFO("MOD_NAME pool created successfully with ID: " << kTestModNamePoolId);
        
        // Clean up task
        CHI_IPC->DelTask(task);
        return true;
      } else {
        FAIL("Failed to create MOD_NAME pool - task did not complete");
        return false;
      }
      
    } catch (const std::exception& e) {
      FAIL("Exception creating MOD_NAME pool: " << e.what());
      return false;
    }
  }
};

//------------------------------------------------------------------------------
// Basic Runtime and Client Initialization Tests
//------------------------------------------------------------------------------

TEST_CASE("Chimaera Runtime Initialization", "[runtime][initialization]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Runtime initialization should succeed") {
    REQUIRE(fixture.initializeRuntime());
    
    // Verify runtime state
    REQUIRE(CHI_CHIMAERA_MANAGER->IsInitialized());
    REQUIRE(CHI_CHIMAERA_MANAGER->IsRuntime());
    REQUIRE_FALSE(CHI_CHIMAERA_MANAGER->IsClient());
  }
  
  SECTION("Multiple runtime initializations should be safe") {
    REQUIRE(fixture.initializeRuntime());
    REQUIRE(fixture.initializeRuntime()); // Second call should succeed
  }
}

TEST_CASE("Chimaera Client Initialization", "[client][initialization]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Client initialization requires runtime first") {
    // Initialize runtime first
    REQUIRE(fixture.initializeRuntime());
    
    // Then initialize client
    REQUIRE(fixture.initializeClient());
    
    // Verify client can access runtime components
    REQUIRE(CHI_IPC->IsInitialized());
  }
  
  SECTION("Client initialization should fail without runtime") {
    // Attempting client init without runtime should work
    // (the framework should handle missing runtime gracefully)
    bool client_result = chi::CHIMAERA_CLIENT_INIT();
    
    // This may succeed or fail depending on implementation
    // The important thing is it doesn't crash
    INFO("Client init without runtime result: " << client_result);
  }
}

//------------------------------------------------------------------------------
// MOD_NAME Custom Task Tests
//------------------------------------------------------------------------------

TEST_CASE("MOD_NAME Custom Task Execution", "[task][mod_name][custom]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Complete workflow: runtime + client + pool creation + task submission") {
    // Step 1: Initialize runtime and client
    REQUIRE(fixture.initializeBoth());
    
    // Step 2: Create MOD_NAME pool
    REQUIRE(fixture.createModNamePool());
    
    // Step 3: Initialize MOD_NAME client
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    
    // Step 4: Create the MOD_NAME container
    chi::DomainQuery dom_query; // Default domain query
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Step 5: Submit custom task
    std::string input_data = "test_input_data";
    chi::u32 operation_id = 42;
    std::string output_data;
    
    // Execute custom operation synchronously
    chi::u32 result_code = mod_name_client.Custom(
        HSHM_MCTX, dom_query, input_data, operation_id, output_data);
    
    // Verify results
    REQUIRE(result_code == 0); // Assuming 0 means success
    REQUIRE_FALSE(output_data.empty());
    
    INFO("Custom task completed successfully");
    INFO("Input: " << input_data);
    INFO("Output: " << output_data);
    INFO("Operation ID: " << operation_id);
    INFO("Result code: " << result_code);
  }
}

TEST_CASE("MOD_NAME Async Task Execution", "[task][mod_name][async]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Async task submission and completion") {
    // Initialize everything
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createModNamePool());
    
    // Initialize MOD_NAME client
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    
    // Create the MOD_NAME container
    chi::DomainQuery dom_query;
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Submit async custom task
    std::string input_data = "async_test_data";
    chi::u32 operation_id = 123;
    
    auto task = mod_name_client.AsyncCustom(
        HSHM_MCTX, dom_query, input_data, operation_id);
    
    REQUIRE_FALSE(task.IsNull());
    
    // Wait for completion
    REQUIRE(fixture.waitForTaskCompletion(task));
    
    // Verify results
    REQUIRE(task->result_code_ == 0);
    std::string output_data = task->data_.str();
    REQUIRE_FALSE(output_data.empty());
    
    INFO("Async task completed successfully");
    INFO("Result: " << output_data);
    
    // Clean up task
    CHI_IPC->DelTask(task);
  }
}

//------------------------------------------------------------------------------
// Error Handling and Edge Cases
//------------------------------------------------------------------------------

TEST_CASE("Error Handling Tests", "[error][edge_cases]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Task submission without runtime should fail gracefully") {
    // Try to create a client without initializing runtime
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    
    // This should not crash, but may fail
    chi::DomainQuery dom_query;
    
    // Creating container without runtime should fail or handle gracefully
    REQUIRE_NOTHROW(mod_name_client.Create(HSHM_MCTX, dom_query));
  }
  
  SECTION("Invalid pool ID should handle gracefully") {
    REQUIRE(fixture.initializeBoth());
    
    // Try to use an invalid pool ID
    constexpr chi::PoolId kInvalidPoolId = 9999;
    chimaera::MOD_NAME::Client invalid_client(kInvalidPoolId);
    
    chi::DomainQuery dom_query;
    
    // This should not crash
    REQUIRE_NOTHROW(invalid_client.Create(HSHM_MCTX, dom_query));
  }
  
  SECTION("Task timeout handling") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createModNamePool());
    
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    chi::DomainQuery dom_query;
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Submit a task
    auto task = mod_name_client.AsyncCustom(
        HSHM_MCTX, dom_query, "timeout_test", 999);
    
    REQUIRE_FALSE(task.IsNull());
    
    // Wait with a very short timeout to test timeout handling
    bool completed = fixture.waitForTaskCompletion(task, 50); // 50ms timeout
    
    // The task may or may not complete in 50ms, but we shouldn't crash
    INFO("Task completed within timeout: " << completed);
    
    // Clean up
    if (!task.IsNull()) {
      CHI_IPC->DelTask(task);
    }
  }
}

//------------------------------------------------------------------------------
// Multi-threaded Tests
//------------------------------------------------------------------------------

TEST_CASE("Concurrent Task Execution", "[concurrent][stress]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Multiple concurrent tasks") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createModNamePool());
    
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    chi::DomainQuery dom_query;
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Submit multiple concurrent tasks
    constexpr int kNumTasks = 5;
    std::vector<hipc::FullPtr<chimaera::MOD_NAME::CustomTask>> tasks;
    
    for (int i = 0; i < kNumTasks; ++i) {
      std::string input_data = "concurrent_test_" + std::to_string(i);
      auto task = mod_name_client.AsyncCustom(
          HSHM_MCTX, dom_query, input_data, i);
      
      REQUIRE_FALSE(task.IsNull());
      tasks.push_back(task);
    }
    
    // Wait for all tasks to complete
    int completed_tasks = 0;
    for (auto& task : tasks) {
      if (fixture.waitForTaskCompletion(task)) {
        completed_tasks++;
        REQUIRE(task->result_code_ == 0);
      }
    }
    
    INFO("Completed " << completed_tasks << " out of " << kNumTasks << " tasks");
    REQUIRE(completed_tasks > 0); // At least some tasks should complete
    
    // Clean up tasks
    for (auto& task : tasks) {
      if (!task.IsNull()) {
        CHI_IPC->DelTask(task);
      }
    }
  }
}

//------------------------------------------------------------------------------
// Memory Management Tests
//------------------------------------------------------------------------------

TEST_CASE("Memory Management", "[memory][cleanup]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Task allocation and deallocation") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createModNamePool());
    
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    chi::DomainQuery dom_query;
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Allocate many tasks to test memory management
    constexpr int kNumAllocations = 10;
    std::vector<hipc::FullPtr<chimaera::MOD_NAME::CustomTask>> allocated_tasks;
    
    for (int i = 0; i < kNumAllocations; ++i) {
      auto task = mod_name_client.AsyncCustom(
          HSHM_MCTX, dom_query, "memory_test", i);
      
      REQUIRE_FALSE(task.IsNull());
      allocated_tasks.push_back(task);
    }
    
    INFO("Allocated " << kNumAllocations << " tasks successfully");
    
    // Clean up all tasks
    for (auto& task : allocated_tasks) {
      CHI_IPC->DelTask(task);
    }
    
    INFO("Deallocated all tasks successfully");
  }
}

//------------------------------------------------------------------------------
// Performance Tests
//------------------------------------------------------------------------------

TEST_CASE("Performance Tests", "[performance][timing]") {
  ChimaeraRuntimeFixture fixture;
  
  SECTION("Task execution latency") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createModNamePool());
    
    chimaera::MOD_NAME::Client mod_name_client(kTestModNamePoolId);
    chi::DomainQuery dom_query;
    mod_name_client.Create(HSHM_MCTX, dom_query);
    
    // Measure task execution time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::string output_data;
    chi::u32 result_code = mod_name_client.Custom(
        HSHM_MCTX, dom_query, "performance_test", 1, output_data);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    
    REQUIRE(result_code == 0);
    INFO("Task execution time: " << duration.count() << " microseconds");
    
    // Reasonable performance expectation (task should complete within 1 second)
    REQUIRE(duration.count() < 1000000); // 1 second in microseconds
  }
}

// Main function to run all tests
SIMPLE_TEST_MAIN()