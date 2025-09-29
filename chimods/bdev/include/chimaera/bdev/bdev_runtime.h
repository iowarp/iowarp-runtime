#ifndef BDEV_RUNTIME_H_
#define BDEV_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/comutex.h>
#include "bdev_tasks.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <aio.h>
#include <vector>
#include <atomic>
#include <chrono>

/**
 * Runtime container for bdev ChiMod
 * 
 * Provides block device operations with async I/O and data allocation management
 */

namespace chimaera::bdev {


/**
 * Free list node for data allocator
 */
struct FreeListNode {
  chi::u64 offset_;
  chi::u64 size_;
  FreeListNode* next_;
  
  FreeListNode(chi::u64 offset, chi::u64 size) 
    : offset_(offset), size_(size), next_(nullptr) {}
};

/**
 * Block size categories for data allocator
 */
enum class BlockSizeCategory : chi::u32 {
  k4KB = 0,
  k64KB = 1, 
  k256KB = 2,
  k1MB = 3,
  kMaxCategories = 4
};

/**
 * Runtime container for bdev operations
 */
class Runtime : public chi::Container {
 public:
  // Required typedef for CHI_TASK_CC macro
  using CreateParams = chimaera::bdev::CreateParams;
  
  Runtime() : bdev_type_(BdevType::kFile), file_fd_(-1), file_size_(0), alignment_(4096), 
              io_depth_(32), ram_buffer_(nullptr), ram_size_(0), remaining_size_(0), 
              next_offset_(0), total_reads_(0), total_writes_(0), 
              total_bytes_read_(0), total_bytes_written_(0) {
    // Initialize free lists
    for (size_t i = 0; i < static_cast<size_t>(BlockSizeCategory::kMaxCategories); ++i) {
      free_lists_[i] = nullptr;
    }
    start_time_ = std::chrono::high_resolution_clock::now();
  }
  ~Runtime() override;

  /**
   * Create the container (Method::kCreate)
   * This method both creates and initializes the container
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& ctx);

  /**
   * Monitor create progress
   */
  void MonitorCreate(chi::MonitorModeId mode, hipc::FullPtr<CreateTask> task,
                     chi::RunContext& ctx);

  /**
   * Allocate multiple blocks (Method::kAllocateBlocks)
   */
  void AllocateBlocks(hipc::FullPtr<AllocateBlocksTask> task, chi::RunContext& ctx);

  /**
   * Monitor allocate blocks progress
   */
  void MonitorAllocateBlocks(chi::MonitorModeId mode, hipc::FullPtr<AllocateBlocksTask> task,
                             chi::RunContext& ctx);


  /**
   * Free data blocks (Method::kFreeBlocks)
   */
  void FreeBlocks(hipc::FullPtr<FreeBlocksTask> task, chi::RunContext& ctx);

  /**
   * Monitor free blocks progress
   */
  void MonitorFreeBlocks(chi::MonitorModeId mode, hipc::FullPtr<FreeBlocksTask> task,
                         chi::RunContext& ctx);

  /**
   * Write data to a block (Method::kWrite)
   */
  void Write(hipc::FullPtr<WriteTask> task, chi::RunContext& ctx);

  /**
   * Monitor write progress
   */
  void MonitorWrite(chi::MonitorModeId mode, hipc::FullPtr<WriteTask> task,
                    chi::RunContext& ctx);

  /**
   * Read data from a block (Method::kRead)
   */
  void Read(hipc::FullPtr<ReadTask> task, chi::RunContext& ctx);

  /**
   * Monitor read progress
   */
  void MonitorRead(chi::MonitorModeId mode, hipc::FullPtr<ReadTask> task,
                   chi::RunContext& ctx);

  /**
   * Get performance statistics (Method::kGetStats)
   */
  void GetStats(hipc::FullPtr<GetStatsTask> task, chi::RunContext& ctx);

  /**
   * Monitor GetStats progress
   */
  void MonitorGetStats(chi::MonitorModeId mode, hipc::FullPtr<GetStatsTask> task,
                       chi::RunContext& ctx);

  /**
   * Backward compatibility methods for StatTask
   */
  void Stat(hipc::FullPtr<StatTask> task, chi::RunContext& ctx) {
    GetStats(task, ctx);
  }
  
  void MonitorStat(chi::MonitorModeId mode, hipc::FullPtr<StatTask> task,
                   chi::RunContext& ctx) {
    MonitorGetStats(mode, task, ctx);
  }

  /**
   * Destroy the container (Method::kDestroy)
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& ctx);

  /**
   * Monitor destroy progress
   */
  void MonitorDestroy(chi::MonitorModeId mode, hipc::FullPtr<DestroyTask> task,
                      chi::RunContext& ctx);

  /**
   * REQUIRED VIRTUAL METHODS FROM chi::Container
   */
  
  /**
   * Execute a method on a task - using autogen dispatcher
   */
  void Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
           chi::RunContext& rctx) override;

  /**
   * Monitor a method execution for scheduling/coordination - using autogen dispatcher
   */
  void Monitor(chi::MonitorModeId mode, chi::u32 method,
               hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) override;

  /**
   * Delete/cleanup a task - using autogen dispatcher
   */
  void Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Get remaining work count for this container
   */
  chi::u64 GetWorkRemaining() const override;

  /**
   * Serialize task IN parameters for network transfer
   */
  void SaveIn(chi::u32 method, chi::TaskSaveInArchive& archive,
              hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task IN parameters from network transfer
   */
  void LoadIn(chi::u32 method, chi::TaskLoadInArchive& archive,
              hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Serialize task OUT parameters for network transfer
   */
  void SaveOut(chi::u32 method, chi::TaskSaveOutArchive& archive,
               hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task OUT parameters from network transfer
   */
  void LoadOut(chi::u32 method, chi::TaskLoadOutArchive& archive,
               hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Create a new copy of a task (deep copy for distributed execution)
   */
  void NewCopy(chi::u32 method, const hipc::FullPtr<chi::Task>& orig_task,
               hipc::FullPtr<chi::Task>& dup_task, bool deep) override;

 private:
  // Storage backend configuration
  BdevType bdev_type_;                            // Backend type (file or RAM)
  
  // File-based storage (kFile)
  int file_fd_;                                    // File descriptor
  chi::u64 file_size_;                            // Total file size
  chi::u32 alignment_;                            // I/O alignment requirement
  chi::u32 io_depth_;                             // Max concurrent I/O operations
  
  // RAM-based storage (kRam)
  char* ram_buffer_;                              // RAM storage buffer
  chi::u64 ram_size_;                            // Total RAM buffer size
  
  // Data allocator state
  std::atomic<chi::u64> remaining_size_;          // Remaining allocatable space
  chi::u64 next_offset_;                          // Next allocation offset
  chi::CoMutex alloc_mutex_;                      // CoMutex for allocation operations
  
  // Free lists for different block sizes
  FreeListNode* free_lists_[static_cast<size_t>(BlockSizeCategory::kMaxCategories)];
  chi::CoMutex free_list_mutexes_[static_cast<size_t>(BlockSizeCategory::kMaxCategories)];
  
  // Performance tracking
  std::atomic<chi::u64> total_reads_;
  std::atomic<chi::u64> total_writes_;
  std::atomic<chi::u64> total_bytes_read_;
  std::atomic<chi::u64> total_bytes_written_;
  std::chrono::high_resolution_clock::time_point start_time_;
  
  // User-provided performance characteristics
  PerfMetrics perf_metrics_;
  
  /**
   * Initialize the data allocator
   */
  void InitializeAllocator();
  
  /**
   * Initialize POSIX AIO control blocks
   */
  void InitializeAsyncIO();
  
  /**
   * Cleanup POSIX AIO control blocks
   */
  void CleanupAsyncIO();
  
  
  /**
   * Get actual block size for category
   */
  chi::u64 GetBlockSize(BlockSizeCategory category);
  
  
  /**
   * Allocate from free list if available
   */
  bool AllocateFromFreeList(BlockSizeCategory category, chi::u64 size, Block& block);
  
  /**
   * Allocate new block from heap offset
   */
  bool AllocateFromHeap(chi::u64 size, BlockSizeCategory category, Block& block);
  
  /**
   * Add block to appropriate free list
   */
  void AddToFreeList(const Block& block);
  
  /**
   * Perform async I/O operation
   */
  chi::u32 PerformAsyncIO(bool is_write, chi::u64 offset, void* buffer, 
                          chi::u64 size, chi::u64& bytes_transferred,
                          hipc::FullPtr<chi::Task> task);
  
  /**
   * Align size to required boundary
   */
  chi::u64 AlignSize(chi::u64 size);
  
  /**
   * Backend-specific write operations
   */
  void WriteToFile(hipc::FullPtr<WriteTask> task);
  void WriteToRam(hipc::FullPtr<WriteTask> task);
  
  /**
   * Backend-specific read operations
   */
  void ReadFromFile(hipc::FullPtr<ReadTask> task);
  void ReadFromRam(hipc::FullPtr<ReadTask> task);
  
  /**
   * Update performance metrics
   */
  void UpdatePerformanceMetrics(bool is_write, chi::u64 bytes, 
                                double duration_us);
};

} // namespace chimaera::bdev

#endif // BDEV_RUNTIME_H_