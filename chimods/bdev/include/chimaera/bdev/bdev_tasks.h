#ifndef BDEV_TASKS_H_
#define BDEV_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/bdev_methods.h"
// Include admin tasks for BaseCreateTask
#include <chimaera/admin/admin_tasks.h>

/**
 * Task struct definitions for bdev
 * 
 * Defines tasks for block device operations with libaio and data allocation
 */

namespace chimaera::bdev {

/**
 * Block device type enumeration
 */
enum class BdevType : chi::u32 {
  kFile = 0,  // File-based block device (default)
  kRam = 1    // RAM-based block device
};

/**
 * Block structure for data allocation
 */
struct Block {
  chi::u64 offset_;     // Offset within file
  chi::u64 size_;       // Size of block
  chi::u32 block_type_; // Block size category (0=4KB, 1=64KB, 2=256KB, 3=1MB)
  
  Block() : offset_(0), size_(0), block_type_(0) {}
  Block(chi::u64 offset, chi::u64 size, chi::u32 block_type)
    : offset_(offset), size_(size), block_type_(block_type) {}
    
  // Cereal serialization
  template<class Archive>
  void serialize(Archive& ar) {
    ar(offset_, size_, block_type_);
  }
};

/**
 * BlockList structure for multi-block allocation
 */
struct BlockList {
  hipc::vector<Block> blocks_;    // List of allocated blocks
  chi::u64 total_size_;          // Total size covered by all blocks
  
  // Default constructor
  BlockList() : total_size_(0) {}
  
  // Constructor with allocator
  explicit BlockList(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
    : blocks_(alloc), total_size_(0) {}
  
  // Copy constructor with allocator
  BlockList(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, const BlockList& other)
    : blocks_(alloc), total_size_(other.total_size_) {
    blocks_.resize(other.blocks_.size());
    for (size_t i = 0; i < other.blocks_.size(); ++i) {
      blocks_[i] = other.blocks_[i];
    }
  }
  
  // Add a block to the list
  void AddBlock(const Block& block) {
    size_t old_size = blocks_.size();
    blocks_.resize(old_size + 1);
    blocks_[old_size] = block;
    total_size_ += block.size_;
  }
  
  // Get number of blocks
  size_t GetBlockCount() const {
    return blocks_.size();
  }
  
  // Get total size covered
  chi::u64 GetTotalSize() const {
    return total_size_;
  }
  
  // Clear all blocks
  void Clear() {
    blocks_.clear();
    total_size_ = 0;
  }
    
  // Cereal serialization
  template<class Archive>
  void serialize(Archive& ar) {
    ar(blocks_, total_size_);
  }
};

/**
 * Performance metrics structure
 */
struct PerfMetrics {
  double read_bandwidth_mbps_;   // Read bandwidth in MB/s
  double write_bandwidth_mbps_;  // Write bandwidth in MB/s
  double read_latency_us_;       // Average read latency in microseconds
  double write_latency_us_;      // Average write latency in microseconds
  double iops_;                  // I/O operations per second
  
  PerfMetrics() : read_bandwidth_mbps_(0.0), write_bandwidth_mbps_(0.0),
                  read_latency_us_(0.0), write_latency_us_(0.0), iops_(0.0) {}
                  
  // Cereal serialization
  template<class Archive>
  void serialize(Archive& ar) {
    ar(read_bandwidth_mbps_, write_bandwidth_mbps_, read_latency_us_, 
       write_latency_us_, iops_);
  }
};

/**
 * CreateParams for bdev chimod
 * Contains configuration parameters for bdev container creation
 */
struct CreateParams {
  // bdev-specific parameters
  BdevType bdev_type_;             // Block device type (file or RAM)
  std::string file_path_;          // Path to block device file (for kFile type)
  chi::u64 total_size_;            // Total size for allocation (0 = file size for kFile, required for kRam)
  chi::u32 io_depth_;              // libaio queue depth (ignored for kRam)
  chi::u32 alignment_;             // I/O alignment (default 4096)
  
  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_bdev";
  
  // Default constructor (defaults to file-based)
  CreateParams() : bdev_type_(BdevType::kFile), total_size_(0), io_depth_(32), alignment_(4096) {}
  
  // Constructor with allocator (required for admin task system)
  explicit CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc)
      : bdev_type_(BdevType::kFile), total_size_(0), io_depth_(32), alignment_(4096) {}
  
  // Copy constructor with allocator (for template system)
  CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc, const CreateParams& other)
      : bdev_type_(other.bdev_type_), file_path_(other.file_path_), total_size_(other.total_size_), 
        io_depth_(other.io_depth_), alignment_(other.alignment_) {}
  
  // Constructor with allocator and parameters
  CreateParams(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
               BdevType bdev_type,
               const std::string& file_path,
               chi::u64 total_size = 0,
               chi::u32 io_depth = 32,
               chi::u32 alignment = 4096)
      : bdev_type_(bdev_type), file_path_(file_path), total_size_(total_size), 
        io_depth_(io_depth), alignment_(alignment) {
    // Debug: Log what parameters were received
    HELOG(kError, "DEBUG: CreateParams constructor called with: bdev_type={}, file_path='{}', total_size={}, io_depth={}, alignment={}", 
          static_cast<chi::u32>(bdev_type_), file_path_, total_size_, io_depth_, alignment_);
  }
  
  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    ar(bdev_type_, file_path_, total_size_, io_depth_, alignment_);
  }
};

/**
 * CreateTask - Initialize the bdev container
 * Type alias for GetOrCreatePoolTask with CreateParams (uses kGetOrCreatePool method)
 * Non-admin modules should use GetOrCreatePoolTask instead of BaseCreateTask
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * AllocateTask - Allocate multiple blocks with specified total size
 */
struct AllocateTask : public chi::Task {
  // Task-specific data
  IN chi::u64 size_;               // Requested total size
  OUT BlockList block_list_;       // Allocated blocks information
  OUT chi::u32 result_code_;       // 0 = success, non-zero = error

  /** SHM default constructor */
  explicit AllocateTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), size_(0), block_list_(alloc), result_code_(0) {}

  /** Emplace constructor */
  explicit AllocateTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      chi::u64 size)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        size_(size), block_list_(alloc), result_code_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kAllocate;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /** Serialize IN and INOUT parameters */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(size_);
  }
  
  /** Serialize OUT and INOUT parameters */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(block_list_, result_code_);
  }
};

/**
 * FreeTask - Free allocated blocks
 */
struct FreeTask : public chi::Task {
  // Task-specific data
  IN BlockList block_list_;    // Blocks to free
  OUT chi::u32 result_code_;   // 0 = success, non-zero = error

  /** SHM default constructor */
  explicit FreeTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), block_list_(alloc), result_code_(0) {}

  /** Emplace constructor for single block (backward compatibility) */
  explicit FreeTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      const Block& block)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        block_list_(alloc), result_code_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kFree;
    task_flags_.Clear();
    pool_query_ = pool_query;
    // Add single block to list
    block_list_.AddBlock(block);
  }

  /** Emplace constructor for multiple blocks */
  explicit FreeTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      const BlockList& block_list)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        block_list_(alloc, block_list), result_code_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kFree;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /** Serialize IN and INOUT parameters */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(block_list_);
  }
  
  /** Serialize OUT and INOUT parameters */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(result_code_);
  }
};

/**
 * WriteTask - Write data to a block using libaio
 */
struct WriteTask : public chi::Task {
  // Task-specific data
  IN Block block_;                  // Block to write to
  INOUT hipc::vector<hshm::u8> data_; // Data to write (input) / written data (output)
  OUT chi::u32 result_code_;        // 0 = success, non-zero = error
  OUT chi::u64 bytes_written_;      // Number of bytes actually written

  /** SHM default constructor */
  explicit WriteTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), data_(alloc), result_code_(0), bytes_written_(0) {}

  /** Emplace constructor */
  explicit WriteTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      const Block& block,
      const std::vector<hshm::u8>& data)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        block_(block), data_(alloc), 
        result_code_(0), bytes_written_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kWrite;
    task_flags_.Clear();
    pool_query_ = pool_query;
    
    // Copy data from std::vector to hipc::vector
    data_.resize(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
      data_[i] = data[i];
    }
  }

  /** Serialize IN and INOUT parameters */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(block_, data_);
  }
  
  /** Serialize OUT and INOUT parameters */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(data_, result_code_, bytes_written_);
  }
};

/**
 * ReadTask - Read data from a block using libaio
 */
struct ReadTask : public chi::Task {
  // Task-specific data
  IN Block block_;                  // Block to read from
  OUT hipc::vector<hshm::u8> data_;  // Read data
  OUT chi::u32 result_code_;        // 0 = success, non-zero = error
  OUT chi::u64 bytes_read_;         // Number of bytes actually read

  /** SHM default constructor */
  explicit ReadTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), data_(alloc), result_code_(0), bytes_read_(0) {}

  /** Emplace constructor */
  explicit ReadTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query,
      const Block& block)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        block_(block), data_(alloc), result_code_(0), bytes_read_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kRead;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /** Serialize IN and INOUT parameters */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    ar(block_);
  }
  
  /** Serialize OUT and INOUT parameters */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(data_, result_code_, bytes_read_);
  }
};

/**
 * StatTask - Get performance statistics and remaining size
 */
struct StatTask : public chi::Task {
  // Task-specific data (no inputs)
  OUT PerfMetrics metrics_;         // Performance metrics
  OUT chi::u64 remaining_size_;     // Remaining allocatable space
  OUT chi::u32 result_code_;        // 0 = success, non-zero = error

  /** SHM default constructor */
  explicit StatTask(const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc) 
      : chi::Task(alloc), remaining_size_(0), result_code_(0) {}

  /** Emplace constructor */
  explicit StatTask(
      const hipc::CtxAllocator<CHI_MAIN_ALLOC_T> &alloc,
      const chi::TaskNode &task_node,
      const chi::PoolId &pool_id, 
      const chi::PoolQuery &pool_query)
      : chi::Task(alloc, task_node, pool_id, pool_query, 10),
        remaining_size_(0), result_code_(0) {
    // Initialize task
    task_node_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kStat;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /** Serialize IN and INOUT parameters */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    // No input parameters
  }
  
  /** Serialize OUT and INOUT parameters */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    ar(metrics_, remaining_size_, result_code_);
  }
};

/**
 * Standard DestroyTask for bdev
 * All ChiMods should use the same DestroyTask structure from admin
 */
using DestroyTask = chimaera::admin::DestroyTask;

} // namespace chimaera::bdev

#endif // BDEV_TASKS_H_