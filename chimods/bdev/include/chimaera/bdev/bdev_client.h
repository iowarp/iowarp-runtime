#ifndef BDEV_CLIENT_H_
#define BDEV_CLIENT_H_

#include <chimaera/chimaera.h>

#include "bdev_tasks.h"

/**
 * Client API for bdev ChiMod
 *
 * Provides simple interface for block device operations with async I/O
 */

namespace chimaera::bdev {

/**
 * Client-side block list (uses std::vector instead of hipc::vector)
 */
struct ClientBlockList {
  std::vector<Block> blocks_;    // List of allocated blocks
  chi::u64 total_size_;          // Total size covered by all blocks
  
  ClientBlockList() : total_size_(0) {}
  
  // Add a block to the list
  void AddBlock(const Block& block) {
    blocks_.push_back(block);
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
};

class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create bdev container - synchronous (file-based, for backward compatibility)
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
              const std::string& file_path, chi::u64 total_size = 0,
              chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    auto task = AsyncCreate(mctx, pool_query, BdevType::kFile, file_path, total_size, io_depth,
                            alignment);
    task->Wait();
    
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;
    
    CHI_IPC->DelTask(task);
  }
  
  /**
   * Create bdev container - synchronous (with explicit bdev type)
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
              BdevType bdev_type, const std::string& file_path = "", chi::u64 total_size = 0,
              chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    auto task = AsyncCreate(mctx, pool_query, bdev_type, file_path, total_size, io_depth,
                            alignment);
    task->Wait();
    
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    pool_id_ = task->new_pool_id_;
    
    CHI_IPC->DelTask(task);
  }

  /**
   * Create bdev container - asynchronous (file-based, for backward compatibility)
   */
  hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      const std::string& file_path, chi::u64 total_size = 0,
      chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    return AsyncCreate(mctx, pool_query, BdevType::kFile, file_path, total_size, io_depth, alignment);
  }
  
  /**
   * Create bdev container - asynchronous (with explicit bdev type)
   */
  hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      BdevType bdev_type, const std::string& file_path = "", chi::u64 total_size = 0,
      chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    auto* ipc_manager = CHI_IPC;

    // CreateTask should always use admin pool, never the client's pool_id_
    // Pass all arguments directly to NewTask constructor including CreateParams arguments
    chi::u32 safe_alignment = (alignment == 0) ? 4096 : alignment;  // Ensure non-zero alignment
    auto task = ipc_manager->NewTask<chimaera::bdev::CreateTask>(
        chi::CreateTaskNode(), 
        chi::kAdminPoolId,  // Send to admin pool for GetOrCreatePool processing
        pool_query,
        "chimaera_bdev_runtime",  // chimod name
        "bdev_pool_" + std::to_string(pool_id_.ToU64()),  // pool name  
        pool_id_,  // target pool ID to create
        // CreateParams arguments:
        bdev_type, file_path, total_size, io_depth, safe_alignment
    );

    // Submit to runtime
    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Allocate multiple blocks - synchronous
   */
  ClientBlockList AllocateBlocks(const hipc::MemContext& mctx, chi::u64 size) {
    auto task = AsyncAllocate(mctx, size);
    task->Wait();
    ClientBlockList result;
    for (size_t i = 0; i < task->block_list_.blocks_.size(); ++i) {
      result.AddBlock(task->block_list_.blocks_[i]);
    }
    CHI_IPC->DelTask(task);
    return result;
  }
  
  /**
   * Allocate single block - synchronous (backward compatibility)
   * Returns the first block if multiple blocks were allocated
   */
  Block Allocate(const hipc::MemContext& mctx, chi::u64 size) {
    auto task = AsyncAllocate(mctx, size);
    task->Wait();
    Block result;
    if (task->block_list_.GetBlockCount() > 0) {
      result = task->block_list_.blocks_[0];
    }
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Allocate data block - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::AllocateTask> AsyncAllocate(
      const hipc::MemContext& mctx, chi::u64 size) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::AllocateTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), size);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Free multiple blocks - synchronous
   */
  chi::u32 FreeBlocks(const hipc::MemContext& mctx, const ClientBlockList& client_block_list) {
    auto task = AsyncFreeBlocks(mctx, client_block_list);
    task->Wait();
    chi::u32 result = task->result_code_;
    CHI_IPC->DelTask(task);
    return result;
  }
  
  /**
   * Free multiple blocks - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::FreeTask> AsyncFreeBlocks(
      const hipc::MemContext& mctx, const ClientBlockList& client_block_list) {
    auto* ipc_manager = CHI_IPC;

    // Create a task with empty block list, then populate it
    auto task = ipc_manager->NewTask<chimaera::bdev::FreeTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), Block{});
    
    // Clear the single block and add all blocks from client list
    task->block_list_.Clear();
    for (size_t i = 0; i < client_block_list.blocks_.size(); ++i) {
      task->block_list_.AddBlock(client_block_list.blocks_[i]);
    }

    ipc_manager->Enqueue(task);
    return task;
  }
  
  /**
   * Free single block - synchronous (backward compatibility)
   */
  chi::u32 Free(const hipc::MemContext& mctx, const Block& block) {
    auto task = AsyncFree(mctx, block);
    task->Wait();
    chi::u32 result = task->result_code_;
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Free single block - asynchronous (backward compatibility)
   */
  hipc::FullPtr<chimaera::bdev::FreeTask> AsyncFree(
      const hipc::MemContext& mctx, const Block& block) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::FreeTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), block);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Write data to block - synchronous
   */
  chi::u64 Write(const hipc::MemContext& mctx, const Block& block,
                 const std::vector<hshm::u8>& data) {
    auto task = AsyncWrite(mctx, block, data);
    task->Wait();
    chi::u64 bytes_written = task->bytes_written_;
    CHI_IPC->DelTask(task);
    return bytes_written;
  }

  /**
   * Write data to block - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::WriteTask> AsyncWrite(
      const hipc::MemContext& mctx, const Block& block,
      const std::vector<hshm::u8>& data) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::WriteTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), block, data);

    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Read data from block - synchronous
   */
  std::vector<hshm::u8> Read(const hipc::MemContext& mctx, const Block& block) {
    auto task = AsyncRead(mctx, block);
    task->Wait();
    std::vector<hshm::u8> result;
    result.reserve(task->data_.size());
    for (size_t i = 0; i < task->data_.size(); ++i) {
      result.push_back(task->data_[i]);
    }
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Read data from block - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::ReadTask> AsyncRead(
      const hipc::MemContext& mctx, const Block& block) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::ReadTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery::Local(), block);

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
  hipc::FullPtr<chimaera::bdev::StatTask> AsyncGetStats(
      const hipc::MemContext& mctx) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<chimaera::bdev::StatTask>(
        chi::CreateTaskNode(), pool_id_, chi::PoolQuery());

    ipc_manager->Enqueue(task);
    return task;
  }
};

}  // namespace chimaera::bdev

#endif  // BDEV_CLIENT_H_