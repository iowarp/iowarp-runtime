#ifndef BDEV_CLIENT_H_
#define BDEV_CLIENT_H_

#include <chimaera/chimaera.h>
#include <unistd.h>

#include <chrono>

#include "bdev_tasks.h"

/**
 * Client API for bdev ChiMod
 *
 * Provides simple interface for block device operations with async I/O
 */

namespace chimaera::bdev {


class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create bdev container - synchronous
   * For file-based bdev, pool_name is the file path; for RAM, pool_name is a
   * unique identifier
   * @return true if creation succeeded, false if it failed
   */
  bool Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
              const std::string& pool_name, BdevType bdev_type,
              chi::u64 total_size = 0, chi::u32 io_depth = 32,
              chi::u32 alignment = 4096) {
    auto task = AsyncCreate(mctx, pool_query, pool_name, bdev_type, total_size,
                            io_depth, alignment);
    task->Wait();

    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;

    // Store the return code from the Create task in the client
    return_code_ = task->return_code_;

    CHI_IPC->DelTask(task);
    
    // Return true for success (return_code_ == 0), false for failure
    return return_code_ == 0;
  }

  /**
   * Create bdev container - asynchronous
   * For file-based bdev, pool_name is the file path; for RAM, pool_name is a
   * unique identifier
   */
  hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      const std::string& pool_name, BdevType bdev_type, chi::u64 total_size = 0,
      chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    auto* ipc_manager = CHI_IPC;

    // CreateTask should always use admin pool, never the client's pool_id_
    // Pass all arguments directly to NewTask constructor including CreateParams
    // arguments
    chi::u32 safe_alignment =
        (alignment == 0) ? 4096 : alignment;  // Ensure non-zero alignment

    auto task = ipc_manager->NewTask<chimaera::bdev::CreateTask>(
        chi::CreateTaskNode(),
        chi::kAdminPoolId,  // Send to admin pool for GetOrCreatePool processing
        pool_query,
        CreateParams::chimod_lib_name,  // chimod name from CreateParams
        pool_name,  // user-provided pool name (file path for files, unique name
                    // for RAM)
        pool_id_,   // target pool ID to create
        // CreateParams arguments:
        bdev_type, total_size, io_depth, safe_alignment);

    // Submit to runtime
    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Allocate multiple blocks - synchronous
   */
  std::vector<Block> AllocateBlocks(const hipc::MemContext& mctx, chi::u64 size) {
    auto task = AsyncAllocateBlocks(mctx, size);
    task->Wait();
    std::vector<Block> result;
    for (size_t i = 0; i < task->blocks_.size(); ++i) {
      result.push_back(task->blocks_[i]);
    }
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Allocate data blocks - asynchronous
   */
  hipc::FullPtr<AllocateBlocksTask> AsyncAllocateBlocks(
      const hipc::MemContext& mctx, chi::u64 size) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<AllocateBlocksTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), size);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Free multiple blocks - synchronous
   */
  chi::u32 FreeBlocks(const hipc::MemContext& mctx,
                      const std::vector<Block>& blocks) {
    auto task = AsyncFreeBlocks(mctx, blocks);
    task->Wait();
    chi::u32 result = task->return_code_;
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Free multiple blocks - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::FreeBlocksTask> AsyncFreeBlocks(
      const hipc::MemContext& mctx, const std::vector<Block>& blocks) {
    auto* ipc_manager = CHI_IPC;

    // Create task with std::vector constructor (constructor parameter uses std::vector)
    auto task = ipc_manager->NewTask<chimaera::bdev::FreeBlocksTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), blocks);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Write data to block - synchronous
   */
  chi::u64 Write(const hipc::MemContext& mctx, const Block& block,
                 hipc::Pointer data, size_t length) {
    auto task = AsyncWrite(mctx, block, data, length);
    task->Wait();
    chi::u64 bytes_written = task->bytes_written_;
    CHI_IPC->DelTask(task);
    return bytes_written;
  }

  /**
   * Write data to block - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::WriteTask> AsyncWrite(
      const hipc::MemContext& mctx, const Block& block, hipc::Pointer data,
      size_t length) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), block, data,
        length);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Read data from block - synchronous
   * Allocates buffer and returns pointer and size via output parameters
   */
  chi::u64 Read(const hipc::MemContext& mctx, const Block& block,
                hipc::Pointer& data_out, size_t buffer_size) {
    auto task = AsyncRead(mctx, block, data_out, buffer_size);
    task->Wait();
    chi::u64 bytes_read = task->bytes_read_;
    CHI_IPC->DelTask(task);
    return bytes_read;
  }

  /**
   * Read data from block - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::ReadTask> AsyncRead(
      const hipc::MemContext& mctx, const Block& block, hipc::Pointer data,
      size_t buffer_size) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), block, data,
        buffer_size);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Get performance statistics - synchronous
   */
  PerfMetrics GetStats(const hipc::MemContext& mctx, chi::u64& remaining_size) {
    auto task = AsyncGetStats(mctx);
    task->Wait();
    PerfMetrics metrics = task->metrics_;
    remaining_size = task->remaining_size_;
    CHI_IPC->DelTask(task);
    return metrics;
  }

  /**
   * Get performance statistics - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::GetStatsTask> AsyncGetStats(
      const hipc::MemContext& mctx) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::GetStatsTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery());

    ipc_manager->Enqueue(task);
    return task;
  }

 private:
  /**
   * Generate a unique pool name with a given prefix
   * Uses timestamp and process ID to ensure uniqueness
   */
  static std::string GeneratePoolName(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         now.time_since_epoch())
                         .count();
    pid_t pid = getpid();
    return prefix + "_" + std::to_string(timestamp) + "_" + std::to_string(pid);
  }
};

}  // namespace chimaera::bdev

#endif  // BDEV_CLIENT_H_