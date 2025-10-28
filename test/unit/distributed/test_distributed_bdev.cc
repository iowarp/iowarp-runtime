/**
 * Distributed unit tests for iowarp runtime
 *
 * Tests distributed operations using DirectHash, Range, and Broadcast query modes.
 * Verifies data integrity across distributed operations.
 */

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../../simple_test.h"

using namespace std::chrono_literals;

// Include Chimaera headers
#include <chimaera/chimaera.h>
#include <chimaera/pool_query.h>
#include <chimaera/singletons.h>
#include <chimaera/types.h>

// Include bdev client and tasks
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/bdev/bdev_tasks.h>

// Include admin client for pool management
#include <chimaera/admin/admin_client.h>
#include <chimaera/admin/admin_tasks.h>

namespace {
// Test configuration constants
constexpr chi::u32 kTestTimeoutMs = 10000;
constexpr chi::u32 kMaxRetries = 100;
constexpr chi::u32 kRetryDelayMs = 50;

// Test file configurations
const std::string kTestFilePrefix = "/tmp/test_distributed_bdev_";
const chi::u64 kDefaultFileSize = 10 * 1024 * 1024;  // 10MB

// Block size constants for testing
const chi::u64 k4KB = 4096;

// Global test state
int g_test_counter = 0;

// Test parameters from command line
int g_num_nodes = 1;
std::string g_test_case = "direct";

/**
 * Simple test fixture for distributed bdev tests
 * Handles setup and teardown of runtime, client, and test files
 */
class DistributedBdevFixture {
 public:
  DistributedBdevFixture() : current_test_file_("") {
    // Generate unique test file name
    current_test_file_ = kTestFilePrefix + std::to_string(getpid()) + "_" +
                         std::to_string(++g_test_counter) + ".dat";
  }

  ~DistributedBdevFixture() { cleanup(); }

  /**
   * Initialize Chimaera client components
   * Note: Runtime is started separately in containers via chi_start_runtime
   */
  bool initializeClient() {
    HILOG(kInfo, "Initializing Chimaera client...");
    bool success = chi::CHIMAERA_CLIENT_INIT();

    if (success) {
      // Give client time to connect to runtime
      std::this_thread::sleep_for(200ms);

      HILOG(kInfo, "Client initialization successful");
    } else {
      HILOG(kInfo, "Failed to initialize Chimaera client");
    }

    return success;
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
      chi::u64 to_write =
          std::min(static_cast<chi::u64>(buffer.size()), size - written);
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
  const std::string& getTestFile() const { return current_test_file_; }

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
        HILOG(kInfo, "Cleaned up test file: {}", current_test_file_);
      }
    }
  }

 private:
  std::string current_test_file_;
};

}  // end anonymous namespace

//==============================================================================
// DISTRIBUTED TEST CASES
//==============================================================================

TEST_CASE("distributed_bdev_direct_hash", "[distributed][direct_hash]") {
  DistributedBdevFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeClient());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("DirectHash test with loop iterations") {
    chi::PoolId custom_pool_id(300, 0);
    chimaera::bdev::Client client(custom_pool_id);
    hipc::MemContext mctx;

    // Create bdev container
    bool success = client.Create(mctx, chi::PoolQuery::Broadcast(),
                                 fixture.getTestFile(), custom_pool_id,
                                 chimaera::bdev::BdevType::kFile);
    REQUIRE(success);
    REQUIRE(client.GetReturnCode() == 0);

    HILOG(kInfo, "Starting DirectHash test with 100 iterations");

    // Get IPC manager for creating tasks directly
    auto* ipc_manager = CHI_IPC;

    // Run 100 iterations with DirectHash pool queries
    for (int i = 0; i < 100; i++) {
      auto pool_query = chi::PoolQuery::DirectHash(i);

      // Allocate block with DirectHash query - use NewTask directly
      auto alloc_task = ipc_manager->NewTask<chimaera::bdev::AllocateBlocksTask>(
          chi::CreateTaskId(), client.pool_id_, pool_query, k4KB);
      ipc_manager->Enqueue(alloc_task);
      alloc_task->Wait();
      REQUIRE(alloc_task->return_code_.load() == 0);
      REQUIRE(alloc_task->blocks_.size() > 0);

      chimaera::bdev::Block block = alloc_task->blocks_[0];
      CHI_IPC->DelTask(alloc_task);

      // Generate test data with iteration-specific pattern
      std::vector<hshm::u8> write_data = fixture.generateTestData(k4KB, static_cast<hshm::u8>(i));

      // Allocate buffer and write data
      auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
      REQUIRE_FALSE(write_buffer.IsNull());
      std::memcpy(write_buffer.ptr_, write_data.data(), write_data.size());

      // Write with DirectHash query - use NewTask directly
      auto write_task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
          chi::CreateTaskId(), client.pool_id_, pool_query, block,
          write_buffer.shm_, write_data.size());
      ipc_manager->Enqueue(write_task);
      write_task->Wait();
      REQUIRE(write_task->return_code_.load() == 0);
      REQUIRE(write_task->bytes_written_ == write_data.size());
      CHI_IPC->DelTask(write_task);

      // Allocate buffer and read data back
      auto read_buffer = CHI_IPC->AllocateBuffer(k4KB);
      REQUIRE_FALSE(read_buffer.IsNull());

      // Read with DirectHash query - use NewTask directly
      auto read_task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
          chi::CreateTaskId(), client.pool_id_, pool_query, block,
          read_buffer.shm_, k4KB);
      ipc_manager->Enqueue(read_task);
      read_task->Wait();
      REQUIRE(read_task->return_code_.load() == 0);
      REQUIRE(read_task->bytes_read_ == write_data.size());

      // Verify data integrity
      std::vector<hshm::u8> read_data(read_task->bytes_read_);
      std::memcpy(read_data.data(), read_buffer.ptr_, read_task->bytes_read_);

      for (size_t j = 0; j < write_data.size(); ++j) {
        REQUIRE(read_data[j] == write_data[j]);
      }

      CHI_IPC->DelTask(read_task);

      // Free the block with DirectHash query - use NewTask directly
      std::vector<chimaera::bdev::Block> free_blocks;
      free_blocks.push_back(block);
      auto free_task = ipc_manager->NewTask<chimaera::bdev::FreeBlocksTask>(
          chi::CreateTaskId(), client.pool_id_, pool_query, free_blocks);
      ipc_manager->Enqueue(free_task);
      free_task->Wait();
      REQUIRE(free_task->return_code_.load() == 0);
      CHI_IPC->DelTask(free_task);

      if ((i + 1) % 10 == 0) {
        HILOG(kInfo, "Completed {} DirectHash iterations", i + 1);
      }
    }

    HILOG(kInfo, "DirectHash test completed successfully - 100 iterations");
  }
}

TEST_CASE("distributed_bdev_range", "[distributed][range]") {
  DistributedBdevFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeClient());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("Range test with multiple operations") {
    chi::PoolId custom_pool_id(301, 0);
    chimaera::bdev::Client client(custom_pool_id);
    hipc::MemContext mctx;

    // Create bdev container
    bool success = client.Create(mctx, chi::PoolQuery::Broadcast(),
                                 fixture.getTestFile(), custom_pool_id,
                                 chimaera::bdev::BdevType::kFile);
    REQUIRE(success);
    REQUIRE(client.GetReturnCode() == 0);

    HILOG(kInfo, "Starting Range test with range (0, {})", g_num_nodes);

    // Get IPC manager for creating tasks directly
    auto* ipc_manager = CHI_IPC;

    // Create range query for all nodes
    auto pool_query = chi::PoolQuery::Range(0, g_num_nodes);

    // Allocate block with Range query - use NewTask directly
    auto alloc_task = ipc_manager->NewTask<chimaera::bdev::AllocateBlocksTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, k4KB);
    ipc_manager->Enqueue(alloc_task);
    alloc_task->Wait();
    REQUIRE(alloc_task->return_code_.load() == 0);
    REQUIRE(alloc_task->blocks_.size() > 0);

    chimaera::bdev::Block block = alloc_task->blocks_[0];
    CHI_IPC->DelTask(alloc_task);

    // Generate test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k4KB, 0xCD);

    // Allocate buffer and write data with Range query
    auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
    REQUIRE_FALSE(write_buffer.IsNull());
    std::memcpy(write_buffer.ptr_, write_data.data(), write_data.size());

    // Write with Range query - use NewTask directly
    auto write_task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, block,
        write_buffer.shm_, write_data.size());
    ipc_manager->Enqueue(write_task);
    write_task->Wait();
    REQUIRE(write_task->return_code_.load() == 0);
    REQUIRE(write_task->bytes_written_ == write_data.size());
    CHI_IPC->DelTask(write_task);

    HILOG(kInfo, "Range write completed - {} bytes", write_data.size());

    // Allocate buffer and read data back with Range query
    auto read_buffer = CHI_IPC->AllocateBuffer(k4KB);
    REQUIRE_FALSE(read_buffer.IsNull());

    // Read with Range query - use NewTask directly
    auto read_task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, block,
        read_buffer.shm_, k4KB);
    ipc_manager->Enqueue(read_task);
    read_task->Wait();
    REQUIRE(read_task->return_code_.load() == 0);
    REQUIRE(read_task->bytes_read_ == write_data.size());

    // Verify data integrity
    std::vector<hshm::u8> read_data(read_task->bytes_read_);
    std::memcpy(read_data.data(), read_buffer.ptr_, read_task->bytes_read_);

    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_data[i] == write_data[i]);
    }

    CHI_IPC->DelTask(read_task);

    HILOG(kInfo, "Range read verified - data integrity confirmed");

    // Free the block with Range query - use NewTask directly
    std::vector<chimaera::bdev::Block> free_blocks;
    free_blocks.push_back(block);
    auto free_task = ipc_manager->NewTask<chimaera::bdev::FreeBlocksTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, free_blocks);
    ipc_manager->Enqueue(free_task);
    free_task->Wait();
    REQUIRE(free_task->return_code_.load() == 0);
    CHI_IPC->DelTask(free_task);

    HILOG(kInfo, "Range test completed successfully");
  }
}

TEST_CASE("distributed_bdev_broadcast", "[distributed][broadcast]") {
  DistributedBdevFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeClient());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("Broadcast test with all nodes") {
    chi::PoolId custom_pool_id(302, 0);
    chimaera::bdev::Client client(custom_pool_id);
    hipc::MemContext mctx;

    // Create bdev container
    bool success = client.Create(mctx, chi::PoolQuery::Broadcast(),
                                 fixture.getTestFile(), custom_pool_id,
                                 chimaera::bdev::BdevType::kFile);
    REQUIRE(success);
    REQUIRE(client.GetReturnCode() == 0);

    HILOG(kInfo, "Starting Broadcast test");

    // Get IPC manager for creating tasks directly
    auto* ipc_manager = CHI_IPC;

    // Create broadcast query
    auto pool_query = chi::PoolQuery::Broadcast();

    // Allocate block with Broadcast query - use NewTask directly
    auto alloc_task = ipc_manager->NewTask<chimaera::bdev::AllocateBlocksTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, k4KB);
    ipc_manager->Enqueue(alloc_task);
    alloc_task->Wait();
    REQUIRE(alloc_task->return_code_.load() == 0);
    REQUIRE(alloc_task->blocks_.size() > 0);

    chimaera::bdev::Block block = alloc_task->blocks_[0];
    CHI_IPC->DelTask(alloc_task);

    // Generate test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k4KB, 0xEF);

    // Allocate buffer and write data with Broadcast query
    auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
    REQUIRE_FALSE(write_buffer.IsNull());
    std::memcpy(write_buffer.ptr_, write_data.data(), write_data.size());

    // Write with Broadcast query - use NewTask directly
    auto write_task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, block,
        write_buffer.shm_, write_data.size());
    ipc_manager->Enqueue(write_task);
    write_task->Wait();
    REQUIRE(write_task->return_code_.load() == 0);
    REQUIRE(write_task->bytes_written_ == write_data.size());
    CHI_IPC->DelTask(write_task);

    HILOG(kInfo, "Broadcast write completed - {} bytes", write_data.size());

    // Allocate buffer and read data back with Broadcast query
    auto read_buffer = CHI_IPC->AllocateBuffer(k4KB);
    REQUIRE_FALSE(read_buffer.IsNull());

    // Read with Broadcast query - use NewTask directly
    auto read_task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, block,
        read_buffer.shm_, k4KB);
    ipc_manager->Enqueue(read_task);
    read_task->Wait();
    REQUIRE(read_task->return_code_.load() == 0);
    REQUIRE(read_task->bytes_read_ == write_data.size());

    // Verify data integrity
    std::vector<hshm::u8> read_data(read_task->bytes_read_);
    std::memcpy(read_data.data(), read_buffer.ptr_, read_task->bytes_read_);

    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_data[i] == write_data[i]);
    }

    CHI_IPC->DelTask(read_task);

    HILOG(kInfo, "Broadcast read verified - data integrity confirmed");

    // Free the block with Broadcast query - use NewTask directly
    std::vector<chimaera::bdev::Block> free_blocks;
    free_blocks.push_back(block);
    auto free_task = ipc_manager->NewTask<chimaera::bdev::FreeBlocksTask>(
        chi::CreateTaskId(), client.pool_id_, pool_query, free_blocks);
    ipc_manager->Enqueue(free_task);
    free_task->Wait();
    REQUIRE(free_task->return_code_.load() == 0);
    CHI_IPC->DelTask(free_task);

    HILOG(kInfo, "Broadcast test completed successfully");
  }
}

//==============================================================================
// MAIN TEST RUNNER
//==============================================================================

int main(int argc, char* argv[]) {
  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--num-nodes" && i + 1 < argc) {
      g_num_nodes = std::stoi(argv[++i]);
    } else if (arg == "--test-case" && i + 1 < argc) {
      g_test_case = argv[++i];
    }
  }

  HILOG(kInfo, "Distributed BDev Test Configuration:");
  HILOG(kInfo, "  Number of nodes: {}", g_num_nodes);
  HILOG(kInfo, "  Test case: {}", g_test_case);

  // Run the appropriate test case based on command line argument
  if (g_test_case == "direct") {
    HILOG(kInfo, "Running DirectHash test case");
    return SimpleTest::run_all_tests();
  } else if (g_test_case == "range") {
    HILOG(kInfo, "Running Range test case");
    return SimpleTest::run_all_tests();
  } else if (g_test_case == "broadcast") {
    HILOG(kInfo, "Running Broadcast test case");
    return SimpleTest::run_all_tests();
  } else {
    HILOG(kError, "Unknown test case: {}", g_test_case);
    HILOG(kInfo, "Valid test cases: direct, range, broadcast");
    return 1;
  }
}
