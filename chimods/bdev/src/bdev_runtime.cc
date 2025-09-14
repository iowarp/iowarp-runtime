#include <chimaera/bdev/bdev_runtime.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

namespace chimaera::bdev {

// Block size constants (in bytes)
const chi::u64 kBlockSizes[] = {
    4096,    // 4KB
    65536,   // 64KB
    262144,  // 256KB
    1048576  // 1MB
};

Runtime::~Runtime() {
  // Clean up libaio (only for file-based storage)
  if (bdev_type_ == BdevType::kFile) {
    CleanupAsyncIO();
  }

  // Clean up storage backend
  if (bdev_type_ == BdevType::kFile && file_fd_ >= 0) {
    close(file_fd_);
  } else if (bdev_type_ == BdevType::kRam && ram_buffer_ != nullptr) {
    free(ram_buffer_);
    ram_buffer_ = nullptr;
  }

  // Clean up free lists
  for (size_t i = 0; i < static_cast<size_t>(BlockSizeCategory::kMaxCategories);
       ++i) {
    FreeListNode* node = free_lists_[i];
    while (node) {
      FreeListNode* next = node->next_;
      delete node;
      node = next;
    }
  }
}

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& ctx) {
  // Get the creation parameters
  hipc::CtxAllocator<CHI_MAIN_ALLOC_T> ctx_alloc(HSHM_MCTX, main_allocator_);
  CreateParams params = task->GetParams(ctx_alloc);

  HILOG(kInfo,
        "DEBUG: Bdev runtime received params: bdev_type={}, file_path='{}', "
        "total_size={}, io_depth={}, alignment={}",
        static_cast<chi::u32>(params.bdev_type_), params.file_path_,
        params.total_size_, params.io_depth_, params.alignment_);

  // Initialize the container with pool information and domain query
  chi::Container::Init(task->pool_id_, task->pool_query_);

  // Create local queues with explicit queue IDs and priorities
  CreateLocalQueue(0, 4,
                   chi::kLowLatency);  // Queue 0: 4 lanes for low latency tasks
  CreateLocalQueue(
      1, 2, chi::kHighLatency);  // Queue 1: 2 lanes for high latency tasks

  // Store backend type
  bdev_type_ = params.bdev_type_;

  // Initialize storage backend based on type
  if (bdev_type_ == BdevType::kFile) {
    // File-based storage initialization
    file_fd_ =
        open(params.file_path_.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (file_fd_ < 0) {
      task->result_code_ = 1;
      return;
    }

    // Get file size
    struct stat st;
    if (fstat(file_fd_, &st) != 0) {
      task->result_code_ = 2;
      close(file_fd_);
      file_fd_ = -1;
      return;
    }

    file_size_ = st.st_size;
    if (params.total_size_ > 0 && params.total_size_ < file_size_) {
      file_size_ = params.total_size_;
    }

    // If file is empty, create it with default size (1GB)
    if (file_size_ == 0) {
      file_size_ = (params.total_size_ > 0) ? params.total_size_
                                            : (1ULL << 30);  // 1GB default
      if (ftruncate(file_fd_, file_size_) != 0) {
        task->result_code_ = 3;
        close(file_fd_);
        file_fd_ = -1;
        return;
      }
    }

    // Initialize async I/O for file backend
    InitializeAsyncIO();

  } else if (bdev_type_ == BdevType::kRam) {
    // RAM-based storage initialization
    if (params.total_size_ == 0) {
      // RAM backend requires explicit size
      task->result_code_ = 4;
      return;
    }

    ram_size_ = params.total_size_;
    ram_buffer_ = static_cast<char*>(malloc(ram_size_));
    if (ram_buffer_ == nullptr) {
      task->result_code_ = 5;
      return;
    }

    // Initialize RAM buffer to zero
    memset(ram_buffer_, 0, ram_size_);
    file_size_ = ram_size_;  // Use file_size_ for common allocation logic

    HILOG(kInfo, "DEBUG: Initialized RAM backend with size {}", ram_size_);
  }

  // Initialize common parameters
  alignment_ = params.alignment_;
  io_depth_ = params.io_depth_;

  HILOG(kInfo, "DEBUG: Setting alignment_={} from params.alignment_={}",
        alignment_, params.alignment_);

  // Initialize the data allocator
  InitializeAllocator();

  // Initialize performance tracking
  start_time_ = std::chrono::high_resolution_clock::now();
  total_reads_ = 0;
  total_writes_ = 0;
  total_bytes_read_ = 0;
  total_bytes_written_ = 0;

  // Conduct performance benchmark
  BenchmarkPerformance();

  // Set success result
  task->result_code_ = 0;

  std::cout << "bdev container created for file: " << params.file_path_
            << " (Size: " << file_size_ << " bytes)" << std::endl;
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task,
                            chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // REQUIRED: Set route_lane_ to indicate where task should be routed
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      // Optional: Global coordination
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Allocate(hipc::FullPtr<AllocateTask> task, chi::RunContext& ctx) {
  // Clear the block list first
  task->block_list_.Clear();

  // Allocate multiple blocks to satisfy the requested size
  if (!AllocateMultipleBlocks(task->size_, task->block_list_)) {
    task->result_code_ = 1;  // Out of space
    return;
  }

  task->result_code_ = 0;
}

void Runtime::MonitorAllocate(chi::MonitorModeId mode,
                              hipc::FullPtr<AllocateTask> task,
                              chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Free(hipc::FullPtr<FreeTask> task, chi::RunContext& ctx) {
  // Free all blocks in the list
  for (size_t i = 0; i < task->block_list_.blocks_.size(); ++i) {
    const Block& block = task->block_list_.blocks_[i];
    // Add block back to the appropriate free list
    AddToFreeList(block);
    // Update remaining size counter
    remaining_size_.fetch_add(block.size_);
  }

  task->result_code_ = 0;
}

void Runtime::MonitorFree(chi::MonitorModeId mode, hipc::FullPtr<FreeTask> task,
                          chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Write(hipc::FullPtr<WriteTask> task, chi::RunContext& ctx) {
  switch (bdev_type_) {
    case BdevType::kFile:
      WriteToFile(task);
      break;
    case BdevType::kRam:
      WriteToRam(task);
      break;
    default:
      task->result_code_ = 1;  // Unknown backend type
      task->bytes_written_ = 0;
      break;
  }
}

void Runtime::MonitorWrite(chi::MonitorModeId mode,
                           hipc::FullPtr<WriteTask> task,
                           chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // Route to high latency queue for I/O operations
      auto lane_ptr = GetLaneFullPtr(1, 0);  // Queue 1 (high latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Read(hipc::FullPtr<ReadTask> task, chi::RunContext& ctx) {
  switch (bdev_type_) {
    case BdevType::kFile:
      ReadFromFile(task);
      break;
    case BdevType::kRam:
      ReadFromRam(task);
      break;
    default:
      task->result_code_ = 1;  // Unknown backend type
      task->bytes_read_ = 0;
      break;
  }
}

void Runtime::MonitorRead(chi::MonitorModeId mode, hipc::FullPtr<ReadTask> task,
                          chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // Route to high latency queue for I/O operations
      auto lane_ptr = GetLaneFullPtr(1, 1);  // Queue 1 (high latency), lane 1
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Stat(hipc::FullPtr<StatTask> task, chi::RunContext& ctx) {
  auto current_time = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time -
                                                                  start_time_);

  PerfMetrics metrics;

  if (elapsed.count() > 0) {
    double elapsed_sec = elapsed.count();
    chi::u64 total_ops = total_reads_ + total_writes_;
    chi::u64 total_bytes = total_bytes_read_ + total_bytes_written_;

    metrics.iops_ = total_ops / elapsed_sec;

    if (total_reads_ > 0) {
      metrics.read_bandwidth_mbps_ =
          (total_bytes_read_ / (1024.0 * 1024.0)) / elapsed_sec;
    }

    if (total_writes_ > 0) {
      metrics.write_bandwidth_mbps_ =
          (total_bytes_written_ / (1024.0 * 1024.0)) / elapsed_sec;
    }

    // Simplified latency calculation - in real implementation would track
    // per-operation
    if (total_ops > 0) {
      metrics.read_latency_us_ = 1000.0;   // Placeholder
      metrics.write_latency_us_ = 1000.0;  // Placeholder
    }
  }

  task->metrics_ = metrics;
  task->remaining_size_ = remaining_size_.load();
  task->result_code_ = 0;
}

void Runtime::MonitorStat(chi::MonitorModeId mode, hipc::FullPtr<StatTask> task,
                          chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& ctx) {
  // Close file descriptor if open
  if (file_fd_ >= 0) {
    close(file_fd_);
    file_fd_ = -1;
  }

  // Clean up free lists
  for (size_t i = 0; i < static_cast<size_t>(BlockSizeCategory::kMaxCategories);
       ++i) {
    std::lock_guard<std::mutex> lock(free_list_mutexes_[i]);
    FreeListNode* node = free_lists_[i];
    while (node) {
      FreeListNode* next = node->next_;
      delete node;
      node = next;
    }
    free_lists_[i] = nullptr;
  }

  task->result_code_ = 0;
  std::cout << "bdev container destroyed for pool: " << pool_id_ << std::endl;
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                             hipc::FullPtr<DestroyTask> task,
                             chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = static_cast<void*>(lane_ptr.ptr_);
      }
      break;
    }
    case chi::MonitorModeId::kGlobalSchedule: {
      break;
    }
    case chi::MonitorModeId::kEstLoad: {
      // Optional: Load estimation
      break;
    }
  }
}

void Runtime::InitializeAllocator() {
  // Initialize free lists
  for (size_t i = 0; i < static_cast<size_t>(BlockSizeCategory::kMaxCategories);
       ++i) {
    free_lists_[i] = nullptr;
  }

  // Start allocation from beginning of file
  next_offset_ = 0;
  remaining_size_ = file_size_;
}

void Runtime::BenchmarkPerformance() {
  // Skip benchmarking for RAM backend (very fast operations)
  if (bdev_type_ == BdevType::kRam) {
    std::cout << "Skipping benchmark for RAM backend (operations < 1us)"
              << std::endl;
    return;
  }

  // Simple benchmark for file backend: write and read a small block
  const chi::u64 benchmark_size = 4096;
  void* aligned_buffer;

  if (posix_memalign(&aligned_buffer, alignment_, benchmark_size) == 0) {
    memset(aligned_buffer, 0xAA, benchmark_size);

    auto start = std::chrono::high_resolution_clock::now();
    pwrite(file_fd_, aligned_buffer, benchmark_size, 0);
    auto end = std::chrono::high_resolution_clock::now();

    auto write_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    start = std::chrono::high_resolution_clock::now();
    pread(file_fd_, aligned_buffer, benchmark_size, 0);
    end = std::chrono::high_resolution_clock::now();

    auto read_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    free(aligned_buffer);

    std::cout << "Benchmark results - Write: " << write_duration.count()
              << "us, Read: " << read_duration.count() << "us" << std::endl;
  }
}

BlockSizeCategory Runtime::DetermineBlockSizeCategory(chi::u64 size) {
  if (size <= kBlockSizes[0]) return BlockSizeCategory::k4KB;
  if (size <= kBlockSizes[1]) return BlockSizeCategory::k64KB;
  if (size <= kBlockSizes[2]) return BlockSizeCategory::k256KB;
  return BlockSizeCategory::k1MB;
}

chi::u64 Runtime::GetBlockSize(BlockSizeCategory category) {
  return kBlockSizes[static_cast<size_t>(category)];
}

bool Runtime::AllocateFromFreeList(BlockSizeCategory category, chi::u64 size,
                                   Block& block) {
  size_t idx = static_cast<size_t>(category);
  std::lock_guard<std::mutex> lock(free_list_mutexes_[idx]);

  FreeListNode** current = &free_lists_[idx];
  while (*current) {
    if ((*current)->size_ >= size) {
      FreeListNode* node = *current;
      *current = node->next_;

      block.offset_ = node->offset_;
      block.size_ = node->size_;
      block.block_type_ = static_cast<chi::u32>(category);

      delete node;
      return true;
    }
    current = &((*current)->next_);
  }

  return false;
}

bool Runtime::AllocateFromHeap(chi::u64 size, BlockSizeCategory category,
                               Block& block) {
  std::lock_guard<std::mutex> lock(alloc_mutex_);

  chi::u64 aligned_size = AlignSize(size);

  if (next_offset_ + aligned_size > file_size_) {
    return false;  // Out of space
  }

  block.offset_ = next_offset_;
  block.size_ = aligned_size;
  block.block_type_ = static_cast<chi::u32>(category);

  next_offset_ += aligned_size;
  remaining_size_.fetch_sub(aligned_size);

  return true;
}

void Runtime::AddToFreeList(const Block& block) {
  size_t idx = block.block_type_;
  if (idx >= static_cast<size_t>(BlockSizeCategory::kMaxCategories)) {
    return;  // Invalid block type
  }

  std::lock_guard<std::mutex> lock(free_list_mutexes_[idx]);

  FreeListNode* node = new FreeListNode(block.offset_, block.size_);
  node->next_ = free_lists_[idx];
  free_lists_[idx] = node;
}

chi::u64 Runtime::AlignSize(chi::u64 size) {
  HILOG(kInfo, "DEBUG: AlignSize called with size={}, alignment_={}", size,
        alignment_);
  if (alignment_ == 0) {
    HILOG(kInfo, "AlignSize called with alignment_ = 0, using default 4096");
    alignment_ = 4096;  // Set to default if somehow it's 0
  }
  return ((size + alignment_ - 1) / alignment_) * alignment_;
}

chi::u32 Runtime::CalculateBlocksNeeded(chi::u64 total_size) {
  if (total_size == 0) {
    return 0;
  }

  chi::u32 blocks_needed = 0;
  chi::u64 remaining_size = total_size;

  // Start with largest blocks and work down
  for (int i = static_cast<int>(BlockSizeCategory::kMaxCategories) - 1; i >= 0;
       --i) {
    BlockSizeCategory category = static_cast<BlockSizeCategory>(i);
    chi::u64 block_size = GetBlockSize(category);

    chi::u32 blocks_of_this_size =
        static_cast<chi::u32>(remaining_size / block_size);
    blocks_needed += blocks_of_this_size;
    remaining_size -= blocks_of_this_size * block_size;

    if (remaining_size == 0) {
      break;
    }
  }

  // If there's still remaining size, we need one more block of smallest size
  if (remaining_size > 0) {
    blocks_needed += 1;
  }

  return blocks_needed;
}

bool Runtime::AllocateMultipleBlocks(chi::u64 total_size,
                                     BlockList& block_list) {
  if (total_size == 0) {
    return true;  // Nothing to allocate
  }

  chi::u64 remaining_size = total_size;

  // Start with largest blocks and work down to minimize fragmentation
  for (int i = static_cast<int>(BlockSizeCategory::kMaxCategories) - 1; i >= 0;
       --i) {
    BlockSizeCategory category = static_cast<BlockSizeCategory>(i);
    chi::u64 block_size = GetBlockSize(category);

    // Allocate as many blocks of this size as needed
    while (remaining_size >= block_size) {
      Block block;

      // Try to allocate from free list first
      if (!AllocateFromFreeList(category, block_size, block)) {
        // Allocate from heap if no free blocks available
        if (!AllocateFromHeap(block_size, category, block)) {
          // If we can't allocate this block, try smaller blocks
          break;
        }
      }

      // Add the allocated block to the list
      block_list.AddBlock(block);
      remaining_size -= block.size_;
    }
  }

  // Check if we successfully allocated enough space
  if (remaining_size > 0) {
    // We couldn't allocate enough space - need to free what we allocated and
    // return false
    for (size_t i = 0; i < block_list.blocks_.size(); ++i) {
      const Block& block = block_list.blocks_[i];
      AddToFreeList(block);
      remaining_size_.fetch_add(block.size_);
    }
    block_list.Clear();
    return false;
  }

  return true;
}

void Runtime::UpdatePerformanceMetrics(bool is_write, chi::u64 bytes,
                                       double duration_us) {
  // This is a simplified implementation
  // In a real implementation, you'd maintain running averages or histograms
}

void Runtime::InitializeAsyncIO() {
  // No initialization needed - will create aiocb on-demand
}

void Runtime::CleanupAsyncIO() {
  // No cleanup needed - aiocb created on stack
}

chi::u32 Runtime::PerformAsyncIO(bool is_write, chi::u64 offset, void* buffer,
                                 chi::u64 size, chi::u64& bytes_transferred,
                                 hipc::FullPtr<chi::Task> task) {
  // Create aiocb on-demand
  struct aiocb aiocb_storage;
  struct aiocb* aiocb = &aiocb_storage;

  // Initialize the AIO control block
  memset(aiocb, 0, sizeof(struct aiocb));
  aiocb->aio_fildes = file_fd_;
  aiocb->aio_buf = buffer;
  aiocb->aio_nbytes = size;
  aiocb->aio_offset = offset;
  aiocb->aio_lio_opcode = is_write ? LIO_WRITE : LIO_READ;

  // Submit the I/O operation
  int result;
  if (is_write) {
    result = aio_write(aiocb);
  } else {
    result = aio_read(aiocb);
  }

  if (result != 0) {
    return 2;  // Failed to submit I/O
  }

  // Poll for completion
  while (true) {
    int error_code = aio_error(aiocb);
    if (error_code == 0) {
      // Operation completed successfully
      break;
    } else if (error_code != EINPROGRESS) {
      // Operation failed
      return 3;
    }
    // Operation still in progress, yield the current task
    if (!task.IsNull()) {
      task->Yield();
    } else {
      std::this_thread::yield();
    }
  }

  // Get the result
  ssize_t bytes_result = aio_return(aiocb);
  if (bytes_result < 0) {
    return 4;  // I/O operation failed
  }

  bytes_transferred = bytes_result;
  return 0;  // Success
}

// Backend-specific write operations
void Runtime::WriteToFile(hipc::FullPtr<WriteTask> task) {
  // Align buffer for direct I/O
  chi::u64 aligned_size = AlignSize(task->data_.size());

  // Allocate aligned buffer
  void* aligned_buffer;
  if (posix_memalign(&aligned_buffer, alignment_, aligned_size) != 0) {
    task->result_code_ = 1;
    task->bytes_written_ = 0;
    return;
  }

  // Copy data to aligned buffer
  memcpy(aligned_buffer, task->data_.data(), task->data_.size());
  if (aligned_size > task->data_.size()) {
    memset(static_cast<char*>(aligned_buffer) + task->data_.size(), 0,
           aligned_size - task->data_.size());
  }

  // Perform async write using POSIX AIO
  chi::u64 bytes_written;
  chi::u32 result =
      PerformAsyncIO(true, task->block_.offset_, aligned_buffer, aligned_size,
                     bytes_written, task.Cast<chi::Task>());

  free(aligned_buffer);

  if (result != 0) {
    task->result_code_ = result;
    task->bytes_written_ = 0;
  } else {
    task->result_code_ = 0;
    task->bytes_written_ =
        std::min(bytes_written, static_cast<chi::u64>(task->data_.size()));

    // Update performance metrics
    total_writes_.fetch_add(1);
    total_bytes_written_.fetch_add(task->bytes_written_);
  }
}

void Runtime::WriteToRam(hipc::FullPtr<WriteTask> task) {
  // Check bounds
  if (task->block_.offset_ + task->data_.size() > ram_size_) {
    task->result_code_ = 1;  // Write beyond buffer bounds
    task->bytes_written_ = 0;
    return;
  }

  // Simple memory copy
  memcpy(ram_buffer_ + task->block_.offset_, task->data_.data(),
         task->data_.size());

  task->result_code_ = 0;
  task->bytes_written_ = task->data_.size();

  // Update performance metrics
  total_writes_.fetch_add(1);
  total_bytes_written_.fetch_add(task->bytes_written_);
}

// Backend-specific read operations
void Runtime::ReadFromFile(hipc::FullPtr<ReadTask> task) {
  // Align buffer for direct I/O
  chi::u64 aligned_size = AlignSize(task->block_.size_);

  // Allocate aligned buffer
  void* aligned_buffer;
  if (posix_memalign(&aligned_buffer, alignment_, aligned_size) != 0) {
    task->result_code_ = 1;
    task->bytes_read_ = 0;
    return;
  }

  // Perform async read using POSIX AIO
  chi::u64 bytes_read;
  chi::u32 result =
      PerformAsyncIO(false, task->block_.offset_, aligned_buffer, aligned_size,
                     bytes_read, task.Cast<chi::Task>());

  if (result != 0) {
    task->result_code_ = result;
    task->bytes_read_ = 0;
    free(aligned_buffer);
    return;
  }

  // Copy data to task output
  chi::u64 actual_bytes = std::min(bytes_read, task->block_.size_);
  task->data_.resize(actual_bytes);
  memcpy(task->data_.data(), aligned_buffer, actual_bytes);

  free(aligned_buffer);

  task->result_code_ = 0;
  task->bytes_read_ = actual_bytes;

  // Update performance metrics
  total_reads_.fetch_add(1);
  total_bytes_read_.fetch_add(actual_bytes);
}

void Runtime::ReadFromRam(hipc::FullPtr<ReadTask> task) {
  // Check bounds
  if (task->block_.offset_ + task->block_.size_ > ram_size_) {
    task->result_code_ = 1;  // Read beyond buffer bounds
    task->bytes_read_ = 0;
    return;
  }

  // Copy data from RAM buffer to task output
  task->data_.resize(task->block_.size_);
  memcpy(task->data_.data(), ram_buffer_ + task->block_.offset_,
         task->block_.size_);

  task->result_code_ = 0;
  task->bytes_read_ = task->block_.size_;

  // Update performance metrics
  total_reads_.fetch_add(1);
  total_bytes_read_.fetch_add(task->bytes_read_);
}

// VIRTUAL METHOD IMPLEMENTATIONS (now in autogen/bdev_lib_exec.cc)

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

}  // namespace chimaera::bdev

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::bdev::Runtime)