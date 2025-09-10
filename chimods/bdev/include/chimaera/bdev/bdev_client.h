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

class Client : public chi::ContainerClient {
 public:
  Client() = default;
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create bdev container - synchronous
   */
  void Create(const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
              const std::string& file_path, chi::u64 total_size = 0,
              chi::u32 io_depth = 32, chi::u32 alignment = 4096) {
    auto task = AsyncCreate(mctx, pool_query, file_path, total_size, io_depth,
                            alignment);
    task->Wait();
    CHI_IPC->DelTask(task);
  }

  /**
   * Create bdev container - asynchronous
   */
  hipc::FullPtr<chimaera::bdev::CreateTask> AsyncCreate(
      const hipc::MemContext& mctx, const chi::PoolQuery& pool_query,
      const std::string& file_path, chi::u64 total_size = 0,
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
        0,   // domain flags
        pool_id_,  // target pool ID to create
        // CreateParams arguments:
        file_path, total_size, io_depth, safe_alignment
    );

    // Submit to runtime
    ipc_manager->Enqueue(task);
    return task;
  }

  /**
   * Allocate data block - synchronous
   */
  Block Allocate(const hipc::MemContext& mctx, chi::u64 size) {
    auto task = AsyncAllocate(mctx, size);
    task->Wait();
    Block result = task->block_;
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
   * Free data block - synchronous
   */
  chi::u32 Free(const hipc::MemContext& mctx, const Block& block) {
    auto task = AsyncFree(mctx, block);
    task->Wait();
    chi::u32 result = task->result_code_;
    CHI_IPC->DelTask(task);
    return result;
  }

  /**
   * Free data block - asynchronous
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