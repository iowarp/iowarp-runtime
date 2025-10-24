/**
 * Comprehensive unit tests for bdev ChiMod
 *
 * Tests the complete bdev functionality: container creation, block allocation,
 * write/read operations, async I/O, performance metrics, and error handling.
 * Uses simple custom test framework for testing.
 */

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "../simple_test.h"

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
    current_test_file_ = kTestFilePrefix + std::to_string(getpid()) + "_" +
                         std::to_string(++g_test_counter) + ".dat";
  }

  ~BdevChimodFixture() { cleanup(); }

  /**
   * Initialize Chimaera runtime (server-side)
   */
  bool initializeRuntime() {
    if (g_runtime_initialized) {
      return true;  // Already initialized
    }

    HILOG(kInfo, "Initializing Chimaera runtime...");
    bool success = chi::CHIMAERA_RUNTIME_INIT();

    if (success) {
      g_runtime_initialized = true;

      // Give runtime time to initialize all components
      std::this_thread::sleep_for(500ms);

      HILOG(kInfo, "Runtime initialization successful");
    } else {
      HILOG(kInfo, "Failed to initialize Chimaera runtime");
    }

    return success;
  }

  /**
   * Initialize Chimaera client components
   */
  bool initializeClient() {
    if (g_client_initialized) {
      return true;  // Already initialized
    }

    HILOG(kInfo, "Initializing Chimaera client...");
    bool success = chi::CHIMAERA_CLIENT_INIT();

    if (success) {
      g_client_initialized = true;

      // Give client time to connect to runtime
      std::this_thread::sleep_for(200ms);

      HILOG(kInfo, "Client initialization successful");
    } else {
      HILOG(kInfo, "Failed to initialize Chimaera client");
    }

    return success;
  }

  /**
   * Initialize both runtime and client (full setup)
   */
  bool initializeBoth() { return initializeRuntime() && initializeClient(); }

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
    chimaera::bdev::Client client(chi::PoolId(100, 0));  // Use non-zero pool ID
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), 
                                 chimaera::bdev::BdevType::kFile);
    REQUIRE(success);

    HILOG(kInfo, "Successfully created bdev container with default parameters");
  }
}

TEST_CASE("bdev_block_allocation_4kb", "[bdev][allocate][4kb]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("Create container and allocate 4KB blocks") {
    chimaera::bdev::Client client(chi::PoolId(102, 0));
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
    REQUIRE(success);

    // Allocate multiple 4KB blocks
    std::vector<chimaera::bdev::Block> blocks;
    for (int i = 0; i < 5; ++i) {
      std::vector<chimaera::bdev::Block> allocated_blocks = client.AllocateBlocks(mctx, k4KB);
      REQUIRE(allocated_blocks.size() > 0);
      chimaera::bdev::Block block = allocated_blocks[0];

      REQUIRE(block.size_ >= k4KB);
      REQUIRE(block.block_type_ == 0);     // 4KB category
      REQUIRE(block.offset_ % 4096 == 0);  // Aligned

      blocks.push_back(block);
      HILOG(kInfo, "Allocated 4KB block {}: offset={}, size={}", i,
            block.offset_, block.size_);
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
    chimaera::bdev::Client client(chi::PoolId(103, 0));
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
    REQUIRE(success);

    // Allocate a block
    std::vector<chimaera::bdev::Block> blocks = client.AllocateBlocks(mctx, k4KB);
    REQUIRE(blocks.size() > 0);
    chimaera::bdev::Block block = blocks[0];

    // Generate test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k4KB, 0xCD);

    // Write data - allocate buffer and copy data
    auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
    REQUIRE_FALSE(write_buffer.IsNull());
    memcpy(write_buffer.ptr_, write_data.data(), write_data.size());
    
    // Convert to hipc::Pointer for Write call - use shm_ member
    chi::u64 bytes_written = client.Write(mctx, block, write_buffer.shm_, write_data.size());
    REQUIRE(bytes_written == write_data.size());

    // Read data back - allocate buffer for reading
    auto read_buffer = CHI_IPC->AllocateBuffer(k4KB);
    REQUIRE_FALSE(read_buffer.IsNull());
    chi::u64 bytes_read = client.Read(mctx, block, read_buffer.shm_, k4KB);
    REQUIRE(bytes_read == write_data.size());
    
    // Convert read data back to vector for verification
    std::vector<hshm::u8> read_data(bytes_read);
    memcpy(read_data.data(), read_buffer.ptr_, bytes_read);

    // Verify data matches
    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_data[i] == write_data[i]);
    }

    HILOG(kInfo, "Successfully wrote and read {} bytes", bytes_written);
  }
}

TEST_CASE("bdev_async_operations", "[bdev][async][io]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kLargeFileSize));
  }

  SECTION("Async allocate, write, and read") {
    chimaera::bdev::Client client(chi::PoolId(104, 0));
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
    REQUIRE(success);

    // Async allocate
    auto alloc_task = client.AsyncAllocateBlocks(mctx, k64KB);
    alloc_task->Wait();
    REQUIRE(alloc_task->return_code_ == 0);

    chimaera::bdev::Block block;
    REQUIRE(alloc_task->blocks_.size() > 0);
    block = alloc_task->blocks_[0];
    CHI_IPC->DelTask(alloc_task);

    // Prepare test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k64KB, 0xEF);

    // Async write - allocate buffer and copy data
    auto async_write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
    REQUIRE_FALSE(async_write_buffer.IsNull());
    memcpy(async_write_buffer.ptr_, write_data.data(), write_data.size());
    
    auto write_task = client.AsyncWrite(mctx, block, async_write_buffer.shm_, write_data.size());
    write_task->Wait();
    REQUIRE(write_task->return_code_ == 0);
    REQUIRE(write_task->bytes_written_ == write_data.size());
    CHI_IPC->DelTask(write_task);

    // Async read - allocate buffer for reading
    auto async_read_buffer = CHI_IPC->AllocateBuffer(k64KB);
    REQUIRE_FALSE(async_read_buffer.IsNull());
    
    auto read_task = client.AsyncRead(mctx, block, async_read_buffer.shm_, k64KB);
    read_task->Wait();
    REQUIRE(read_task->return_code_ == 0);
    REQUIRE(read_task->bytes_read_ == write_data.size());

    // Verify data - copy from buffer to check
    std::vector<hshm::u8> async_read_data(read_task->bytes_read_);
    memcpy(async_read_data.data(), async_read_buffer.ptr_, read_task->bytes_read_);
    REQUIRE(async_read_data.size() == write_data.size());
    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(async_read_data[i] == write_data[i]);
    }

    CHI_IPC->DelTask(read_task);

    HILOG(kInfo, "Successfully completed async allocate/write/read cycle");
  }
}

TEST_CASE("bdev_performance_metrics", "[bdev][performance][metrics]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kLargeFileSize));
  }

  SECTION("Track performance metrics during operations") {
    chimaera::bdev::Client client(chi::PoolId(105, 0));
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
    REQUIRE(success);

    // Get initial stats
    chi::u64 initial_remaining;
    chimaera::bdev::PerfMetrics initial_metrics =
        client.GetStats(mctx, initial_remaining);

    REQUIRE(initial_remaining > 0);
    REQUIRE(initial_metrics.read_bandwidth_mbps_ >= 0.0);
    REQUIRE(initial_metrics.write_bandwidth_mbps_ >= 0.0);

    // Perform some I/O operations
    std::vector<chimaera::bdev::Block> blocks1 = client.AllocateBlocks(mctx, k1MB);
    REQUIRE(blocks1.size() > 0);
    chimaera::bdev::Block block1 = blocks1[0];
    std::vector<chimaera::bdev::Block> blocks2 = client.AllocateBlocks(mctx, k256KB);
    REQUIRE(blocks2.size() > 0);
    chimaera::bdev::Block block2 = blocks2[0];

    std::vector<hshm::u8> data1 = fixture.generateTestData(k1MB, 0x12);
    std::vector<hshm::u8> data2 = fixture.generateTestData(k256KB, 0x34);

    // Allocate buffers for data1 write
    auto data1_write_buffer = CHI_IPC->AllocateBuffer(data1.size());
    REQUIRE_FALSE(data1_write_buffer.IsNull());
    memcpy(data1_write_buffer.ptr_, data1.data(), data1.size());
    
    // Allocate buffers for data2 write  
    auto data2_write_buffer = CHI_IPC->AllocateBuffer(data2.size());
    REQUIRE_FALSE(data2_write_buffer.IsNull());
    memcpy(data2_write_buffer.ptr_, data2.data(), data2.size());

    client.Write(mctx, block1, data1_write_buffer.shm_, data1.size());
    client.Write(mctx, block2, data2_write_buffer.shm_, data2.size());

    // Allocate buffers for reads
    auto data1_read_buffer = CHI_IPC->AllocateBuffer(k1MB);
    REQUIRE_FALSE(data1_read_buffer.IsNull());
    auto data2_read_buffer = CHI_IPC->AllocateBuffer(k256KB);
    REQUIRE_FALSE(data2_read_buffer.IsNull());

    client.Read(mctx, block1, data1_read_buffer.shm_, k1MB);
    client.Read(mctx, block2, data2_read_buffer.shm_, k256KB);

    // Get updated stats
    chi::u64 final_remaining;
    chimaera::bdev::PerfMetrics final_metrics =
        client.GetStats(mctx, final_remaining);

    // Remaining space should have decreased
    REQUIRE(final_remaining < initial_remaining);

    // Performance metrics should be updated (may be zero for very fast
    // operations)
    REQUIRE(final_metrics.read_bandwidth_mbps_ >= 0.0);
    REQUIRE(final_metrics.write_bandwidth_mbps_ >= 0.0);
    REQUIRE(final_metrics.iops_ >= 0.0);

    HILOG(kInfo, "Initial remaining: {} bytes, Final remaining: {} bytes",
          initial_remaining, final_remaining);
    HILOG(kInfo, "Read BW: {} MB/s", final_metrics.read_bandwidth_mbps_);
    HILOG(kInfo, "Write BW: {} MB/s", final_metrics.write_bandwidth_mbps_);
    HILOG(kInfo, "IOPS: {}", final_metrics.iops_);
  }
}

TEST_CASE("bdev_error_conditions", "[bdev][error][edge_cases]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("Handle invalid file paths") {
    chimaera::bdev::Client client(chi::PoolId(106, 0));
    hipc::MemContext mctx;

    // Try to create with non-existent file
    auto create_task = client.AsyncCreate(mctx, chi::PoolQuery::Local(),
                                          "/nonexistent/path/file.dat", chimaera::bdev::BdevType::kFile);
    create_task->Wait();
    REQUIRE(create_task->return_code_ != 0);  // Should fail
    CHI_IPC->DelTask(create_task);

    HILOG(kInfo, "Correctly handled invalid file path");
  }
}

//==============================================================================
// RAM BACKEND TESTS
//==============================================================================

TEST_CASE("bdev_ram_container_creation", "[bdev][ram][create]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client for RAM backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8001, 0));

  // Create RAM-based bdev container (1MB)
  const chi::u64 ram_size = 1024 * 1024;
  std::string pool_name = "ram_test_" + std::to_string(getpid()) + "_" + std::to_string(8001);
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), pool_name,
                                         chimaera::bdev::BdevType::kRam, ram_size);
  REQUIRE(bdev_success);

  std::this_thread::sleep_for(100ms);

  HILOG(kInfo, "RAM backend container created successfully with size: {} bytes",
        ram_size);
}

TEST_CASE("bdev_ram_allocation_and_io", "[bdev][ram][io]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client for RAM backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8002, 0));

  // Create RAM-based bdev container (1MB)
  const chi::u64 ram_size = 1024 * 1024;
  std::string pool_name = "ram_test_" + std::to_string(getpid()) + "_" + std::to_string(8002);
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), pool_name,
                                         chimaera::bdev::BdevType::kRam, ram_size);
  REQUIRE(bdev_success);
  std::this_thread::sleep_for(100ms);

  // Allocate a 4KB block
  std::vector<chimaera::bdev::Block> blocks = bdev_client.AllocateBlocks(HSHM_MCTX, k4KB);
  REQUIRE(blocks.size() > 0);
  chimaera::bdev::Block block = blocks[0];
  REQUIRE(block.size_ == k4KB);
  REQUIRE(block.offset_ < ram_size);

  // Prepare test data with pattern
  std::vector<hshm::u8> write_data(k4KB);
  for (size_t i = 0; i < write_data.size(); ++i) {
    write_data[i] = static_cast<hshm::u8>((i + 0xAB) % 256);
  }

  // Write data to RAM - allocate buffer and copy data
  auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
  REQUIRE_FALSE(write_buffer.IsNull());
  memcpy(write_buffer.ptr_, write_data.data(), write_data.size());
  
  chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, block, write_buffer.shm_, write_data.size());
  REQUIRE(bytes_written == k4KB);

  // Read data back from RAM - allocate buffer for reading
  auto read_buffer = CHI_IPC->AllocateBuffer(k4KB);
  REQUIRE_FALSE(read_buffer.IsNull());
  chi::u64 bytes_read = bdev_client.Read(HSHM_MCTX, block, read_buffer.shm_, k4KB);
  REQUIRE(bytes_read == k4KB);
  
  // Convert read data back to vector for verification
  std::vector<hshm::u8> read_data(bytes_read);
  memcpy(read_data.data(), read_buffer.ptr_, bytes_read);

  // Verify data integrity
  bool data_matches =
      std::equal(write_data.begin(), write_data.end(), read_data.begin());
  REQUIRE(data_matches);

  // Free the block
  std::vector<chimaera::bdev::Block> free_blocks;
  free_blocks.push_back(block);
  chi::u32 free_result = bdev_client.FreeBlocks(HSHM_MCTX, free_blocks);
  REQUIRE(free_result == 0);

  HILOG(kInfo, "RAM backend I/O operations completed successfully");
}

TEST_CASE("bdev_ram_large_blocks", "[bdev][ram][large]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client for RAM backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8003, 0));

  // Create RAM-based bdev container (10MB)
  const chi::u64 ram_size = 10 * 1024 * 1024;
  std::string pool_name = "ram_test_" + std::to_string(getpid()) + "_" + std::to_string(8003);
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), pool_name,
                                         chimaera::bdev::BdevType::kRam, ram_size);
  REQUIRE(bdev_success);
  std::this_thread::sleep_for(100ms);

  // Test different block sizes
  std::vector<chi::u64> block_sizes = {k4KB, k64KB, k256KB, k1MB};

  for (chi::u64 block_size : block_sizes) {
    HILOG(kInfo, "Testing RAM backend with block size: {} bytes", block_size);

    // Allocate block
    std::vector<chimaera::bdev::Block> blocks = bdev_client.AllocateBlocks(HSHM_MCTX, block_size);
    REQUIRE(blocks.size() > 0);
    chimaera::bdev::Block block = blocks[0];
    REQUIRE(block.size_ == block_size);

    // Create test pattern
    std::vector<hshm::u8> test_data(block_size);
    for (size_t i = 0; i < test_data.size(); i += 1024) {
      test_data[i] = static_cast<hshm::u8>((i / 1024) % 256);
    }

    // Write and read - allocate buffers
    auto test_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
    REQUIRE_FALSE(test_write_buffer.IsNull());
    memcpy(test_write_buffer.ptr_, test_data.data(), test_data.size());
    
    chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, block, test_write_buffer.shm_, test_data.size());
    REQUIRE(bytes_written == block_size);

    auto test_read_buffer = CHI_IPC->AllocateBuffer(block_size);
    REQUIRE_FALSE(test_read_buffer.IsNull());
    chi::u64 bytes_read = bdev_client.Read(HSHM_MCTX, block, test_read_buffer.shm_, block_size);
    REQUIRE(bytes_read == block_size);
    
    // Convert read data back to vector for verification
    std::vector<hshm::u8> read_data(bytes_read);
    memcpy(read_data.data(), test_read_buffer.ptr_, bytes_read);

    // Verify critical points in the data
    for (size_t i = 0; i < read_data.size(); i += 1024) {
      REQUIRE(read_data[i] == test_data[i]);
    }

    // Free block
    std::vector<chimaera::bdev::Block> free_blocks;
    free_blocks.push_back(block);
    chi::u32 free_result = bdev_client.FreeBlocks(HSHM_MCTX, free_blocks);
    REQUIRE(free_result == 0);
  }

  HILOG(kInfo, "RAM backend large block tests completed");
}

TEST_CASE("bdev_ram_performance", "[bdev][ram][performance]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client for RAM backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8004, 0));

  // Create RAM-based bdev container (100MB)
  const chi::u64 ram_size = 100 * 1024 * 1024;
  std::string pool_name = "ram_test_" + std::to_string(getpid()) + "_" + std::to_string(8004);
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), pool_name,
                                         chimaera::bdev::BdevType::kRam, ram_size);
  REQUIRE(bdev_success);
  std::this_thread::sleep_for(100ms);

  // Allocate a 1MB block
  std::vector<chimaera::bdev::Block> blocks = bdev_client.AllocateBlocks(HSHM_MCTX, k1MB);
  REQUIRE(blocks.size() > 0);
  chimaera::bdev::Block block = blocks[0];
  REQUIRE(block.size_ == k1MB);

  // Prepare test data
  std::vector<hshm::u8> test_data(k1MB, 0xCD);
  
  // Allocate buffer for write
  auto perf_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
  REQUIRE_FALSE(perf_write_buffer.IsNull());
  memcpy(perf_write_buffer.ptr_, test_data.data(), test_data.size());

  // Measure write performance
  auto write_start = std::chrono::high_resolution_clock::now();
  chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, block, perf_write_buffer.shm_, test_data.size());
  auto write_end = std::chrono::high_resolution_clock::now();

  REQUIRE(bytes_written == k1MB);

  // Allocate buffer for read
  auto perf_read_buffer = CHI_IPC->AllocateBuffer(k1MB);
  REQUIRE_FALSE(perf_read_buffer.IsNull());

  // Measure read performance
  auto read_start = std::chrono::high_resolution_clock::now();
  chi::u64 bytes_read = bdev_client.Read(HSHM_MCTX, block, perf_read_buffer.shm_, k1MB);
  auto read_end = std::chrono::high_resolution_clock::now();
  
  // Convert read data back to vector for verification
  std::vector<hshm::u8> read_data(bytes_read);
  memcpy(read_data.data(), perf_read_buffer.ptr_, bytes_read);

  REQUIRE(read_data.size() == k1MB);

  // Calculate performance metrics
  auto write_duration =
      std::chrono::duration<double, std::micro>(write_end - write_start);
  auto read_duration =
      std::chrono::duration<double, std::micro>(read_end - read_start);

  double write_mbps =
      (k1MB / (1024.0 * 1024.0)) / (write_duration.count() / 1000000.0);
  double read_mbps =
      (k1MB / (1024.0 * 1024.0)) / (read_duration.count() / 1000000.0);

  HILOG(kInfo, "RAM Backend Performance:");
  HILOG(kInfo, "  Write: {} MB/s ({} μs)", write_mbps, write_duration.count());
  HILOG(kInfo, "  Read:  {} MB/s ({} μs)", read_mbps, read_duration.count());

  // RAM should be very fast - expect sub-millisecond operations
  REQUIRE(write_duration.count() < 10000.0);  // Less than 10ms
  REQUIRE(read_duration.count() < 10000.0);   // Less than 10ms

  // Free block
  std::vector<chimaera::bdev::Block> free_blocks;
  free_blocks.push_back(block);
  chi::u32 free_result = bdev_client.FreeBlocks(HSHM_MCTX, free_blocks);
  REQUIRE(free_result == 0);
}

TEST_CASE("bdev_ram_bounds_checking", "[bdev][ram][bounds]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client for RAM backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8005, 0));

  // Create small RAM-based bdev container (64KB)
  const chi::u64 ram_size = 64 * 1024;
  std::string pool_name = "ram_test_" + std::to_string(getpid()) + "_" + std::to_string(8005);
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), pool_name,
                                         chimaera::bdev::BdevType::kRam, ram_size);
  REQUIRE(bdev_success);
  std::this_thread::sleep_for(100ms);

  // Create a block that would go beyond bounds
  chimaera::bdev::Block out_of_bounds_block;
  out_of_bounds_block.offset_ = ram_size - 1024;  // Near end of buffer
  out_of_bounds_block.size_ = 2048;               // Extends beyond buffer
  out_of_bounds_block.block_type_ = 0;

  // Prepare test data
  std::vector<hshm::u8> test_data(2048, 0xEF);

  // Write should fail with bounds check - allocate buffer
  auto error_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
  REQUIRE_FALSE(error_write_buffer.IsNull());
  memcpy(error_write_buffer.ptr_, test_data.data(), test_data.size());
  
  chi::u64 bytes_written =
      bdev_client.Write(HSHM_MCTX, out_of_bounds_block, error_write_buffer.shm_, test_data.size());
  REQUIRE(bytes_written == 0);  // Should fail

  // Read should also fail with bounds check - allocate buffer
  auto error_read_buffer = CHI_IPC->AllocateBuffer(2048);
  REQUIRE_FALSE(error_read_buffer.IsNull());
  chi::u64 bytes_read = bdev_client.Read(HSHM_MCTX, out_of_bounds_block, error_read_buffer.shm_, 2048);
  
  // Convert read data back to vector (should be empty due to error)
  std::vector<hshm::u8> read_data(bytes_read);
  if (bytes_read > 0) {
    memcpy(read_data.data(), error_read_buffer.ptr_, bytes_read);
  }
  REQUIRE(read_data.empty());  // Should fail

  HILOG(kInfo, "RAM backend bounds checking working correctly");
}

//==============================================================================
// FILE BACKEND TESTS (Enhanced)
//==============================================================================

TEST_CASE("bdev_file_vs_ram_comparison", "[bdev][file][ram][comparison]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());
  REQUIRE(fixture.createTestFile(kDefaultFileSize));

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create two bdev clients - one for file, one for RAM
  chimaera::bdev::Client file_client(chi::PoolId(8006, 0));
  chimaera::bdev::Client ram_client(chi::PoolId(8007, 0));

  // Create file-based container
  bool file_success = file_client.Create(HSHM_MCTX, chi::PoolQuery::Local(),
                                         fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
  REQUIRE(file_success);
  std::this_thread::sleep_for(100ms);

  // Create RAM-based container (same size as file)
  std::string ram_pool_name = "ram_comparison_" + std::to_string(getpid()) + "_" + std::to_string(8007);
  bool ram_success = ram_client.Create(HSHM_MCTX, chi::PoolQuery::Local(),
                                       ram_pool_name, chimaera::bdev::BdevType::kRam, kDefaultFileSize);
  REQUIRE(ram_success);
  std::this_thread::sleep_for(100ms);

  // Test same operations on both backends
  const chi::u64 test_size = k64KB;

  // Allocate blocks on both
  std::vector<chimaera::bdev::Block> file_blocks = file_client.AllocateBlocks(HSHM_MCTX, test_size);
  REQUIRE(file_blocks.size() > 0);
  chimaera::bdev::Block file_block = file_blocks[0];
  std::vector<chimaera::bdev::Block> ram_blocks = ram_client.AllocateBlocks(HSHM_MCTX, test_size);
  REQUIRE(ram_blocks.size() > 0);
  chimaera::bdev::Block ram_block = ram_blocks[0];

  REQUIRE(file_block.size_ == test_size);
  REQUIRE(ram_block.size_ == test_size);

  // Create identical test data
  std::vector<hshm::u8> test_data(test_size);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  for (size_t i = 0; i < test_data.size(); ++i) {
    test_data[i] = static_cast<hshm::u8>(dis(gen));
  }

  // Allocate buffer for file write
  auto file_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
  REQUIRE_FALSE(file_write_buffer.IsNull());
  memcpy(file_write_buffer.ptr_, test_data.data(), test_data.size());

  // Write to both backends and measure time
  auto file_write_start = std::chrono::high_resolution_clock::now();
  chi::u64 file_bytes_written =
      file_client.Write(HSHM_MCTX, file_block, file_write_buffer.shm_, test_data.size());
  auto file_write_end = std::chrono::high_resolution_clock::now();

  // Allocate buffer for ram write
  auto ram_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
  REQUIRE_FALSE(ram_write_buffer.IsNull());
  memcpy(ram_write_buffer.ptr_, test_data.data(), test_data.size());

  auto ram_write_start = std::chrono::high_resolution_clock::now();
  chi::u64 ram_bytes_written =
      ram_client.Write(HSHM_MCTX, ram_block, ram_write_buffer.shm_, test_data.size());
  auto ram_write_end = std::chrono::high_resolution_clock::now();

  REQUIRE(file_bytes_written == test_size);
  REQUIRE(ram_bytes_written == test_size);

  // Allocate buffer for file read
  auto file_read_buffer = CHI_IPC->AllocateBuffer(test_size);
  REQUIRE_FALSE(file_read_buffer.IsNull());

  // Read from both backends and measure time
  auto file_read_start = std::chrono::high_resolution_clock::now();
  chi::u64 file_bytes_read = file_client.Read(HSHM_MCTX, file_block, file_read_buffer.shm_, test_size);
  auto file_read_end = std::chrono::high_resolution_clock::now();
  
  // Convert read data back to vector
  std::vector<hshm::u8> file_read_data(file_bytes_read);
  memcpy(file_read_data.data(), file_read_buffer.ptr_, file_bytes_read);

  // Allocate buffer for ram read
  auto ram_read_buffer = CHI_IPC->AllocateBuffer(test_size);
  REQUIRE_FALSE(ram_read_buffer.IsNull());

  auto ram_read_start = std::chrono::high_resolution_clock::now();
  chi::u64 ram_bytes_read = ram_client.Read(HSHM_MCTX, ram_block, ram_read_buffer.shm_, test_size);
  auto ram_read_end = std::chrono::high_resolution_clock::now();
  
  // Convert read data back to vector
  std::vector<hshm::u8> ram_read_data(ram_bytes_read);
  memcpy(ram_read_data.data(), ram_read_buffer.ptr_, ram_bytes_read);

  REQUIRE(file_read_data.size() == test_size);
  REQUIRE(ram_read_data.size() == test_size);

  // Verify data integrity on both
  bool file_data_ok =
      std::equal(test_data.begin(), test_data.end(), file_read_data.begin());
  bool ram_data_ok =
      std::equal(test_data.begin(), test_data.end(), ram_read_data.begin());

  REQUIRE(file_data_ok);
  REQUIRE(ram_data_ok);

  // Calculate and compare performance
  auto file_write_time = std::chrono::duration<double, std::micro>(
      file_write_end - file_write_start);
  auto ram_write_time = std::chrono::duration<double, std::micro>(
      ram_write_end - ram_write_start);
  auto file_read_time = std::chrono::duration<double, std::micro>(
      file_read_end - file_read_start);
  auto ram_read_time =
      std::chrono::duration<double, std::micro>(ram_read_end - ram_read_start);

  HILOG(kInfo, "Performance Comparison (64KB operations):");
  HILOG(kInfo, "  File Write: {} μs", file_write_time.count());
  HILOG(kInfo, "  RAM Write:  {} μs", ram_write_time.count());
  HILOG(kInfo, "  File Read:  {} μs", file_read_time.count());
  HILOG(kInfo, "  RAM Read:   {} μs", ram_read_time.count());

  // RAM should be significantly faster
  REQUIRE(ram_write_time.count() < file_write_time.count());
  REQUIRE(ram_read_time.count() < file_read_time.count());

  // Clean up
  std::vector<chimaera::bdev::Block> file_free_blocks;
  file_free_blocks.push_back(file_block);
  file_client.FreeBlocks(HSHM_MCTX, file_free_blocks);
  std::vector<chimaera::bdev::Block> ram_free_blocks;
  ram_free_blocks.push_back(ram_block);
  ram_client.FreeBlocks(HSHM_MCTX, ram_free_blocks);
}

TEST_CASE("bdev_file_explicit_backend", "[bdev][file][explicit]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());
  REQUIRE(fixture.createTestFile(kDefaultFileSize));

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Create bdev client with explicit file backend
  chimaera::bdev::Client bdev_client(chi::PoolId(8008, 0));

  // Create file-based container using explicit backend type
  bool bdev_success = bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(),
                                         fixture.getTestFile(), chimaera::bdev::BdevType::kFile, 0,
                                         32, 4096);
  REQUIRE(bdev_success);
  std::this_thread::sleep_for(100ms);

  // Test basic operations
  std::vector<chimaera::bdev::Block> blocks = bdev_client.AllocateBlocks(HSHM_MCTX, k4KB);
  REQUIRE(blocks.size() > 0);
  chimaera::bdev::Block block = blocks[0];
  REQUIRE(block.size_ == k4KB);

  std::vector<hshm::u8> test_data(k4KB, 0x42);
  
  // Allocate buffers for Write/Read operations
  auto final_write_buffer = CHI_IPC->AllocateBuffer(test_data.size());
  REQUIRE_FALSE(final_write_buffer.IsNull());
  memcpy(final_write_buffer.ptr_, test_data.data(), test_data.size());
  
  chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, block, final_write_buffer.shm_, test_data.size());
  REQUIRE(bytes_written == k4KB);

  auto final_read_buffer = CHI_IPC->AllocateBuffer(k4KB);
  REQUIRE_FALSE(final_read_buffer.IsNull());
  chi::u64 bytes_read = bdev_client.Read(HSHM_MCTX, block, final_read_buffer.shm_, k4KB);
  REQUIRE(bytes_read == k4KB);
  
  // Convert read data back to vector for verification
  std::vector<hshm::u8> read_data(bytes_read);
  memcpy(read_data.data(), final_read_buffer.ptr_, bytes_read);

  bool data_ok =
      std::equal(test_data.begin(), test_data.end(), read_data.begin());
  REQUIRE(data_ok);

  std::vector<chimaera::bdev::Block> free_blocks;
  free_blocks.push_back(block);
  chi::u32 free_result = bdev_client.FreeBlocks(HSHM_MCTX, free_blocks);
  REQUIRE(free_result == 0);

  HILOG(kInfo,
        "File backend with explicit type specification working correctly");
}

TEST_CASE("bdev_error_conditions_enhanced", "[bdev][error][enhanced]") {
  BdevChimodFixture fixture;
  REQUIRE(fixture.initializeBoth());

  // Admin client is automatically initialized via CHI_ADMIN singleton
  std::this_thread::sleep_for(100ms);

  // Test 1: RAM backend without size specification
  {
    chimaera::bdev::Client ram_client_no_size(chi::PoolId(8009, 0));

    // This should fail because RAM backend requires explicit size
    std::string pool_name = "ram_fail_test_" + std::to_string(getpid());
    bool creation_success = ram_client_no_size.Create(HSHM_MCTX, chi::PoolQuery::Local(),
                                                      pool_name, chimaera::bdev::BdevType::kRam,
                                                      0);  // Size 0 should fail
    std::this_thread::sleep_for(100ms);
    
    // Creation should fail for RAM backend with zero size
    bool creation_failed = !creation_success;
    
    // If creation didn't fail at the Create level, test allocation to see if the container is invalid
    if (!creation_failed) {
      std::vector<chimaera::bdev::Block> blocks =
          ram_client_no_size.AllocateBlocks(HSHM_MCTX, k4KB);
      creation_failed = (blocks.size() == 0);  // Should be invalid block list
    }

    HILOG(kInfo, "RAM backend properly rejects zero size: {}",
          creation_failed ? "YES" : "NO");
  }

  // Test 2: File backend with non-existent file
  {
    chimaera::bdev::Client file_client_bad_path(chi::PoolId(8010, 0));

    // This should fail because the file path doesn't exist
    bool creation_success = file_client_bad_path.Create(HSHM_MCTX, chi::PoolQuery::Local(),
                                                        "/nonexistent/path/file.dat",
                                                        chimaera::bdev::BdevType::kFile);
    std::this_thread::sleep_for(100ms);

    // Creation should fail for non-existent file path
    bool creation_failed = !creation_success;
    
    // If creation didn't fail at the Create level, test allocation to see if the container is invalid
    if (!creation_failed) {
      std::vector<chimaera::bdev::Block> blocks =
          file_client_bad_path.AllocateBlocks(HSHM_MCTX, k4KB);
      creation_failed = (blocks.size() == 0);
    }

    HILOG(kInfo, "File backend properly handles bad path: {}",
          creation_failed ? "YES" : "NO");
  }
}

//==============================================================================
// PARALLEL OPERATIONS TESTS
//==============================================================================

TEST_CASE("bdev_parallel_io_operations", "[bdev][parallel][io]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(100 * 1024 * 1024));  // 100MB for parallel ops
  }

  SECTION("Parallel allocate/write/free operations") {
    // Test configuration
    const size_t num_threads = 4;
    const size_t ops_per_thread = 100;
    const size_t io_size = 4096;  // 4KB I/O size

    // Create BDev container
    chi::PoolId pool_id(200, 0);
    chimaera::bdev::Client client(pool_id);
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(),
                                 chimaera::bdev::BdevType::kFile);
    REQUIRE(success);
    REQUIRE(client.GetReturnCode() == 0);

    HILOG(kInfo, "Starting parallel I/O operations test:");
    HILOG(kInfo, "  Threads: {}", num_threads);
    HILOG(kInfo, "  Operations per thread: {}", ops_per_thread);
    HILOG(kInfo, "  I/O size: {} bytes", io_size);
    HILOG(kInfo, "  Total operations: {}", num_threads * ops_per_thread);

    // Worker thread function
    auto worker_thread = [&](size_t thread_id) {
      // Create thread-local BDev client
      chimaera::bdev::Client thread_client(pool_id);
      hipc::MemContext thread_mctx;

      // Allocate write buffer in shared memory
      auto write_buffer = CHI_IPC->AllocateBuffer(io_size);
      std::memset(write_buffer.ptr_, static_cast<int>(thread_id), io_size);

      // Perform I/O operations
      for (size_t i = 0; i < ops_per_thread; i++) {
        // Allocate block
        auto blocks = thread_client.AllocateBlocks(thread_mctx, 1);
        REQUIRE(blocks.size() == 1);

        // Write data
        auto write_task = thread_client.AsyncWrite(thread_mctx, blocks[0],
                                                    write_buffer.shm_, io_size);
        write_task->Wait();
        REQUIRE(write_task->return_code_.load() == 0);
        REQUIRE(write_task->bytes_written_ == io_size);

        // Free block
        auto free_task = thread_client.AsyncFreeBlocks(thread_mctx, blocks);
        free_task->Wait();
        REQUIRE(free_task->return_code_.load() == 0);
      }
    };

    // Launch worker threads
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_threads; i++) {
      threads.emplace_back(worker_thread, i);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
      thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Calculate and log statistics
    size_t total_ops = num_threads * ops_per_thread;
    double ops_per_sec = (total_ops * 1000.0) / elapsed.count();
    double bandwidth_mbps = (total_ops * io_size * 1000.0) /
                            (elapsed.count() * 1024 * 1024);

    HILOG(kInfo, "Parallel I/O test completed:");
    HILOG(kInfo, "  Total time: {} ms", elapsed.count());
    HILOG(kInfo, "  IOPS: {:.0f} ops/sec", ops_per_sec);
    HILOG(kInfo, "  Bandwidth: {:.2f} MB/s", bandwidth_mbps);
    HILOG(kInfo, "  Avg latency: {:.3f} us/op",
          (elapsed.count() * 1000.0) / total_ops);

    // Verify all operations completed successfully
    REQUIRE(elapsed.count() > 0);
    REQUIRE(ops_per_sec > 0);
  }
}

//==============================================================================
// TASK_FORCE_NET TESTS
//==============================================================================

/**
 * Test TASK_FORCE_NET flag - forces tasks through network code even for local execution
 */
TEST_CASE("bdev_force_net_flag", "[bdev][network][force_net]") {
  BdevChimodFixture fixture;

  SECTION("Setup") {
    REQUIRE(fixture.initializeBoth());
    REQUIRE(fixture.createTestFile(kDefaultFileSize));
  }

  SECTION("Write and Read with TASK_FORCE_NET") {
    chimaera::bdev::Client client(chi::PoolId(105, 0));
    hipc::MemContext mctx;

    bool success = client.Create(mctx, chi::PoolQuery::Local(), fixture.getTestFile(), chimaera::bdev::BdevType::kFile);
    REQUIRE(success);
    REQUIRE(client.GetReturnCode() == 0);

    HILOG(kInfo, "Created BDev container for TASK_FORCE_NET test");

    // Allocate a block
    auto alloc_task = client.AsyncAllocateBlocks(mctx, k64KB);
    alloc_task->Wait();
    REQUIRE(alloc_task->return_code_ == 0);
    REQUIRE(alloc_task->blocks_.size() > 0);

    chimaera::bdev::Block block = alloc_task->blocks_[0];
    CHI_IPC->DelTask(alloc_task);

    // Prepare test data
    std::vector<hshm::u8> write_data = fixture.generateTestData(k64KB, 0xAB);

    // Create write task with TASK_FORCE_NET flag
    auto write_buffer = CHI_IPC->AllocateBuffer(write_data.size());
    REQUIRE_FALSE(write_buffer.IsNull());
    memcpy(write_buffer.ptr_, write_data.data(), write_data.size());

    // Use NewTask directly to create write task
    auto* ipc_manager = CHI_IPC;
    auto write_task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
        chi::CreateTaskId(), client.pool_id_, chi::PoolQuery::Local(),
        block, write_buffer.shm_, write_data.size());

    // Set TASK_FORCE_NET flag BEFORE enqueueing
    write_task->SetFlags(TASK_FORCE_NET);
    REQUIRE(write_task->task_flags_.Any(TASK_FORCE_NET));
    HILOG(kInfo, "Write task TASK_FORCE_NET flag set");

    // Enqueue the task
    ipc_manager->Enqueue(write_task);

    // Wait for write to complete
    write_task->Wait();
    REQUIRE(write_task->return_code_ == 0);
    REQUIRE(write_task->bytes_written_ == write_data.size());
    CHI_IPC->DelTask(write_task);

    HILOG(kInfo, "Write with TASK_FORCE_NET completed successfully");

    // Create read task with TASK_FORCE_NET flag
    auto read_buffer = CHI_IPC->AllocateBuffer(k64KB);
    REQUIRE_FALSE(read_buffer.IsNull());

    // Use NewTask directly to create read task
    auto read_task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
        chi::CreateTaskId(), client.pool_id_, chi::PoolQuery::Local(),
        block, read_buffer.shm_, k64KB);

    // Set TASK_FORCE_NET flag BEFORE enqueueing
    read_task->SetFlags(TASK_FORCE_NET);
    REQUIRE(read_task->task_flags_.Any(TASK_FORCE_NET));
    HILOG(kInfo, "Read task TASK_FORCE_NET flag set");

    // Enqueue the task
    ipc_manager->Enqueue(read_task);

    // Wait for read to complete
    read_task->Wait();
    REQUIRE(read_task->return_code_ == 0);
    REQUIRE(read_task->bytes_read_ == write_data.size());

    HILOG(kInfo, "Read with TASK_FORCE_NET completed successfully");

    // Verify data integrity
    std::vector<hshm::u8> read_data(read_task->bytes_read_);
    memcpy(read_data.data(), read_buffer.ptr_, read_task->bytes_read_);
    REQUIRE(read_data.size() == write_data.size());

    for (size_t i = 0; i < write_data.size(); ++i) {
      REQUIRE(read_data[i] == write_data[i]);
    }

    CHI_IPC->DelTask(read_task);

    HILOG(kInfo, "TASK_FORCE_NET test completed - data verified successfully");
  }
}

//==============================================================================
// MAIN TEST RUNNER
//==============================================================================

SIMPLE_TEST_MAIN()