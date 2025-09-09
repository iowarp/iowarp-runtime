/**
 * Comprehensive unit tests for bdev ChiMod
 * 
 * Tests the complete bdev functionality: container creation, block allocation,
 * write/read operations, async I/O, performance metrics, and error handling.
 * Uses simple custom test framework for testing.
 */

#include "../simple_test.h"
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>

using namespace std::chrono_literals;

// Include Chimaera headers
#include <chimaera/chimaera.h>
#include <chimaera/singletons.h>
#include <chimaera/types.h>
#include <chimaera/pool_query.h>

// Include bdev client and tasks
#include <bdev/bdev_client.h>
#include <bdev/bdev_tasks.h>

// Include admin client for pool management
#include <admin/admin_client.h>
#include <admin/admin_tasks.h>

namespace {
  // Test configuration constants
  constexpr chi::u32 kTestTimeoutMs = 10000;
  constexpr chi::u32 kMaxRetries = 100;
  constexpr chi::u32 kRetryDelayMs = 50;
  
  // Note: Tests use default pool ID (0) instead of hardcoding specific values
  
  // Test file configurations
  const std::string kTestFilePrefix = "/tmp/test_bdev_";
  const chi::u64 kDefaultFileSize = 10 * 1024 * 1024;  // 10MB
  const chi::u64 kLargeFileSize = 100 * 1024 * 1024;   // 100MB
  
  // Block size constants for testing
  const chi::u64 k4KB = 4096;
  const chi::u64 k64KB = 65536;
  const chi::u64 k256KB = 262144;
  const chi::u64 k1MB = 1048576;
  
  // Global test state
  bool g_runtime_initialized = false;
  bool g_client_initialized = false;
  int g_test_counter = 0;

  /**
   * Simple test fixture for bdev ChiMod tests
   * Handles setup and teardown of runtime, client, and test files
   */
  class BdevChimodFixture {
  public:
    BdevChimodFixture() : current_test_file_("") {
      // Generate unique test file name
      current_test_file_ = kTestFilePrefix + std::to_string(getpid()) + 
                          "_" + std::to_string(++g_test_counter) + ".dat";
    }
    
    ~BdevChimodFixture() {
      cleanup();
    }
    
    /**
     * Initialize Chimaera runtime (server-side)
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
        
        INFO("Runtime initialization successful");
      } else {
        INFO("Failed to initialize Chimaera runtime");
      }
      
      return success;
    }
    
    /**
     * Initialize Chimaera client components
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
        
        INFO("Client initialization successful");
      } else {
        INFO("Failed to initialize Chimaera client");
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
     * Create a test file with specified size
     */
    bool createTestFile(chi::u64 size = kDefaultFileSize) {
      // Create the test file
      std::ofstream file(current_test_file_, std::ios::binary);
      if (!file.is_open()) {
        return false;
      }
      
      // Write zeros to create file of specified size
      std::vector<char> buffer(4096, 0);
      chi::u64 written = 0;
      
      while (written < size) {
        chi::u64 to_write = std::min(static_cast<chi::u64>(buffer.size()), size - written);
        file.write(buffer.data(), to_write);
        if (!file.good()) {
          file.close();
          return false;
        }
        written += to_write;
      }
      
      file.close();
      
      // Verify file was created with correct size
      struct stat st;
      if (stat(current_test_file_.c_str(), &st) != 0) {
        return false;
      }
      
      return static_cast<chi::u64>(st.st_size) == size;
    }
    
    /**
     * Get the current test file path
     */
    const std::string& getTestFile() const {
      return current_test_file_;
    }
    
    /**
     * Generate test data with specified pattern
     */
    std::vector<hshm::u8> generateTestData(size_t size, hshm::u8 pattern = 0xAB) {
      std::vector<hshm::u8> data;
      data.reserve(size);
      
      // Create a repeating pattern
      for (size_t i = 0; i < size; ++i) {
        data.push_back(static_cast<hshm::u8>((pattern + i) % 256));
      }
      
      return data;
    }
    
    /**
     * Clean up test resources
     */
    void cleanup() {
      // Remove test file if it exists
      if (!current_test_file_.empty()) {
        if (access(current_test_file_.c_str(), F_OK) == 0) {
          unlink(current_test_file_.c_str());
          INFO("Cleaned up test file: " + current_test_file_);
        }
      }
    }

  private:
    std::string current_test_file_;
  };

} // end anonymous namespace

//==============================================================================
// BASIC FUNCTIONALITY TESTS
//==============================================================================

TEST_CASE("bdev_container_creation", "[bdev][create]") {
  BdevChimodFixture fixture;
  
  SECTION("Initialize runtime and client") {
    REQUIRE(fixture.initializeBoth());
  }
  
  SECTION("Create test file") {
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }
  
  SECTION("Create bdev container with default parameters") {
    chimaera::bdev::Client client(100);  // Use non-zero pool ID
    hipc::MemContext mctx;
    
    REQUIRE_NOTHROW(client.Create(mctx, chi::PoolQuery::Local(), 
                                  fixture.getTestFile()));
    
    INFO("Successfully created bdev container with default parameters");
  }
}

TEST_CASE("bdev_block_allocation_4kb", "[bdev][allocate][4kb]") {
  BdevChimodFixture fixture;
  
  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }
  
  SECTION("Create container and allocate 4KB blocks") {
    chimaera::bdev::Client client(102);
    hipc::MemContext mctx;
    
    client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile());
    
    // Allocate multiple 4KB blocks
    std::vector<chimaera::bdev::Block> blocks;
    for (int i = 0; i < 5; ++i) {
      chimaera::bdev::Block block = client.Allocate(mctx, k4KB);
      
      REQUIRE(block.size_ >= k4KB);
      REQUIRE(block.block_type_ == 0);  // 4KB category
      REQUIRE(block.offset_ % 4096 == 0);  // Aligned
      
      blocks.push_back(block);
      INFO("Allocated 4KB block " + std::to_string(i) + ": offset=" + 
           std::to_string(block.offset_) + ", size=" + std::to_string(block.size_));
    }
    
    // Verify blocks don't overlap
    for (size_t i = 0; i < blocks.size(); ++i) {
      for (size_t j = i + 1; j < blocks.size(); ++j) {
        chi::u64 end_i = blocks[i].offset_ + blocks[i].size_;
        chi::u64 start_j = blocks[j].offset_;
        chi::u64 end_j = blocks[j].offset_ + blocks[j].size_;
        chi::u64 start_i = blocks[i].offset_;
        
        REQUIRE((end_i <= start_j) || (end_j <= start_i));
      }
    }
  }
}

TEST_CASE("bdev_write_read_basic", "[bdev][io][basic]") {
  BdevChimodFixture fixture;
  
  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }
  
  SECTION("Write and read data verification") {
    chimaera::bdev::Client client(103);
    hipc::MemContext mctx;
    
    client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile());
    
    // Allocate a block
    chimaera::bdev::Block block = client.Allocate(mctx, k4KB);
    
    // Generate test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k4KB, 0xCD);
    
    // Write data
    chi::u64 bytes_written = client.Write(mctx, block, write_data);
    REQUIRE(bytes_written == write_data.size());
    
    // Read data back
    std::vector<hshm::u8> read_data = client.Read(mctx, block);
    REQUIRE(read_data.size() == write_data.size());
    
    // Verify data matches
    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_data[i] == write_data[i]);
    }
    
    INFO("Successfully wrote and read " + std::to_string(bytes_written) + " bytes");
  }
}

TEST_CASE("bdev_async_operations", "[bdev][async][io]") {
  BdevChimodFixture fixture;
  
  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kLargeFileSize));
  }
  
  SECTION("Async allocate, write, and read") {
    chimaera::bdev::Client client(104);
    hipc::MemContext mctx;
    
    client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile());
    
    // Async allocate
    auto alloc_task = client.AsyncAllocate(mctx, k64KB);
    alloc_task->Wait();
    REQUIRE(alloc_task->result_code_ == 0);
    
    chimaera::bdev::Block block = alloc_task->block_;
    CHI_IPC->DelTask(alloc_task);
    
    // Prepare test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k64KB, 0xEF);
    
    // Async write
    auto write_task = client.AsyncWrite(mctx, block, write_data);
    write_task->Wait();
    REQUIRE(write_task->result_code_ == 0);
    REQUIRE(write_task->bytes_written_ == write_data.size());
    CHI_IPC->DelTask(write_task);
    
    // Async read
    auto read_task = client.AsyncRead(mctx, block);
    read_task->Wait();
    REQUIRE(read_task->result_code_ == 0);
    REQUIRE(read_task->bytes_read_ == write_data.size());
    
    // Verify data
    REQUIRE(read_task->data_.size() == write_data.size());
    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_task->data_[i] == write_data[i]);
    }
    
    CHI_IPC->DelTask(read_task);
    
    INFO("Successfully completed async allocate/write/read cycle");
  }
}

TEST_CASE("bdev_performance_metrics", "[bdev][performance][metrics]") {
  BdevChimodFixture fixture;
  
  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kLargeFileSize));
  }
  
  SECTION("Track performance metrics during operations") {
    chimaera::bdev::Client client(105);
    hipc::MemContext mctx;
    
    client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile());
    
    // Get initial stats
    chi::u64 initial_remaining;
    chimaera::bdev::PerfMetrics initial_metrics = client.GetStats(mctx, initial_remaining);
    
    REQUIRE(initial_remaining > 0);
    REQUIRE(initial_metrics.read_bandwidth_mbps_ >= 0.0);
    REQUIRE(initial_metrics.write_bandwidth_mbps_ >= 0.0);
    
    // Perform some I/O operations
    chimaera::bdev::Block block1 = client.Allocate(mctx, k1MB);
    chimaera::bdev::Block block2 = client.Allocate(mctx, k256KB);
    
    std::vector<hshm::u8> data1 = fixture.generateTestData(k1MB, 0x12);
    std::vector<hshm::u8> data2 = fixture.generateTestData(k256KB, 0x34);
    
    client.Write(mctx, block1, data1);
    client.Write(mctx, block2, data2);
    
    client.Read(mctx, block1);
    client.Read(mctx, block2);
    
    // Get updated stats
    chi::u64 final_remaining;
    chimaera::bdev::PerfMetrics final_metrics = client.GetStats(mctx, final_remaining);
    
    // Remaining space should have decreased
    REQUIRE(final_remaining < initial_remaining);
    
    // Performance metrics should be updated (may be zero for very fast operations)
    REQUIRE(final_metrics.read_bandwidth_mbps_ >= 0.0);
    REQUIRE(final_metrics.write_bandwidth_mbps_ >= 0.0);
    REQUIRE(final_metrics.iops_ >= 0.0);
    
    INFO("Initial remaining: " + std::to_string(initial_remaining) + 
         ", Final remaining: " + std::to_string(final_remaining));
    INFO("Read BW: " + std::to_string(final_metrics.read_bandwidth_mbps_) + " MB/s");
    INFO("Write BW: " + std::to_string(final_metrics.write_bandwidth_mbps_) + " MB/s");
    INFO("IOPS: " + std::to_string(final_metrics.iops_));
  }
}

TEST_CASE("bdev_error_conditions", "[bdev][error][edge_cases]") {
  BdevChimodFixture fixture;
  
  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }
  
  SECTION("Handle invalid file paths") {
    chimaera::bdev::Client client(106);
    hipc::MemContext mctx;
    
    // Try to create with non-existent file
    auto create_task = client.AsyncCreate(mctx, chi::PoolQuery::Local(), 
                                         "/nonexistent/path/file.dat");
    create_task->Wait();
    REQUIRE(create_task->result_code_ != 0);  // Should fail
    CHI_IPC->DelTask(create_task);
    
    INFO("Correctly handled invalid file path");
  }
}

//==============================================================================
// MAIN TEST RUNNER
//==============================================================================

SIMPLE_TEST_MAIN()