# Bdev ChiMod Documentation

## Overview

The Bdev (Block Device) ChiMod provides a high-performance interface for block device operations supporting both file-based and RAM-based storage backends. It manages block allocation, read/write operations, and performance monitoring with flexible storage options.

**Key Features:**
- **Dual Backend Support**: File-based storage (using libaio) and RAM-based storage (using malloc)
- **Asynchronous I/O**: For file-based storage using libaio, synchronous operations for RAM-based storage
- **Hierarchical block allocation** with multiple size categories (4KB, 64KB, 256KB, 1MB)
- **Performance monitoring** and statistics collection for both backends
- **Memory-aligned I/O operations** for optimal file-based performance
- **Block allocation and deallocation management** with unified API

## CMake Integration

### External Projects

To use the Bdev ChiMod in external projects:

```cmake
find_package(chimaera-bdev REQUIRED)
find_package(chimaera-admin REQUIRED)  # Always required
find_package(chimaera-core REQUIRED)

target_link_libraries(your_application
  chimaera::bdev_client         # Bdev client library
  chimaera::admin_client        # Admin client (required)
  chimaera::cxx                 # Main chimaera library
  hshm::cxx                     # HermesShm library
  ${CMAKE_THREAD_LIBS_INIT}     # Threading support
)
```

### Required Headers

```cpp
#include <chimaera/chimaera.h>
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/bdev/bdev_tasks.h>
#include <chimaera/admin/admin_client.h>  // Required for CreateTask
```

## API Reference

### Client Class: `chimaera::bdev::Client`

The Bdev client provides the primary interface for block device operations.

#### Constructor

```cpp
// Default constructor
Client()

// Constructor with pool ID
explicit Client(const chi::PoolId& pool_id)
```

#### Container Management

##### `Create()` - Synchronous (File-based, Backward Compatible)
Creates and initializes the bdev container with file-based storage.

```cpp
void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
           const std::string& file_path, chi::u64 total_size = 0,
           chi::u32 io_depth = 32, chi::u32 alignment = 4096)
```

##### `Create()` - Synchronous (With Backend Type Selection)
Creates and initializes the bdev container with specified backend type.

```cpp
void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
           BdevType bdev_type, const std::string& file_path = "", chi::u64 total_size = 0,
           chi::u32 io_depth = 32, chi::u32 alignment = 4096)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `pool_query`: Pool domain query (typically `chi::PoolQuery::Local()`)
- `bdev_type`: Backend type (`BdevType::kFile` or `BdevType::kRam`)
- `file_path`: Path to the block device file (required for kFile, ignored for kRam)
- `total_size`: Total size available for allocation (0 = use file size for kFile, required for kRam)
- `io_depth`: libaio queue depth for asynchronous operations (ignored for kRam, default: 32)
- `alignment`: I/O alignment in bytes for optimal performance (default: 4096)

**Usage Examples:**

*File-based storage:*
```cpp
chi::CHIMAERA_CLIENT_INIT();
const chi::PoolId pool_id = chi::PoolId(8000, 0);
chimaera::bdev::Client bdev_client(pool_id);

auto pool_query = chi::PoolQuery::Local();
// Traditional file-based storage
bdev_client.Create(HSHM_MCTX, pool_query, BdevType::kFile, "/dev/nvme0n1", 0, 64, 4096);
```

*RAM-based storage:*
```cpp
// RAM-based storage (1GB)
bdev_client.Create(HSHM_MCTX, pool_query, BdevType::kRam, "", 1024*1024*1024);
```

##### `AsyncCreate()` - Asynchronous (File-based, Backward Compatible)
Creates and initializes the bdev container asynchronously with file-based storage.

```cpp
hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
    const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
    const std::string& file_path, chi::u64 total_size = 0,
    chi::u32 io_depth = 32, chi::u32 alignment = 4096)
```

##### `AsyncCreate()` - Asynchronous (With Backend Type Selection)
Creates and initializes the bdev container asynchronously with specified backend type.

```cpp
hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
    const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
    BdevType bdev_type, const std::string& file_path = "", chi::u64 total_size = 0,
    chi::u32 io_depth = 32, chi::u32 alignment = 4096)
```

**Returns:** Task pointer for asynchronous completion checking

#### Block Management Operations

##### `Allocate()` - Synchronous
Allocates a block of the specified size.

```cpp
Block Allocate(const hipc::MemContext& mctx, chi::u64 size)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `size`: Size of the block to allocate in bytes

**Returns:** `Block` structure containing offset, size, and block type information

**Usage:**
```cpp
Block my_block = bdev_client.Allocate(HSHM_MCTX, 65536);  // Allocate 64KB
std::cout << "Allocated block at offset " << my_block.offset_ 
          << " with size " << my_block.size_ << std::endl;
```

##### `AsyncAllocate()` - Asynchronous
```cpp
hipc::FullPtr<chimaera::bdev::AllocateTask> AsyncAllocate(
    const hipc::MemContext& mctx, chi::u64 size)
```

##### `Free()` - Synchronous
Frees a previously allocated block.

```cpp
chi::u32 Free(const hipc::MemContext& mctx, const Block& block)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `block`: Block structure to free

**Returns:** Result code (0 = success, non-zero = error)

##### `AsyncFree()` - Asynchronous
```cpp
hipc::FullPtr<chimaera::bdev::FreeTask> AsyncFree(
    const hipc::MemContext& mctx, const Block& block)
```

#### I/O Operations

##### `Write()` - Synchronous
Writes data to a previously allocated block.

```cpp
chi::u64 Write(const hipc::MemContext& mctx, const Block& block,
              const std::vector<hshm::u8>& data)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `block`: Target block for writing
- `data`: Data to write as a byte vector

**Returns:** Number of bytes actually written

**Usage:**
```cpp
// Prepare data
std::vector<hshm::u8> write_data(4096, 0xAB);  // 4KB of 0xAB pattern

// Write to block
chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, my_block, write_data);
std::cout << "Wrote " << bytes_written << " bytes" << std::endl;
```

##### `AsyncWrite()` - Asynchronous
```cpp
hipc::FullPtr<chimaera::bdev::WriteTask> AsyncWrite(
    const hipc::MemContext& mctx, const Block& block,
    const std::vector<hshm::u8>& data)
```

##### `Read()` - Synchronous
Reads data from a previously allocated and written block.

```cpp
std::vector<hshm::u8> Read(const hipc::MemContext& mctx, const Block& block)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `block`: Source block for reading

**Returns:** Vector containing the read data

**Usage:**
```cpp
// Read data back
std::vector<hshm::u8> read_data = bdev_client.Read(HSHM_MCTX, my_block);
std::cout << "Read " << read_data.size() << " bytes" << std::endl;

// Verify data integrity
bool data_matches = std::equal(write_data.begin(), write_data.end(), read_data.begin());
std::cout << "Data integrity check: " << (data_matches ? "PASS" : "FAIL") << std::endl;
```

##### `AsyncRead()` - Asynchronous
```cpp
hipc::FullPtr<chimaera::bdev::ReadTask> AsyncRead(
    const hipc::MemContext& mctx, const Block& block)
```

#### Performance Monitoring

##### `GetStats()` - Synchronous
Retrieves performance statistics and remaining storage space.

```cpp
PerfMetrics GetStats(const hipc::MemContext& mctx, chi::u64& remaining_size)
```

**Parameters:**
- `mctx`: Memory context for task allocation
- `remaining_size`: Output parameter for remaining allocatable space

**Returns:** `PerfMetrics` structure with performance data

**Usage:**
```cpp
chi::u64 remaining_space;
PerfMetrics metrics = bdev_client.GetStats(HSHM_MCTX, remaining_space);

std::cout << "Performance Statistics:" << std::endl;
std::cout << "  Read bandwidth: " << metrics.read_bandwidth_mbps_ << " MB/s" << std::endl;
std::cout << "  Write bandwidth: " << metrics.write_bandwidth_mbps_ << " MB/s" << std::endl;
std::cout << "  Read latency: " << metrics.read_latency_us_ << " μs" << std::endl;
std::cout << "  Write latency: " << metrics.write_latency_us_ << " μs" << std::endl;
std::cout << "  IOPS: " << metrics.iops_ << std::endl;
std::cout << "  Remaining space: " << remaining_space << " bytes" << std::endl;
```

##### `AsyncGetStats()` - Asynchronous
```cpp
hipc::FullPtr<chimaera::bdev::StatTask> AsyncGetStats(
    const hipc::MemContext& mctx)
```

## Data Structures

### BdevType Enum
Specifies the storage backend type.

```cpp
enum class BdevType : chi::u32 {
  kFile = 0,  // File-based block device (default)
  kRam = 1    // RAM-based block device
};
```

**Backend Characteristics:**
- **kFile**: Uses file-based storage with libaio for asynchronous I/O, supports alignment requirements, persistent data
- **kRam**: Uses malloc-allocated RAM buffer, synchronous operations, volatile data (lost on restart)

### Block Structure
Represents an allocated block of storage.

```cpp
struct Block {
  chi::u64 offset_;     // Offset within file/device
  chi::u64 size_;       // Size of block in bytes
  chi::u32 block_type_; // Block size category (0=4KB, 1=64KB, 2=256KB, 3=1MB)
}
```

**Block Type Categories:**
- `0`: 4KB blocks - for small, frequent I/O operations
- `1`: 64KB blocks - for medium-sized operations
- `2`: 256KB blocks - for large sequential operations  
- `3`: 1MB blocks - for very large bulk operations

### PerfMetrics Structure
Contains performance monitoring data.

```cpp
struct PerfMetrics {
  double read_bandwidth_mbps_;   // Read bandwidth in MB/s
  double write_bandwidth_mbps_;  // Write bandwidth in MB/s
  double read_latency_us_;       // Average read latency in microseconds
  double write_latency_us_;      // Average write latency in microseconds
  double iops_;                  // I/O operations per second
}
```

## Task Types

### CreateTask
Container creation task for the bdev module. This is an alias for `chimaera::admin::GetOrCreatePoolTask<CreateParams>`.

**Key Fields:**
- Inherits from `BaseCreateTask` with bdev-specific `CreateParams`
- Processed by admin module for pool creation
- Contains serialized bdev configuration parameters

### AllocateTask
Block allocation task.

**Key Fields:**
- `size_`: Requested block size in bytes
- `block_`: Allocated block information (output)
- `result_code_`: Operation result (0 = success)

### FreeTask
Block deallocation task.

**Key Fields:**
- `block_`: Block to free
- `result_code_`: Operation result (0 = success)

### WriteTask
Block write operation task.

**Key Fields:**
- `block_`: Target block for writing
- `data_`: Data to write (INOUT - input for write, output for verification)
- `result_code_`: Operation result (0 = success)
- `bytes_written_`: Number of bytes actually written

### ReadTask
Block read operation task.

**Key Fields:**
- `block_`: Source block for reading
- `data_`: Read data (output)
- `result_code_`: Operation result (0 = success)
- `bytes_read_`: Number of bytes actually read

### StatTask
Performance statistics retrieval task.

**Key Fields:**
- `metrics_`: Performance metrics (output)
- `remaining_size_`: Remaining allocatable space (output)
- `result_code_`: Operation result (0 = success)

## Configuration

### CreateParams Structure
Configuration parameters for bdev container creation:

```cpp
struct CreateParams {
  BdevType bdev_type_;         // Block device type (file or RAM)
  std::string file_path_;      // Path to block device file (for kFile type)
  chi::u64 total_size_;        // Total size for allocation (0 = file size for kFile, required for kRam)
  chi::u32 io_depth_;          // libaio queue depth (ignored for kRam, default: 32)
  chi::u32 alignment_;         // I/O alignment in bytes (default: 4096)
  
  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_bdev";
}
```

**Parameter Guidelines:**
- **bdev_type_**: Choose `BdevType::kFile` for persistent storage or `BdevType::kRam` for high-speed volatile storage
- **file_path_**: Required for kFile (can be block device `/dev/nvme0n1` or regular file), ignored for kRam
- **total_size_**: 
  - For kFile: Set to 0 to use full file/device size, or specify limit
  - For kRam: **Required** - specifies the RAM buffer size to allocate
- **io_depth_**: Higher values improve parallelism for kFile but use more memory (typical: 16-128), ignored for kRam
- **alignment_**: Must match device requirements for kFile (typically 512 or 4096 bytes), less critical for kRam

**Important:** The `chimod_lib_name` does NOT include the `_runtime` suffix as it is automatically appended by the module manager.

## Usage Examples

### File-based Block Device Workflow
```cpp
#include <chimaera/chimaera.h>
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/admin/admin_client.h>

int main() {
  try {
    // Initialize Chimaera client
    chi::CHIMAERA_CLIENT_INIT();
    
    // Create admin client first (always required)
    const chi::PoolId admin_pool_id = chi::PoolId(7000, 0);
    chimaera::admin::Client admin_client(admin_pool_id);
    admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local());
    
    // Create bdev client
    const chi::PoolId bdev_pool_id = chi::PoolId(8000, 0);
    chimaera::bdev::Client bdev_client(bdev_pool_id);
    
    // Initialize bdev container with NVMe device (file-based)
    bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), 
                      BdevType::kFile, "/dev/nvme0n1", 0, 64, 4096);
    
    // Allocate a 1MB block
    Block large_block = bdev_client.Allocate(HSHM_MCTX, 1024 * 1024);
    
    // Prepare test data
    std::vector<hshm::u8> test_data(large_block.size_, 0xDE);
    for (size_t i = 0; i < test_data.size(); i += 4096) {
      // Add pattern to verify data integrity
      test_data[i] = static_cast<hshm::u8>(i % 256);
    }
    
    // Write data
    chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, large_block, test_data);
    std::cout << "Wrote " << bytes_written << " bytes to block" << std::endl;
    
    // Read data back
    std::vector<hshm::u8> read_data = bdev_client.Read(HSHM_MCTX, large_block);
    
    // Verify data integrity
    bool integrity_ok = (read_data.size() == test_data.size()) &&
                       std::equal(test_data.begin(), test_data.end(), read_data.begin());
    std::cout << "Data integrity: " << (integrity_ok ? "PASS" : "FAIL") << std::endl;
    
    // Get performance statistics
    chi::u64 remaining_space;
    PerfMetrics perf = bdev_client.GetStats(HSHM_MCTX, remaining_space);
    
    std::cout << "\nPerformance Summary:" << std::endl;
    std::cout << "  Read: " << perf.read_bandwidth_mbps_ << " MB/s" << std::endl;
    std::cout << "  Write: " << perf.write_bandwidth_mbps_ << " MB/s" << std::endl;
    std::cout << "  IOPS: " << perf.iops_ << std::endl;
    
    // Free the allocated block
    chi::u32 free_result = bdev_client.Free(HSHM_MCTX, large_block);
    std::cout << "Block freed: " << (free_result == 0 ? "SUCCESS" : "FAILED") << std::endl;
    
    return 0;
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
```

### RAM-based Block Device Workflow
```cpp
#include <chimaera/chimaera.h>
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/admin/admin_client.h>

int main() {
  try {
    // Initialize Chimaera client
    chi::CHIMAERA_CLIENT_INIT();
    
    // Create admin client first (always required)
    const chi::PoolId admin_pool_id = chi::PoolId(7000, 0);
    chimaera::admin::Client admin_client(admin_pool_id);
    admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local());
    
    // Create bdev client
    const chi::PoolId bdev_pool_id = chi::PoolId(8000, 0);
    chimaera::bdev::Client bdev_client(bdev_pool_id);
    
    // Initialize bdev container with RAM backend (1GB)
    bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), 
                      BdevType::kRam, "", 1024*1024*1024);
    
    // Allocate a 1MB block (from RAM)
    Block large_block = bdev_client.Allocate(HSHM_MCTX, 1024 * 1024);
    
    // Prepare test data
    std::vector<hshm::u8> test_data(large_block.size_, 0xAB);
    
    // Write data to RAM (very fast)
    auto start = std::chrono::high_resolution_clock::now();
    chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, large_block, test_data);
    auto write_end = std::chrono::high_resolution_clock::now();
    
    // Read data from RAM (very fast)
    std::vector<hshm::u8> read_data = bdev_client.Read(HSHM_MCTX, large_block);
    auto read_end = std::chrono::high_resolution_clock::now();
    
    // Calculate performance
    double write_time_ms = std::chrono::duration<double, std::milli>(write_end - start).count();
    double read_time_ms = std::chrono::duration<double, std::milli>(read_end - write_end).count();
    
    std::cout << "RAM Backend Performance:" << std::endl;
    std::cout << "  Write time: " << write_time_ms << " ms" << std::endl;
    std::cout << "  Read time: " << read_time_ms << " ms" << std::endl;
    std::cout << "  Write bandwidth: " << (bytes_written / 1024.0 / 1024.0) / (write_time_ms / 1000.0) << " MB/s" << std::endl;
    
    // Verify data integrity
    bool integrity_ok = (read_data.size() == test_data.size()) &&
                       std::equal(test_data.begin(), test_data.end(), read_data.begin());
    std::cout << "Data integrity: " << (integrity_ok ? "PASS" : "FAIL") << std::endl;
    
    // Free the allocated block
    chi::u32 free_result = bdev_client.Free(HSHM_MCTX, large_block);
    std::cout << "Block freed: " << (free_result == 0 ? "SUCCESS" : "FAILED") << std::endl;
    
    return 0;
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
```

### Asynchronous Operations
```cpp
// Example of asynchronous block allocation and I/O
auto alloc_task = bdev_client.AsyncAllocate(HSHM_MCTX, 65536);  // 64KB
alloc_task->Wait();

if (alloc_task->result_code_ == 0) {
  Block block = alloc_task->block_;
  CHI_IPC->DelTask(alloc_task);
  
  // Async write
  std::vector<hshm::u8> data(65536, 0xFF);
  auto write_task = bdev_client.AsyncWrite(HSHM_MCTX, block, data);
  write_task->Wait();
  
  std::cout << "Async write completed, bytes written: " 
            << write_task->bytes_written_ << std::endl;
  CHI_IPC->DelTask(write_task);
  
  // Async read
  auto read_task = bdev_client.AsyncRead(HSHM_MCTX, block);
  read_task->Wait();
  
  std::cout << "Async read completed, bytes read: " 
            << read_task->bytes_read_ << std::endl;
  CHI_IPC->DelTask(read_task);
  
  // Free block
  bdev_client.Free(HSHM_MCTX, block);
}
```

### Performance Benchmarking
```cpp
// Benchmark different block sizes
const std::vector<chi::u64> block_sizes = {4096, 65536, 262144, 1048576};
const size_t num_operations = 1000;

for (chi::u64 block_size : block_sizes) {
  auto start_time = std::chrono::high_resolution_clock::now();
  
  for (size_t i = 0; i < num_operations; ++i) {
    Block block = bdev_client.Allocate(HSHM_MCTX, block_size);
    
    std::vector<hshm::u8> data(block_size, static_cast<hshm::u8>(i % 256));
    bdev_client.Write(HSHM_MCTX, block, data);
    
    std::vector<hshm::u8> read_data = bdev_client.Read(HSHM_MCTX, block);
    
    bdev_client.Free(HSHM_MCTX, block);
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end_time - start_time);
  
  double throughput_mbps = (block_size * num_operations) / 
                          (duration.count() * 1024.0);
  
  std::cout << "Block size " << block_size << " bytes: " 
            << throughput_mbps << " MB/s" << std::endl;
}
```

## Dependencies

- **HermesShm**: Shared memory framework and IPC
- **Chimaera core runtime**: Base runtime objects and task framework
- **Admin ChiMod**: Required for pool creation and management
- **cereal**: Serialization library for network communication
- **libaio**: Linux asynchronous I/O library for high-performance block operations
- **Boost.Fiber** and **Boost.Context**: Coroutine support

## Installation

1. Ensure libaio is installed on your system:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libaio-dev
   
   # RHEL/CentOS
   sudo yum install libaio-devel
   ```

2. Build Chimaera with the bdev module:
   ```bash
   cmake --preset debug
   cmake --build build
   ```

3. Install to system or custom prefix:
   ```bash
   cmake --install build --prefix /usr/local
   ```

4. For external projects, set CMAKE_PREFIX_PATH:
   ```bash
   export CMAKE_PREFIX_PATH="/usr/local:/path/to/hermes-shm:/path/to/other/deps"
   ```

## Error Handling

All synchronous methods may encounter errors during block device operations. Check result codes and handle exceptions appropriately:

```cpp
try {
  Block block = bdev_client.Allocate(HSHM_MCTX, 1024 * 1024);
  // Use block...
} catch (const std::runtime_error& e) {
  std::cerr << "Block allocation failed: " << e.what() << std::endl;
}

// For asynchronous operations, check result_code_
auto task = bdev_client.AsyncAllocate(HSHM_MCTX, 65536);
task->Wait();

if (task->result_code_ != 0) {
  std::cerr << "Async allocation failed with code: " << task->result_code_ << std::endl;
}

CHI_IPC->DelTask(task);
```

**Common Error Scenarios:**
- Insufficient storage space for allocation
- I/O alignment violations
- Device access permissions
- Corrupted block metadata
- Network failures in distributed setups

## Performance Considerations

### Backend Selection

**Use RAM Backend (`BdevType::kRam`) when:**
- Maximum performance is critical
- Data persistence is not required
- Working with temporary data or caching
- Testing and benchmarking scenarios
- Sufficient system RAM is available

**Use File Backend (`BdevType::kFile`) when:**
- Data persistence is required
- Working with datasets larger than available RAM
- Integration with existing storage infrastructure
- Need for data durability across restarts

### Performance Tuning

1. **Block Size Selection**: Choose appropriate block sizes based on I/O patterns
   - Small blocks (4KB): Random access patterns
   - Large blocks (1MB): Sequential operations

2. **I/O Depth** (File backend only): Higher io_depth values improve parallelism but consume more memory

3. **Alignment** (File backend): Ensure data is properly aligned to device boundaries (typically 4096 bytes)

4. **Async Operations**: Use async methods for better parallelism in I/O-intensive applications

5. **Batch Operations**: Group multiple allocations/deallocations when possible to reduce overhead

### Expected Performance Characteristics

**RAM Backend:**
- **Latency**: ~1-10 microseconds for read/write operations
- **Bandwidth**: Limited by memory bandwidth (~10-50 GB/s typical)
- **IOPS**: Very high (>1M IOPS)
- **Scalability**: Excellent for concurrent access

**File Backend:**
- **Latency**: Device-dependent (~10-100 microseconds for NVMe)
- **Bandwidth**: Device-dependent (~1-7 GB/s for high-end NVMe)
- **IOPS**: Device-dependent (~100K-1M IOPS for NVMe)
- **Scalability**: Good with proper io_depth tuning

## Important Notes

1. **Admin Dependency**: The bdev module requires the admin module to be initialized first for pool creation.

2. **Block Lifecycle**: Always free allocated blocks to prevent memory leaks and fragmentation.

3. **Thread Safety**: Operations are designed for single-threaded access. Use external synchronization for multi-threaded environments.

4. **Device Permissions**: Ensure the application has appropriate permissions to access block devices.

5. **Data Persistence**: Data written to blocks persists across container restarts if backed by persistent storage.