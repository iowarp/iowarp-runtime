#include <chimaera/bdev/bdev_runtime.h>
#include <chimaera/comutex.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cmath>
#include <cstring>
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

  // Get the pool name which serves as the file path for file-based operations
  std::string pool_name = task->pool_name_.str();

  HILOG(kInfo,
        "DEBUG: Bdev runtime received params: bdev_type={}, pool_name='{}', "
        "total_size={}, io_depth={}, alignment={}",
        static_cast<chi::u32>(params.bdev_type_), pool_name, params.total_size_,
        params.io_depth_, params.alignment_);

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
    // File-based storage initialization - use pool_name as file path
    file_fd_ = open(pool_name.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (file_fd_ < 0) {
      task->return_code_ = 1;
      return;
    }

    // Get file size
    struct stat st;
    if (fstat(file_fd_, &st) != 0) {
      task->return_code_ = 2;
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
        task->return_code_ = 3;
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
      task->return_code_ = 4;
      return;
    }

    ram_size_ = params.total_size_;
    ram_buffer_ = static_cast<char*>(malloc(ram_size_));
    if (ram_buffer_ == nullptr) {
      task->return_code_ = 5;
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

  // Store user-provided performance characteristics
  perf_metrics_ = params.perf_metrics_;

  // Set success result
  task->return_code_ = 0;

  HILOG(kDebug, "bdev container created for pool: {} (Size: {} bytes)",
        pool_name, file_size_);
}

void Runtime::MonitorCreate(chi::MonitorModeId mode,
                            hipc::FullPtr<CreateTask> task,
                            chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      // REQUIRED: Set route_lane_ to indicate where task should be routed
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = lane_ptr.ptr_;
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

void Runtime::AllocateBlocks(hipc::FullPtr<AllocateBlocksTask> task,
                             chi::RunContext& ctx) {
  // Clear the block vector first
  task->blocks_.clear();

  chi::u64 total_size = task->size_;
  if (total_size == 0) {
    task->return_code_ = 0;  // Nothing to allocate
    return;
  }

  // Calculate minimum block set according to new algorithm
  std::vector<std::pair<BlockSizeCategory, chi::u32>> blocks_to_allocate;
  chi::u64 total_allocated_size = 0;

  if (total_size < (1ULL << 20)) {  // < 1MB
    // Allocate a single block of the next largest size
    BlockSizeCategory category;
    if (total_size <= kBlockSizes[0]) {  // <= 4KB
      category = BlockSizeCategory::k4KB;
    } else if (total_size <= kBlockSizes[1]) {  // <= 64KB
      category = BlockSizeCategory::k64KB;
    } else if (total_size <= kBlockSizes[2]) {  // <= 256KB
      category = BlockSizeCategory::k256KB;
    } else {  // <= 1MB
      category = BlockSizeCategory::k1MB;
    }

    blocks_to_allocate.push_back({category, 1});
    total_allocated_size = GetBlockSize(category);

  } else {  // >= 1MB
    // Allocate only 1MB blocks to meet requirement
    chi::u32 num_1mb_blocks = static_cast<chi::u32>(
        (total_size + kBlockSizes[3] - 1) / kBlockSizes[3]);
    blocks_to_allocate.push_back({BlockSizeCategory::k1MB, num_1mb_blocks});
    total_allocated_size = num_1mb_blocks * kBlockSizes[3];
  }

  // Now allocate the determined blocks using the allocation strategy
  for (const auto& block_spec : blocks_to_allocate) {
    BlockSizeCategory category = block_spec.first;
    chi::u32 num_blocks = block_spec.second;
    chi::u64 block_size = GetBlockSize(category);

    for (chi::u32 i = 0; i < num_blocks; ++i) {
      Block block;

      // Try to allocate from free list first
      if (!AllocateFromFreeList(category, block_size, block)) {
        // If no free blocks, allocate from heap
        if (!AllocateFromHeap(block_size, category, block)) {
          // If both heap and free lists exhausted, clean up and return error
          for (size_t j = 0; j < task->blocks_.size(); ++j) {
            const Block& allocated_block = task->blocks_[j];
            AddToFreeList(allocated_block);
            remaining_size_.fetch_add(allocated_block.size_);
          }
          task->blocks_.clear();
          task->return_code_ = 1;  // Out of space
          return;
        }
      }

      // Add the allocated block to the vector
      size_t old_size = task->blocks_.size();
      task->blocks_.resize(old_size + 1);
      task->blocks_[old_size] = block;
    }
  }

  // Update capacity: decrement remaining_size_ based on total allocated block
  // size
  remaining_size_.fetch_sub(total_allocated_size);

  task->return_code_ = 0;
}

void Runtime::MonitorAllocateBlocks(chi::MonitorModeId mode,
                                    hipc::FullPtr<AllocateBlocksTask> task,
                                    chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = lane_ptr.ptr_;
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

void Runtime::FreeBlocks(hipc::FullPtr<FreeBlocksTask> task,
                         chi::RunContext& ctx) {
  HILOG(kInfo, "FreeBlocks called with {} blocks", task->blocks_.size());
  // Free all blocks in the vector
  for (size_t i = 0; i < task->blocks_.size(); ++i) {
    const Block& block = task->blocks_[i];
    // Add block back to the appropriate free list
    AddToFreeList(block);
    // Update remaining size counter
    remaining_size_.fetch_add(block.size_);
  }

  task->return_code_ = 0;
}

void Runtime::MonitorFreeBlocks(chi::MonitorModeId mode,
                                hipc::FullPtr<FreeBlocksTask> task,
                                chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = lane_ptr.ptr_;
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
      task->return_code_ = 1;  // Unknown backend type
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
        ctx.route_lane_ = lane_ptr.ptr_;
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
      task->return_code_ = 1;  // Unknown backend type
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
        ctx.route_lane_ = lane_ptr.ptr_;
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

void Runtime::GetStats(hipc::FullPtr<GetStatsTask> task, chi::RunContext& ctx) {
  // Return the user-provided performance characteristics instead of calculating them
  task->metrics_ = perf_metrics_;
  task->remaining_size_ = remaining_size_.load();
  task->return_code_ = 0;
}

void Runtime::MonitorGetStats(chi::MonitorModeId mode,
                              hipc::FullPtr<GetStatsTask> task,
                              chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = lane_ptr.ptr_;
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
    chi::ScopedCoMutex lock(free_list_mutexes_[i]);
    FreeListNode* node = free_lists_[i];
    while (node) {
      FreeListNode* next = node->next_;
      delete node;
      node = next;
    }
    free_lists_[i] = nullptr;
  }

  task->return_code_ = 0;
  HILOG(kDebug, "bdev container destroyed for pool: {}", pool_id_);
}

void Runtime::MonitorDestroy(chi::MonitorModeId mode,
                             hipc::FullPtr<DestroyTask> task,
                             chi::RunContext& ctx) {
  switch (mode) {
    case chi::MonitorModeId::kLocalSchedule: {
      auto lane_ptr = GetLaneFullPtr(0, 0);  // Queue 0 (low latency), lane 0
      if (!lane_ptr.IsNull()) {
        ctx.route_lane_ = lane_ptr.ptr_;
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


chi::u64 Runtime::GetBlockSize(BlockSizeCategory category) {
  return kBlockSizes[static_cast<size_t>(category)];
}

bool Runtime::AllocateFromFreeList(BlockSizeCategory category, chi::u64 size,
                                   Block& block) {
  size_t idx = static_cast<size_t>(category);
  chi::ScopedCoMutex lock(free_list_mutexes_[idx]);

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
  chi::ScopedCoMutex lock(alloc_mutex_);

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

  chi::ScopedCoMutex lock(free_list_mutexes_[idx]);

  FreeListNode* node = new FreeListNode(block.offset_, block.size_);
  node->next_ = free_lists_[idx];
  free_lists_[idx] = node;
}

chi::u64 Runtime::AlignSize(chi::u64 size) {
  if (alignment_ == 0) {
    alignment_ = 4096;  // Set to default if somehow it's 0
  }
  return ((size + alignment_ - 1) / alignment_) * alignment_;
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
  // Convert hipc::Pointer to hipc::FullPtr<char> for data access
  hipc::FullPtr<char> data_ptr(task->data_);

  // Align buffer for direct I/O
  chi::u64 aligned_size = AlignSize(task->length_);

  // Allocate aligned buffer
  void* aligned_buffer;
  if (posix_memalign(&aligned_buffer, alignment_, aligned_size) != 0) {
    task->return_code_ = 1;
    task->bytes_written_ = 0;
    return;
  }

  // Copy data to aligned buffer
  memcpy(aligned_buffer, data_ptr.ptr_, task->length_);
  if (aligned_size > task->length_) {
    memset(static_cast<char*>(aligned_buffer) + task->length_, 0,
           aligned_size - task->length_);
  }

  // Perform async write using POSIX AIO
  chi::u64 bytes_written;
  chi::u32 result =
      PerformAsyncIO(true, task->block_.offset_, aligned_buffer, aligned_size,
                     bytes_written, task.Cast<chi::Task>());

  free(aligned_buffer);

  if (result != 0) {
    task->return_code_ = result;
    task->bytes_written_ = 0;
  } else {
    task->return_code_ = 0;
    task->bytes_written_ =
        std::min(bytes_written, static_cast<chi::u64>(task->length_));

    // Update performance metrics
    total_writes_.fetch_add(1);
    total_bytes_written_.fetch_add(task->bytes_written_);
  }
}

void Runtime::WriteToRam(hipc::FullPtr<WriteTask> task) {
  // Convert hipc::Pointer to hipc::FullPtr<char> for data access
  hipc::FullPtr<char> data_ptr(task->data_);

  // Check bounds
  if (task->block_.offset_ + task->length_ > ram_size_) {
    task->return_code_ = 1;  // Write beyond buffer bounds
    task->bytes_written_ = 0;
    return;
  }

  // Simple memory copy
  memcpy(ram_buffer_ + task->block_.offset_, data_ptr.ptr_, task->length_);

  task->return_code_ = 0;
  task->bytes_written_ = task->length_;

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
    task->return_code_ = 1;
    task->bytes_read_ = 0;
    return;
  }

  // Perform async read using POSIX AIO
  chi::u64 bytes_read;
  chi::u32 result =
      PerformAsyncIO(false, task->block_.offset_, aligned_buffer, aligned_size,
                     bytes_read, task.Cast<chi::Task>());

  if (result != 0) {
    task->return_code_ = result;
    task->bytes_read_ = 0;
    free(aligned_buffer);
    return;
  }

  // Convert hipc::Pointer to hipc::FullPtr<char> for data access
  hipc::FullPtr<char> data_ptr(task->data_);

  // Copy data to task output
  chi::u64 actual_bytes = std::min(bytes_read, task->block_.size_);
  chi::u64 available_space =
      std::min(actual_bytes, static_cast<chi::u64>(task->length_));
  memcpy(data_ptr.ptr_, aligned_buffer, available_space);

  free(aligned_buffer);

  task->return_code_ = 0;
  task->bytes_read_ = available_space;

  // Update performance metrics
  total_reads_.fetch_add(1);
  total_bytes_read_.fetch_add(available_space);
}

void Runtime::ReadFromRam(hipc::FullPtr<ReadTask> task) {
  // Convert hipc::Pointer to hipc::FullPtr<char> for data access
  hipc::FullPtr<char> data_ptr(task->data_);

  // Check bounds
  if (task->block_.offset_ + task->block_.size_ > ram_size_) {
    task->return_code_ = 1;  // Read beyond buffer bounds
    task->bytes_read_ = 0;
    return;
  }

  // Copy data from RAM buffer to task output (limit by available buffer space)
  chi::u64 available_space =
      std::min(task->block_.size_, static_cast<chi::u64>(task->length_));
  memcpy(data_ptr.ptr_, ram_buffer_ + task->block_.offset_, available_space);

  task->return_code_ = 0;
  task->bytes_read_ = available_space;

  // Update performance metrics
  total_reads_.fetch_add(1);
  total_bytes_read_.fetch_add(available_space);
}

// VIRTUAL METHOD IMPLEMENTATIONS (now in autogen/bdev_lib_exec.cc)

chi::u64 Runtime::GetWorkRemaining() const { return 0; }

}  // namespace chimaera::bdev

// Define ChiMod entry points using CHI_TASK_CC macro
CHI_TASK_CC(chimaera::bdev::Runtime)