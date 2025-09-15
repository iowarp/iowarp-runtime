# Boost Fiber
In CMake:
```
find_package(Boost REQUIRED COMPONENTS regex system filesystem fiber REQUIRED)
```

In C++:
```cpp
#include <boost/context/fiber_fcontext.hpp>

namespace bctx = boost::context::detail;

bctx::transfer_t shared_xfer;

void f3(bctx::transfer_t t) {
  ++value1;
  shared_xfer = t;
  shared_xfer = bctx::jump_fcontext(shared_xfer.fctx, 0);
  ++value1;
  shared_xfer = bctx::jump_fcontext(shared_xfer.fctx, shared_xfer.data);
}

TEST_CASE("TestBoostFcontext") {
  value1 = 0;
  stack_allocator alloc;
  int size = KILOBYTES(64);

  hshm::Timer t;
  t.Resume();
  size_t ops = (1 << 20);

  for (size_t i = 0; i < ops; ++i) {
    void *sp = alloc.allocate(size);
    shared_xfer.fctx = bctx::make_fcontext(sp, size, f3);
    shared_xfer = bctx::jump_fcontext(shared_xfer.fctx, 0);
    shared_xfer = bctx::jump_fcontext(shared_xfer.fctx, 0);
    alloc.deallocate(sp, size);
  }

  t.Pause();
  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}
```

# Hermes SHM (HSHM)

All hermes_shm tools can be included with the single header ``<hermes_shm/hermes_shm.h>``.

# HSHM Allocator and Backend Management Guide

## Overview

The HSHM Memory Management system provides a two-tier architecture where Memory Backends manage raw memory resources and Allocators manage allocation strategies within those backends. This guide demonstrates how to create, destroy, and attach allocators and backends for both single-process and distributed (MPI) applications.

## Core Concepts

### Memory Backends

Memory backends provide the underlying storage mechanism:

```cpp
#include "hermes_shm/memory/memory_manager.h"

namespace hshm::ipc {
    enum class MemoryBackendType {
        kPosixShmMmap,      // POSIX shared memory with mmap
        kMallocBackend,     // Standard malloc/free
        kArrayBackend,      // Pre-allocated array
        kPosixMmap,         // POSIX mmap files
        kGpuMalloc,         // GPU malloc (CUDA/ROCm)
        kGpuShmMmap,        // GPU shared memory
    };
}
```

### Allocators

Allocators implement different allocation strategies:

- **StackAllocator**: Linear allocation (fast, no fragmentation)
- **ScalablePageAllocator**: Page-based allocation (balanced performance/flexibility)
- **ThreadLocalAllocator**: Thread-local allocation pools
- **MallocAllocator**: Wrapper around standard malloc

## Single-Process Allocator Management

### Basic Backend and Allocator Setup

```cpp
#include "hermes_shm/memory/memory_manager.h"
#include "hermes_shm/memory/allocator/stack_allocator.h"
#include "hermes_shm/memory/backend/posix_shm_mmap.h"

void basic_allocator_setup_example() {
    // Get the memory manager singleton
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    
    // Define IDs
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);  // major=1, minor=0
    
    // Step 1: Create a memory backend
    std::string shm_url = "my_shared_memory";
    size_t backend_size = hshm::Unit<size_t>::Gigabytes(1);
    
    mem_mngr->CreateBackend<hipc::PosixShmMmap>(
        backend_id, backend_size, shm_url);
    
    printf("Created backend with ID %u, size %zu GB\n", 
           backend_id.id_, backend_size / (1024*1024*1024));
    
    // Step 2: Create an allocator on the backend
    size_t custom_header_size = sizeof(int);  // Space for custom metadata
    mem_mngr->CreateAllocator<hipc::StackAllocator>(
        backend_id, alloc_id, custom_header_size);
    
    // Step 3: Get the allocator for use
    auto* allocator = mem_mngr->GetAllocator<hipc::StackAllocator>(alloc_id);
    if (allocator) {
        printf("✓ Stack allocator created successfully\n");
        
        // Initialize custom header
        auto* custom_header = allocator->template GetCustomHeader<int>();
        *custom_header = 0x12345678;  // Custom metadata
        
        // Use the allocator
        auto full_ptr = allocator->Allocate(HSHM_MCTX, 1024);
        printf("Allocated %zu bytes at offset %zu\n", 
               size_t(1024), full_ptr.shm_.off_.load());
        
        // Free the memory
        allocator->Free(HSHM_MCTX, full_ptr);
        
        // Verify custom header persisted
        printf("Custom header: 0x%08X\n", *custom_header);
    }
    
    // Step 4: Cleanup (usually done at program exit)
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    printf("Cleanup completed\n");
}
```

### Multiple Allocators on Single Backend
Not supported.

## Distributed (MPI) Allocator Management

### MPI Rank 0 (Leader) Setup

```cpp
#include <mpi.h>

template<typename AllocatorT>
void SetupMpiAllocatorRank0() {
    // This runs only on MPI rank 0 (leader process)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0) return;
    
    printf("Rank 0: Setting up shared memory backend and allocator\\n");
    
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    std::string shm_url = "mpi_shared_allocator";
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);
    
    // Clean up any existing resources
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    // Create shared memory backend
    mem_mngr->CreateBackend<hipc::PosixShmMmap>(
        backend_id, hshm::Unit<size_t>::Gigabytes(1), shm_url);
    
    // Create allocator with custom header space
    struct AllocatorHeader {
        int magic_number;
        size_t total_allocations;
        size_t peak_usage;
    };
    
    mem_mngr->CreateAllocator<AllocatorT>(
        backend_id, alloc_id, sizeof(AllocatorHeader));
    
    auto* allocator = mem_mngr->GetAllocator<AllocatorT>(alloc_id);
    if (allocator) {
        // Initialize shared header
        auto* header = allocator->template GetCustomHeader<AllocatorHeader>();
        header->magic_number = 0xDEADBEEF;
        header->total_allocations = 0;
        header->peak_usage = 0;
        
        printf("Rank 0: ✓ Created %s with shared header\\n", 
               typeid(AllocatorT).name());
    }
}
```

### MPI Non-Leader Ranks Setup

```cpp
template<typename AllocatorT>
void SetupMpiAllocatorRankN() {
    // This runs on all MPI ranks except 0 (follower processes)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) return;
    
    printf("Rank %d: Attaching to shared memory backend\\n", rank);
    
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    std::string shm_url = "mpi_shared_allocator";
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);
    
    // Clean up any existing local state
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    // Attach to existing shared memory (created by rank 0)
    mem_mngr->AttachBackend(hshm::ipc::MemoryBackendType::kPosixShmMmap, shm_url);
    
    // Get the allocator (already created by rank 0)
    auto* allocator = mem_mngr->GetAllocator<AllocatorT>(alloc_id);
    if (allocator) {
        struct AllocatorHeader {
            int magic_number;
            size_t total_allocations;
            size_t peak_usage;
        };
        
        // Verify we can access shared header
        auto* header = allocator->template GetCustomHeader<AllocatorHeader>();
        if (header->magic_number == 0xDEADBEEF) {
            printf("Rank %d: ✓ Successfully attached to shared allocator\\n", rank);
        } else {
            printf("Rank %d: ✗ Shared header validation failed\\n", rank);
        }
    } else {
        printf("Rank %d: ✗ Failed to get allocator after attach\\n", rank);
    }
}
```

## Best Practices

1. **Backend Selection**:
   - Use `PosixShmMmap` for multi-process applications requiring shared memory
   - Use `MallocBackend` for single-process applications requiring maximum speed
   - Use `PosixMmap` for persistent storage requirements
   - Use `ArrayBackend` for embedded systems or when memory is pre-allocated

2. **MPI Setup Pattern**:
   - Rank 0 creates backends and allocators
   - Other ranks attach to existing shared memory
   - Use barriers to synchronize setup phases
   - Clean up existing resources before creating new ones

3. **Resource Management**:
   - Always unregister allocators before destroying backends
   - Use RAII patterns when possible
   - Check allocation success before using pointers
   - Monitor memory usage through allocator statistics

4. **Custom Headers**:
   - Use atomic types for multi-process shared statistics
   - Include version numbers for compatibility checking
   - Add application-specific metadata as needed
   - Initialize headers immediately after allocator creation

5. **Error Handling**:
   - Check return values from all allocation operations
   - Handle backend creation failures gracefully
   - Verify shared memory attachment in MPI scenarios
   - Implement retry logic for transient failures

6. **Performance Optimization**:
   - Choose appropriate allocator types for workload patterns
   - Monitor fragmentation and allocation/deallocation patterns
   - Use aligned allocations when needed for SIMD operations
   - Consider thread-local allocators for high-frequency allocations

7. **Testing and Validation**:
   - Test with various allocation sizes and patterns
   - Verify data integrity after allocations
   - Test alignment requirements for specialized workloads
   - Validate proper cleanup and resource deallocation

# HSHM Atomic Types Guide

## Overview

The Atomic Types API in Hermes Shared Memory (HSHM) provides cross-platform atomic operations with support for CPU, GPU (CUDA/ROCm), and non-atomic variants. The API abstracts platform differences and provides consistent atomic operations for thread-safe programming across different execution environments.

## Atomic Type Variants

### Platform-Specific Atomic Types

```cpp
#include "hermes_shm/types/atomic.h"

void atomic_variants_example() {
    // Standard atomic (uses std::atomic on host, GPU atomics on device)
    hshm::ipc::atomic<int> standard_atomic(42);
    
    // Non-atomic (for single-threaded or externally synchronized code)
    hshm::ipc::nonatomic<int> non_atomic_value(100);
    
    // Explicit GPU atomic (CUDA/ROCm specific)
#if HSHM_ENABLE_CUDA || HSHM_ENABLE_ROCM
    hshm::ipc::rocm_atomic<int> gpu_atomic(200);
#endif
    
    // Explicit standard library atomic
    hshm::ipc::std_atomic<int> std_lib_atomic(300);
    
    // Conditional atomic - chooses atomic or non-atomic based on template parameter
    hshm::ipc::opt_atomic<int, true>  conditional_atomic(400);     // Uses atomic
    hshm::ipc::opt_atomic<int, false> conditional_nonatomic(500); // Uses nonatomic
    
    printf("Standard atomic: %d\n", standard_atomic.load());
    printf("Non-atomic: %d\n", non_atomic_value.load());
    printf("Conditional atomic: %d\n", conditional_atomic.load());
}
```

## Basic Atomic Operations

### Load, Store, and Exchange

```cpp
void basic_atomic_operations() {
    hshm::ipc::atomic<int> counter(0);
    
    // Load value
    int current = counter.load();
    printf("Current value: %d\n", current);
    
    // Store new value
    counter.store(10);
    printf("After store(10): %d\n", counter.load());
    
    // Exchange (atomically set new value and return old)
    int old_value = counter.exchange(20);
    printf("Exchange returned: %d, new value: %d\n", old_value, counter.load());
    
    // Compare and exchange (conditional atomic update)
    int expected = 20;
    bool success = counter.compare_exchange_weak(expected, 30);
    printf("CAS success: %s, value: %d\n", success ? "yes" : "no", counter.load());
    
    // Try CAS with wrong expected value
    expected = 25;  // Wrong expected value
    success = counter.compare_exchange_strong(expected, 40);
    printf("CAS with wrong expected: %s, value: %d, expected now: %d\n", 
           success ? "yes" : "no", counter.load(), expected);
}
```

### Arithmetic Operations

```cpp
void arithmetic_operations_example() {
    hshm::ipc::atomic<int> counter(10);
    
    // Fetch and add
    int old_val = counter.fetch_add(5);
    printf("fetch_add(5): old=%d, new=%d\n", old_val, counter.load());
    
    // Fetch and subtract
    old_val = counter.fetch_sub(3);
    printf("fetch_sub(3): old=%d, new=%d\n", old_val, counter.load());
    
    // Increment operators
    ++counter;  // Pre-increment
    printf("After pre-increment: %d\n", counter.load());
    
    counter++;  // Post-increment
    printf("After post-increment: %d\n", counter.load());
    
    // Decrement operators
    --counter;  // Pre-decrement
    printf("After pre-decrement: %d\n", counter.load());
    
    counter--;  // Post-decrement
    printf("After post-decrement: %d\n", counter.load());
    
    // Assignment operators
    counter += 10;
    printf("After += 10: %d\n", counter.load());
    
    counter -= 5;
    printf("After -= 5: %d\n", counter.load());
}
```

### Bitwise Operations

```cpp
void bitwise_operations_example() {
    hshm::ipc::atomic<uint32_t> flags(0xF0F0F0F0);
    
    printf("Initial flags: 0x%08X\n", flags.load());
    
    // Bitwise AND
    uint32_t result = (flags & 0xFF00FF00).load();
    printf("flags & 0xFF00FF00 = 0x%08X\n", result);
    
    // Bitwise OR
    result = (flags | 0x0F0F0F0F).load();
    printf("flags | 0x0F0F0F0F = 0x%08X\n", result);
    
    // Bitwise XOR
    result = (flags ^ 0xFFFFFFFF).load();
    printf("flags ^ 0xFFFFFFFF = 0x%08X\n", result);
    
    // Assignment bitwise operations
    flags &= 0xFF00FF00;
    printf("After &= 0xFF00FF00: 0x%08X\n", flags.load());
    
    flags |= 0x0F0F0F0F;
    printf("After |= 0x0F0F0F0F: 0x%08X\n", flags.load());
    
    flags ^= 0x12345678;
    printf("After ^= 0x12345678: 0x%08X\n", flags.load());
}
```

## Conditional Atomic Types

```cpp
template<bool THREAD_SAFE>
class ConfigurableCounter {
    hshm::ipc::opt_atomic<int, THREAD_SAFE> count_;
    
public:
    ConfigurableCounter() : count_(0) {}
    
    void Increment() {
        count_.fetch_add(1);
    }
    
    void Add(int value) {
        count_.fetch_add(value);
    }
    
    int Get() const {
        return count_.load();
    }
    
    void Reset() {
        count_.store(0);
    }
};

void conditional_atomic_example() {
    // Thread-safe version
    ConfigurableCounter<true> thread_safe_counter;
    
    // Non-atomic version for single-threaded use
    ConfigurableCounter<false> fast_counter;
    
    const int iterations = 100000;
    
    // Test thread-safe version with multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&thread_safe_counter, iterations]() {
            for (int j = 0; j < iterations; ++j) {
                thread_safe_counter.Increment();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Test non-atomic version (single-threaded)
    for (int i = 0; i < 4 * iterations; ++i) {
        fast_counter.Increment();
    }
    
    printf("Thread-safe counter: %d\n", thread_safe_counter.Get());
    printf("Fast counter: %d\n", fast_counter.Get());
    printf("Both should equal: %d\n", 4 * iterations);
}
```

## Serialization Support

```cpp
#include <sstream>
#include <cereal/archives/binary.hpp>

void atomic_serialization_example() {
    hshm::ipc::atomic<int> counter(12345);
    hshm::ipc::nonatomic<double> value(3.14159);
    
    // Serialize to binary stream
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive archive(ss);
        archive(counter, value);
    }
    
    // Deserialize from binary stream
    hshm::ipc::atomic<int> loaded_counter;
    hshm::ipc::nonatomic<double> loaded_value;
    {
        cereal::BinaryInputArchive archive(ss);
        archive(loaded_counter, loaded_value);
    }
    
    printf("Original counter: %d, loaded: %d\n", 
           counter.load(), loaded_counter.load());
    printf("Original value: %f, loaded: %f\n", 
           value.load(), loaded_value.load());
}
```

## Best Practices

1. **Platform Selection**: Use `hshm::ipc::atomic<T>` for automatic platform selection (CPU vs GPU)
2. **Performance**: Use `nonatomic<T>` for single-threaded code or when external synchronization is provided
3. **Memory Ordering**: Specify appropriate memory ordering for performance-critical code
4. **GPU Compatibility**: Use HSHM atomic types for code that runs on both CPU and GPU
5. **Lock-Free Design**: Prefer atomic operations over locks for high-performance concurrent code
6. **Reference Counting**: Use atomic counters for thread-safe reference counting implementations
7. **Conditional Compilation**: Use `opt_atomic<T, bool>` for compile-time atomic vs non-atomic selection
8. **Cross-Platform**: All atomic types work consistently across different architectures and GPUs
9. **Serialization**: Atomic types support standard serialization for persistence and communication
10. **Testing**: Always test atomic code under high contention to verify correctness and performance

# HSHM Bitfield Types Guide

## Overview

The Bitfield Types API in Hermes Shared Memory (HSHM) provides efficient bit manipulation utilities with support for atomic operations, cross-device compatibility, and variable-length bitfields. These types enable compact storage of flags, permissions, and state information while providing convenient manipulation operations.

## Basic Bitfield Usage

### Standard Bitfield Operations

```cpp
#include "hermes_shm/types/bitfield.h"

void basic_bitfield_example() {
    // Create a 32-bit bitfield
    hshm::bitfield32_t flags;
    
    // Define some flag constants
    constexpr uint32_t FLAG_ENABLED    = BIT_OPT(uint32_t, 0);  // Bit 0: 0x1
    constexpr uint32_t FLAG_VISIBLE    = BIT_OPT(uint32_t, 1);  // Bit 1: 0x2
    constexpr uint32_t FLAG_ACTIVE     = BIT_OPT(uint32_t, 2);  // Bit 2: 0x4
    constexpr uint32_t FLAG_PERSISTENT = BIT_OPT(uint32_t, 3);  // Bit 3: 0x8
    
    // Set individual bits
    flags.SetBits(FLAG_ENABLED);
    flags.SetBits(FLAG_VISIBLE);
    
    // Set multiple bits at once
    flags.SetBits(FLAG_ACTIVE | FLAG_PERSISTENT);
    
    // Check if specific bits are set
    if (flags.Any(FLAG_ENABLED)) {
        printf("Object is enabled\n");
    }
    
    // Check if all specified bits are set
    if (flags.All(FLAG_ENABLED | FLAG_VISIBLE)) {
        printf("Object is enabled and visible\n");
    }
    
    // Unset specific bits
    flags.UnsetBits(FLAG_PERSISTENT);
    
    // Check individual bits
    bool is_active = flags.Any(FLAG_ACTIVE);
    bool is_persistent = flags.Any(FLAG_PERSISTENT);
    
    printf("Active: %s, Persistent: %s\n", 
           is_active ? "yes" : "no", 
           is_persistent ? "yes" : "no");
    
    // Clear all bits
    flags.Clear();
    
    printf("All flags cleared: %s\n", 
           flags.Any(ALL_BITS(uint32_t)) ? "no" : "yes");
}
```

### Different Bitfield Sizes

```cpp
void bitfield_sizes_example() {
    // Different sized bitfields
    hshm::bitfield8_t   small_flags;    // 8-bit
    hshm::bitfield16_t  medium_flags;   // 16-bit  
    hshm::bitfield32_t  large_flags;    // 32-bit
    hshm::bitfield64_t  huge_flags;     // 64-bit
    
    // Generic integer bitfield
    hshm::ibitfield int_flags;          // int-sized
    
    // Set some bits in each
    small_flags.SetBits(0x03);          // Set bits 0,1
    medium_flags.SetBits(0xFF00);       // Set bits 8-15
    large_flags.SetBits(0xAAAAAAAA);    // Alternating bits
    huge_flags.SetBits(0x123456789ABCDEFULL);
    
    printf("8-bit:  0x%02X\n", small_flags.bits_.load());
    printf("16-bit: 0x%04X\n", medium_flags.bits_.load());
    printf("32-bit: 0x%08X\n", large_flags.bits_.load());
    printf("64-bit: 0x%016lX\n", huge_flags.bits_.load());
}
```

### Bit Masking and Ranges

```cpp
void bitfield_masking_example() {
    hshm::bitfield32_t permissions;
    
    // Define permission masks using MakeMask
    uint32_t read_mask  = hshm::bitfield32_t::MakeMask(0, 3);  // Bits 0-2
    uint32_t write_mask = hshm::bitfield32_t::MakeMask(3, 3);  // Bits 3-5
    uint32_t exec_mask  = hshm::bitfield32_t::MakeMask(6, 3);  // Bits 6-8
    uint32_t owner_mask = hshm::bitfield32_t::MakeMask(9, 3);  // Bits 9-11
    
    printf("Permission masks:\n");
    printf("Read:  0x%03X (bits 0-2)\n", read_mask);
    printf("Write: 0x%03X (bits 3-5)\n", write_mask);
    printf("Exec:  0x%03X (bits 6-8)\n", exec_mask);
    printf("Owner: 0x%03X (bits 9-11)\n", owner_mask);
    
    // Set permissions for user, group, others
    permissions.SetBits(read_mask | write_mask | exec_mask);  // Owner: RWX
    permissions.SetBits(read_mask << 3);                      // Group: R--
    permissions.SetBits(read_mask << 6);                      // Others: R--
    
    // Check specific permission groups
    bool owner_can_read = permissions.Any(read_mask);
    bool group_can_write = permissions.Any(write_mask << 3);
    bool others_can_exec = permissions.Any(exec_mask << 6);
    
    printf("Owner can read: %s\n", owner_can_read ? "yes" : "no");
    printf("Group can write: %s\n", group_can_write ? "yes" : "no");  
    printf("Others can exec: %s\n", others_can_exec ? "yes" : "no");
    
    // Copy specific bits between bitfields
    hshm::bitfield32_t new_permissions;
    new_permissions.CopyBits(permissions, read_mask | exec_mask);
    
    printf("Copied R-X permissions: 0x%08X\n", new_permissions.bits_.load());
}
```

## Atomic Bitfield Operations

### Thread-Safe Bitfield Usage

```cpp
#include "hermes_shm/types/bitfield.h"
#include <thread>
#include <vector>

void atomic_bitfield_example() {
    // Atomic bitfield for thread-safe operations
    hshm::abitfield32_t shared_status;
    
    constexpr uint32_t WORKER_READY   = BIT_OPT(uint32_t, 0);
    constexpr uint32_t WORKER_BUSY    = BIT_OPT(uint32_t, 1);
    constexpr uint32_t WORKER_DONE    = BIT_OPT(uint32_t, 2);
    constexpr uint32_t SYSTEM_SHUTDOWN = BIT_OPT(uint32_t, 31);
    
    const int num_workers = 4;
    std::vector<std::thread> workers;
    
    // Launch worker threads
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back([&shared_status, i]() {
            // Signal worker is ready
            shared_status.SetBits(WORKER_READY);
            
            // Wait for all workers to be ready
            while (shared_status.bits_.load() & WORKER_READY != 
                   (WORKER_READY * num_workers)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Set busy flag and do work
            shared_status.SetBits(WORKER_BUSY);
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            shared_status.UnsetBits(WORKER_BUSY);
            
            // Signal completion
            shared_status.SetBits(WORKER_DONE);
            
            printf("Worker %d completed\n", i);
        });
    }
    
    // Monitor progress
    while (!shared_status.All(WORKER_DONE)) {
        uint32_t status = shared_status.bits_.load();
        int ready_count = __builtin_popcount(status & WORKER_READY);
        int busy_count = __builtin_popcount(status & WORKER_BUSY);
        int done_count = __builtin_popcount(status & WORKER_DONE);
        
        printf("Status - Ready: %d, Busy: %d, Done: %d\n", 
               ready_count, busy_count, done_count);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Signal shutdown
    shared_status.SetBits(SYSTEM_SHUTDOWN);
    
    // Wait for all workers
    for (auto& worker : workers) {
        worker.join();
    }
    
    printf("All workers completed. Final status: 0x%08X\n", 
           shared_status.bits_.load());
}
```

### Lock-Free Status Tracking

```cpp
class TaskManager {
    hshm::abitfield64_t task_status_;  // Track up to 64 tasks
    
public:
    bool StartTask(int task_id) {
        if (task_id >= 64) return false;
        
        uint64_t task_bit = BIT_OPT(uint64_t, task_id);
        
        // Check if task is already running
        if (task_status_.Any(task_bit)) {
            return false;  // Task already active
        }
        
        // Atomically set the task bit
        task_status_.SetBits(task_bit);
        return true;
    }
    
    void CompleteTask(int task_id) {
        if (task_id >= 64) return;
        
        uint64_t task_bit = BIT_OPT(uint64_t, task_id);
        task_status_.UnsetBits(task_bit);
    }
    
    bool IsTaskActive(int task_id) {
        if (task_id >= 64) return false;
        
        uint64_t task_bit = BIT_OPT(uint64_t, task_id);
        return task_status_.Any(task_bit);
    }
    
    int GetActiveTaskCount() {
        return __builtin_popcountll(task_status_.bits_.load());
    }
    
    std::vector<int> GetActiveTasks() {
        std::vector<int> active_tasks;
        uint64_t status = task_status_.bits_.load();
        
        for (int i = 0; i < 64; ++i) {
            if (status & BIT_OPT(uint64_t, i)) {
                active_tasks.push_back(i);
            }
        }
        
        return active_tasks;
    }
    
    void WaitForAllTasks() {
        while (task_status_.bits_.load() != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

void task_management_example() {
    TaskManager manager;
    std::vector<std::thread> workers;
    
    // Start multiple tasks
    for (int i = 0; i < 10; ++i) {
        if (manager.StartTask(i)) {
            workers.emplace_back([&manager, i]() {
                printf("Task %d started\n", i);
                
                // Simulate work
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 + i * 50));
                
                manager.CompleteTask(i);
                printf("Task %d completed\n", i);
            });
        }
    }
    
    // Monitor progress
    while (manager.GetActiveTaskCount() > 0) {
        auto active = manager.GetActiveTasks();
        printf("Active tasks: ");
        for (int task : active) {
            printf("%d ", task);
        }
        printf("(total: %d)\n", manager.GetActiveTaskCount());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Wait for completion
    for (auto& worker : workers) {
        worker.join();
    }
    
    printf("All tasks completed\n");
}
```

## Large Bitfields

### Variable-Length Bitfields

```cpp
void big_bitfield_example() {
    // Create a bitfield with 256 bits (8 x 32-bit words)
    hshm::big_bitfield<256> large_bitfield;
    
    printf("Bitfield size: %zu 32-bit words\n", large_bitfield.size());
    
    // Set a range of bits
    large_bitfield.SetBits(10, 20);  // Set 20 bits starting from bit 10
    
    // Check if any bits in range are set
    bool has_bits_30_40 = large_bitfield.Any(30, 10);
    bool has_bits_10_30 = large_bitfield.Any(10, 20);
    
    printf("Bits 30-39 set: %s\n", has_bits_30_40 ? "yes" : "no");
    printf("Bits 10-29 set: %s\n", has_bits_10_30 ? "yes" : "no");
    
    // Check if all bits in range are set
    bool all_bits_10_30 = large_bitfield.All(10, 20);
    printf("All bits 10-29 set: %s\n", all_bits_10_30 ? "yes" : "no");
    
    // Set specific patterns
    large_bitfield.SetBits(64, 32);   // Set bits 64-95 (entire second word)
    large_bitfield.SetBits(128, 64);  // Set bits 128-191 (third and fourth words)
    
    // Unset a range
    large_bitfield.UnsetBits(80, 16); // Unset bits 80-95
    
    // Clear entire bitfield
    large_bitfield.Clear();
    printf("Bitfield cleared\n");
}
```

### Custom-Sized Bitfields

```cpp
template<size_t NUM_NODES>
class NodeStatusTracker {
    hshm::big_bitfield<NUM_NODES> online_nodes_;
    hshm::big_bitfield<NUM_NODES> healthy_nodes_;
    hshm::big_bitfield<NUM_NODES> maintenance_nodes_;
    
public:
    void SetNodeOnline(size_t node_id) {
        if (node_id < NUM_NODES) {
            online_nodes_.SetBits(node_id, 1);
            printf("Node %zu is now online\n", node_id);
        }
    }
    
    void SetNodeOffline(size_t node_id) {
        if (node_id < NUM_NODES) {
            online_nodes_.UnsetBits(node_id, 1);
            healthy_nodes_.UnsetBits(node_id, 1);
            printf("Node %zu is now offline\n", node_id);
        }
    }
    
    void SetNodeHealthy(size_t node_id, bool healthy) {
        if (node_id < NUM_NODES) {
            if (healthy) {
                healthy_nodes_.SetBits(node_id, 1);
            } else {
                healthy_nodes_.UnsetBits(node_id, 1);
            }
            printf("Node %zu health: %s\n", node_id, healthy ? "good" : "bad");
        }
    }
    
    void SetNodeMaintenance(size_t node_id, bool in_maintenance) {
        if (node_id < NUM_NODES) {
            if (in_maintenance) {
                maintenance_nodes_.SetBits(node_id, 1);
                online_nodes_.UnsetBits(node_id, 1);  // Take offline
            } else {
                maintenance_nodes_.UnsetBits(node_id, 1);
            }
            printf("Node %zu maintenance: %s\n", node_id, 
                   in_maintenance ? "active" : "inactive");
        }
    }
    
    size_t GetAvailableNodeCount() {
        size_t count = 0;
        for (size_t i = 0; i < NUM_NODES; ++i) {
            if (online_nodes_.Any(i, 1) && 
                healthy_nodes_.Any(i, 1) && 
                !maintenance_nodes_.Any(i, 1)) {
                count++;
            }
        }
        return count;
    }
    
    std::vector<size_t> GetAvailableNodes() {
        std::vector<size_t> available;
        for (size_t i = 0; i < NUM_NODES; ++i) {
            if (online_nodes_.Any(i, 1) && 
                healthy_nodes_.Any(i, 1) && 
                !maintenance_nodes_.Any(i, 1)) {
                available.push_back(i);
            }
        }
        return available;
    }
    
    void PrintStatus() {
        printf("\n=== Cluster Status ===\n");
        printf("Total nodes: %zu\n", NUM_NODES);
        printf("Available nodes: %zu\n", GetAvailableNodeCount());
        
        auto available = GetAvailableNodes();
        printf("Available node IDs: ");
        for (size_t node : available) {
            printf("%zu ", node);
        }
        printf("\n");
    }
};

void cluster_monitoring_example() {
    // Track status of 1000 nodes
    NodeStatusTracker<1000> cluster;
    
    // Simulate bringing nodes online
    for (size_t i = 0; i < 100; ++i) {
        cluster.SetNodeOnline(i);
        cluster.SetNodeHealthy(i, true);
    }
    
    // Simulate some failures and maintenance
    cluster.SetNodeHealthy(10, false);
    cluster.SetNodeHealthy(25, false);
    cluster.SetNodeMaintenance(50, true);
    cluster.SetNodeMaintenance(75, true);
    
    cluster.PrintStatus();
}
```

## Bitfield Patterns and Best Practices

### State Machine Implementation

```cpp
enum class ProcessState : uint32_t {
    CREATED    = BIT_OPT(uint32_t, 0),  // 0x001
    RUNNING    = BIT_OPT(uint32_t, 1),  // 0x002
    SUSPENDED  = BIT_OPT(uint32_t, 2),  // 0x004
    ZOMBIE     = BIT_OPT(uint32_t, 3),  // 0x008
    TERMINATED = BIT_OPT(uint32_t, 4),  // 0x010
    
    // Composite states
    ACTIVE     = RUNNING | SUSPENDED,    // 0x006
    FINISHED   = ZOMBIE | TERMINATED,    // 0x018
};

class Process {
    hshm::abitfield32_t state_;
    int process_id_;
    
public:
    explicit Process(int pid) : process_id_(pid) {
        state_.SetBits(static_cast<uint32_t>(ProcessState::CREATED));
    }
    
    void Start() {
        if (state_.Any(static_cast<uint32_t>(ProcessState::CREATED))) {
            state_.UnsetBits(static_cast<uint32_t>(ProcessState::CREATED));
            state_.SetBits(static_cast<uint32_t>(ProcessState::RUNNING));
            printf("Process %d started\n", process_id_);
        }
    }
    
    void Suspend() {
        if (state_.Any(static_cast<uint32_t>(ProcessState::RUNNING))) {
            state_.UnsetBits(static_cast<uint32_t>(ProcessState::RUNNING));
            state_.SetBits(static_cast<uint32_t>(ProcessState::SUSPENDED));
            printf("Process %d suspended\n", process_id_);
        }
    }
    
    void Resume() {
        if (state_.Any(static_cast<uint32_t>(ProcessState::SUSPENDED))) {
            state_.UnsetBits(static_cast<uint32_t>(ProcessState::SUSPENDED));
            state_.SetBits(static_cast<uint32_t>(ProcessState::RUNNING));
            printf("Process %d resumed\n", process_id_);
        }
    }
    
    void Terminate() {
        if (state_.Any(static_cast<uint32_t>(ProcessState::ACTIVE))) {
            state_.Clear();
            state_.SetBits(static_cast<uint32_t>(ProcessState::TERMINATED));
            printf("Process %d terminated\n", process_id_);
        }
    }
    
    bool IsActive() const {
        return state_.Any(static_cast<uint32_t>(ProcessState::ACTIVE));
    }
    
    bool IsFinished() const {
        return state_.Any(static_cast<uint32_t>(ProcessState::FINISHED));
    }
    
    std::string GetStateString() const {
        uint32_t state = state_.bits_.load();
        
        if (state & static_cast<uint32_t>(ProcessState::CREATED))    return "CREATED";
        if (state & static_cast<uint32_t>(ProcessState::RUNNING))    return "RUNNING";
        if (state & static_cast<uint32_t>(ProcessState::SUSPENDED))  return "SUSPENDED";
        if (state & static_cast<uint32_t>(ProcessState::ZOMBIE))     return "ZOMBIE";
        if (state & static_cast<uint32_t>(ProcessState::TERMINATED)) return "TERMINATED";
        
        return "UNKNOWN";
    }
};

void state_machine_example() {
    Process proc(12345);
    
    printf("Initial state: %s\n", proc.GetStateString().c_str());
    
    proc.Start();
    printf("State: %s, Active: %s\n", 
           proc.GetStateString().c_str(), 
           proc.IsActive() ? "yes" : "no");
    
    proc.Suspend();
    printf("State: %s, Active: %s\n", 
           proc.GetStateString().c_str(), 
           proc.IsActive() ? "yes" : "no");
    
    proc.Resume();
    printf("State: %s, Active: %s\n", 
           proc.GetStateString().c_str(), 
           proc.IsActive() ? "yes" : "no");
    
    proc.Terminate();
    printf("State: %s, Finished: %s\n", 
           proc.GetStateString().c_str(), 
           proc.IsFinished() ? "yes" : "no");
}
```

### Feature Flag System

```cpp
class FeatureFlags {
    hshm::bitfield64_t enabled_features_;
    
public:
    enum Feature : uint64_t {
        ADVANCED_LOGGING    = BIT_OPT(uint64_t, 0),
        GPU_ACCELERATION    = BIT_OPT(uint64_t, 1),
        COMPRESSION         = BIT_OPT(uint64_t, 2),
        ENCRYPTION          = BIT_OPT(uint64_t, 3),
        CACHING             = BIT_OPT(uint64_t, 4),
        ASYNC_IO            = BIT_OPT(uint64_t, 5),
        METRICS_COLLECTION  = BIT_OPT(uint64_t, 6),
        DEBUG_MODE          = BIT_OPT(uint64_t, 7),
        EXPERIMENTAL_API    = BIT_OPT(uint64_t, 8),
        CLOUD_INTEGRATION   = BIT_OPT(uint64_t, 9),
        
        // Feature combinations
        PERFORMANCE_PACK    = GPU_ACCELERATION | COMPRESSION | ASYNC_IO,
        SECURITY_PACK       = ENCRYPTION,
        DEBUG_PACK          = ADVANCED_LOGGING | DEBUG_MODE | METRICS_COLLECTION,
    };
    
    void EnableFeature(Feature feature) {
        enabled_features_.SetBits(static_cast<uint64_t>(feature));
    }
    
    void DisableFeature(Feature feature) {
        enabled_features_.UnsetBits(static_cast<uint64_t>(feature));
    }
    
    bool IsFeatureEnabled(Feature feature) const {
        return enabled_features_.Any(static_cast<uint64_t>(feature));
    }
    
    void EnableFeaturePack(Feature pack) {
        enabled_features_.SetBits(static_cast<uint64_t>(pack));
    }
    
    void LoadFromConfig(const std::string& config_string) {
        // Parse config string format: "feature1,feature2,feature3"
        std::istringstream ss(config_string);
        std::string feature_name;
        
        enabled_features_.Clear();
        
        while (std::getline(ss, feature_name, ',')) {
            if (feature_name == "gpu")        EnableFeature(GPU_ACCELERATION);
            if (feature_name == "compress")   EnableFeature(COMPRESSION);
            if (feature_name == "encrypt")    EnableFeature(ENCRYPTION);
            if (feature_name == "cache")      EnableFeature(CACHING);
            if (feature_name == "async")      EnableFeature(ASYNC_IO);
            if (feature_name == "debug")      EnableFeaturePack(DEBUG_PACK);
            if (feature_name == "perf")       EnableFeaturePack(PERFORMANCE_PACK);
        }
    }
    
    std::string GetEnabledFeaturesString() const {
        std::vector<std::string> features;
        
        if (IsFeatureEnabled(ADVANCED_LOGGING))   features.push_back("logging");
        if (IsFeatureEnabled(GPU_ACCELERATION))   features.push_back("gpu");
        if (IsFeatureEnabled(COMPRESSION))        features.push_back("compression");
        if (IsFeatureEnabled(ENCRYPTION))         features.push_back("encryption");
        if (IsFeatureEnabled(CACHING))            features.push_back("caching");
        if (IsFeatureEnabled(ASYNC_IO))           features.push_back("async_io");
        if (IsFeatureEnabled(METRICS_COLLECTION)) features.push_back("metrics");
        if (IsFeatureEnabled(DEBUG_MODE))         features.push_back("debug");
        
        std::string result;
        for (size_t i = 0; i < features.size(); ++i) {
            if (i > 0) result += ", ";
            result += features[i];
        }
        return result;
    }
};

void feature_flags_example() {
    FeatureFlags flags;
    
    // Enable individual features
    flags.EnableFeature(FeatureFlags::GPU_ACCELERATION);
    flags.EnableFeature(FeatureFlags::COMPRESSION);
    
    printf("Enabled features: %s\n", flags.GetEnabledFeaturesString().c_str());
    
    // Enable feature pack
    flags.EnableFeaturePack(FeatureFlags::DEBUG_PACK);
    printf("After enabling debug pack: %s\n", flags.GetEnabledFeaturesString().c_str());
    
    // Load from configuration
    flags.LoadFromConfig("gpu,encrypt,cache,async");
    printf("From config: %s\n", flags.GetEnabledFeaturesString().c_str());
    
    // Check specific features in application code
    if (flags.IsFeatureEnabled(FeatureFlags::GPU_ACCELERATION)) {
        printf("Using GPU acceleration\n");
    }
    
    if (flags.IsFeatureEnabled(FeatureFlags::ENCRYPTION)) {
        printf("Encryption is enabled\n");
    }
}
```

## Serialization and Persistence

```cpp
#include <fstream>
#include <cereal/archives/binary.hpp>

void serialization_example() {
    // Create and configure bitfield
    hshm::bitfield32_t config_flags;
    config_flags.SetBits(0x12345678);
    
    // Serialize to file
    {
        std::ofstream os("bitfield.bin", std::ios::binary);
        cereal::BinaryOutputArchive archive(os);
        archive(config_flags);
    }
    
    // Deserialize from file
    hshm::bitfield32_t loaded_flags;
    {
        std::ifstream is("bitfield.bin", std::ios::binary);
        cereal::BinaryInputArchive archive(is);
        archive(loaded_flags);
    }
    
    printf("Original:  0x%08X\n", config_flags.bits_.load());
    printf("Loaded:    0x%08X\n", loaded_flags.bits_.load());
    printf("Match:     %s\n", 
           (config_flags.bits_.load() == loaded_flags.bits_.load()) ? "yes" : "no");
}
```

## Best Practices

1. **Use Atomic Variants**: Use `abitfield` types for shared data structures accessed by multiple threads
2. **Define Constants**: Always define named constants for bit positions instead of magic numbers
3. **Mask Operations**: Use `MakeMask()` for multi-bit fields and ranges
4. **Size Selection**: Choose appropriate bitfield size (8, 16, 32, 64 bits) based on your needs
5. **Large Bitfields**: Use `big_bitfield<N>` for bitfields larger than 64 bits
6. **Performance**: Bitfield operations are very fast, but atomic operations have some overhead
7. **Cross-Platform**: All bitfield types work consistently across different architectures
8. **Serialization**: Bitfields support standard serialization libraries for persistence
9. **State Machines**: Use bitfields for efficient state representation with composite states
10. **Feature Flags**: Implement feature toggle systems using bitfields for compact storage and fast checking

# HSHM Configuration Parsing Guide

## Overview

The Configuration Parsing API in Hermes Shared Memory (HSHM) provides powerful utilities for parsing configuration files, processing hostnames, and converting human-readable units. The `ConfigParse` class and `BaseConfig` abstract class form the foundation for flexible configuration management.

## Basic Configuration with YAML

### Creating a Configuration Class

```cpp
#include "hermes_shm/util/config_parse.h"
#include "yaml-cpp/yaml.h"

class ApplicationConfig : public hshm::BaseConfig {
public:
    // Configuration fields
    std::string server_address;
    int port;
    size_t buffer_size;
    double timeout_seconds;
    std::vector<std::string> allowed_hosts;
    std::map<std::string, std::string> features;
    
    // Required: Set default values
    void LoadDefault() override {
        server_address = "localhost";
        port = 8080;
        buffer_size = hshm::Unit<size_t>::Megabytes(1);
        timeout_seconds = 30.0;
        allowed_hosts.clear();
        features.clear();
    }
    
private:
    // Required: Parse YAML configuration
    void ParseYAML(YAML::Node &yaml_conf) override {
        if (yaml_conf["server"]) {
            auto server = yaml_conf["server"];
            if (server["address"]) {
                server_address = server["address"].as<std::string>();
            }
            if (server["port"]) {
                port = server["port"].as<int>();
            }
        }
        
        if (yaml_conf["buffer_size"]) {
            std::string size_str = yaml_conf["buffer_size"].as<std::string>();
            buffer_size = hshm::ConfigParse::ParseSize(size_str);
        }
        
        if (yaml_conf["timeout"]) {
            timeout_seconds = yaml_conf["timeout"].as<double>();
        }
        
        if (yaml_conf["allowed_hosts"]) {
            ParseHostList(yaml_conf["allowed_hosts"]);
        }
        
        if (yaml_conf["features"]) {
            for (auto it = yaml_conf["features"].begin(); 
                 it != yaml_conf["features"].end(); ++it) {
                features[it->first.as<std::string>()] = it->second.as<std::string>();
            }
        }
    }
    
    void ParseHostList(YAML::Node hosts_node) {
        allowed_hosts.clear();
        for (auto host_node : hosts_node) {
            std::string host_pattern = host_node.as<std::string>();
            // Expand hostname patterns
            hshm::ConfigParse::ParseHostNameString(host_pattern, allowed_hosts);
        }
    }
};
```

### Loading Configuration

```cpp
// Example YAML configuration file: config.yaml
/*
server:
  address: "0.0.0.0"
  port: 9090

buffer_size: "2GB"
timeout: 60.0

allowed_hosts:
  - "compute[01-10]-ib"
  - "storage[001-003]"
  - "login1;login2"

features:
  compression: "enabled"
  encryption: "aes256"
  cache_size: "512MB"
*/

ApplicationConfig config;

// Load from file with defaults
config.LoadFromFile("/path/to/config.yaml");

// Load from file without defaults
config.LoadFromFile("/path/to/config.yaml", false);

// Load from string
std::string yaml_content = R"(
server:
  address: "192.168.1.100"
  port: 8888
buffer_size: "512MB"
)";
config.LoadText(yaml_content);

// Access configuration values
printf("Server: %s:%d\n", config.server_address.c_str(), config.port);
printf("Buffer Size: %zu bytes\n", config.buffer_size);
printf("Hosts: %zu allowed\n", config.allowed_hosts.size());
```

## Hostname Parsing

### Basic Hostname Expansion

```cpp
std::vector<std::string> hosts;

// Simple range expansion
hshm::ConfigParse::ParseHostNameString("node[01-05]", hosts);
// Result: node01, node02, node03, node04, node05

// Multiple ranges with prefix and suffix
hosts.clear();
hshm::ConfigParse::ParseHostNameString("compute[001-003,010-012]-40g", hosts);
// Result: compute001-40g, compute002-40g, compute003-40g,
//         compute010-40g, compute011-40g, compute012-40g

// Semicolon separation for different patterns
hosts.clear();
hshm::ConfigParse::ParseHostNameString("gpu[01-02]-ib;cpu[01-03]-eth", hosts);
// Result: gpu01-ib, gpu02-ib, cpu01-eth, cpu02-eth, cpu03-eth

// Single values in ranges
hosts.clear();
hshm::ConfigParse::ParseHostNameString("special[1,5,9,10]", hosts);
// Result: special1, special5, special9, special10
```

### Advanced Hostname Patterns

```cpp
class ClusterConfig {
    std::vector<std::string> compute_nodes_;
    std::vector<std::string> storage_nodes_;
    std::vector<std::string> management_nodes_;
    
public:
    void ParseClusterTopology(const std::string& topology_file) {
        YAML::Node topology = YAML::LoadFile(topology_file);
        
        // Parse different node types with complex patterns
        if (topology["compute"]) {
            std::string pattern = topology["compute"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, compute_nodes_);
        }
        
        if (topology["storage"]) {
            std::string pattern = topology["storage"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, storage_nodes_);
        }
        
        if (topology["management"]) {
            std::string pattern = topology["management"].as<std::string>();
            hshm::ConfigParse::ParseHostNameString(pattern, management_nodes_);
        }
        
        DisplayTopology();
    }
    
    void DisplayTopology() {
        printf("Cluster Topology:\n");
        printf("  Compute Nodes (%zu):\n", compute_nodes_.size());
        for (size_t i = 0; i < std::min(size_t(5), compute_nodes_.size()); ++i) {
            printf("    %s\n", compute_nodes_[i].c_str());
        }
        if (compute_nodes_.size() > 5) {
            printf("    ... and %zu more\n", compute_nodes_.size() - 5);
        }
        
        printf("  Storage Nodes (%zu):\n", storage_nodes_.size());
        for (const auto& node : storage_nodes_) {
            printf("    %s\n", node.c_str());
        }
        
        printf("  Management Nodes (%zu):\n", management_nodes_.size());
        for (const auto& node : management_nodes_) {
            printf("    %s\n", node.c_str());
        }
    }
};

// Example topology.yaml:
/*
compute: "cn[001-128]-ib"
storage: "st[01-08]-40g"
management: "mgmt[1-2];login[1-2];scheduler"
*/
```

### Hostfile Processing

```cpp
// Parse a hostfile with multiple formats
std::vector<std::string> ParseHostfile(const std::string& hostfile_path) {
    std::vector<std::string> all_hosts = hshm::ConfigParse::ParseHostfile(hostfile_path);
    
    // Process and validate hosts
    std::vector<std::string> valid_hosts;
    for (const auto& host : all_hosts) {
        if (IsValidHostname(host)) {
            valid_hosts.push_back(host);
        } else {
            fprintf(stderr, "Warning: Invalid hostname '%s' skipped\n", host.c_str());
        }
    }
    
    return valid_hosts;
}

bool IsValidHostname(const std::string& hostname) {
    // Basic validation
    if (hostname.empty() || hostname.length() > 255) {
        return false;
    }
    
    // Check for valid characters
    for (char c : hostname) {
        if (!std::isalnum(c) && c != '-' && c != '.') {
            return false;
        }
    }
    
    return true;
}

// Example hostfile content:
/*
# Compute nodes
compute[001-064]-ib
compute[065-128]-ib

# GPU nodes  
gpu[01-16]-40g

# Special nodes
login1
login2
scheduler
storage[01-04]
*/
```

## Size and Unit Parsing

### Memory Size Parsing

```cpp
// Parse various memory size formats
size_t size1 = hshm::ConfigParse::ParseSize("1024");        // 1024 bytes
size_t size2 = hshm::ConfigParse::ParseSize("4K");          // 4 KB = 4096 bytes
size_t size3 = hshm::ConfigParse::ParseSize("4KB");         // 4 KB = 4096 bytes
size_t size4 = hshm::ConfigParse::ParseSize("2.5M");        // 2.5 MB
size_t size5 = hshm::ConfigParse::ParseSize("1.5GB");       // 1.5 GB
size_t size6 = hshm::ConfigParse::ParseSize("2T");          // 2 TB
size_t size7 = hshm::ConfigParse::ParseSize("0.5PB");       // 0.5 PB
size_t size_inf = hshm::ConfigParse::ParseSize("inf");      // Maximum size_t value

printf("Parsed sizes:\n");
printf("  4K = %zu bytes\n", size2);
printf("  2.5M = %zu bytes (%.2f MB)\n", size4, size4 / (1024.0 * 1024.0));
printf("  1.5GB = %zu bytes\n", size5);
printf("  inf = %zu (max value)\n", size_inf);
```

### Bandwidth Parsing

```cpp
// Parse bandwidth specifications (bytes per second)
size_t bw1 = hshm::ConfigParse::ParseBandwidth("100MB");    // 100 MB/s
size_t bw2 = hshm::ConfigParse::ParseBandwidth("10GB");     // 10 GB/s
size_t bw3 = hshm::ConfigParse::ParseBandwidth("1.5TB");    // 1.5 TB/s

// Note: ParseBandwidth currently treats input as bytes/second
// Additional parsing for "Gbps", "MB/s" etc. would need custom implementation
```

### Latency Parsing

```cpp
// Parse latency values (returns nanoseconds)
size_t lat1 = hshm::ConfigParse::ParseLatency("100n");      // 100 nanoseconds
size_t lat2 = hshm::ConfigParse::ParseLatency("50u");       // 50 microseconds
size_t lat3 = hshm::ConfigParse::ParseLatency("10m");       // 10 milliseconds  
size_t lat4 = hshm::ConfigParse::ParseLatency("1s");        // 1 second

printf("Latencies in nanoseconds:\n");
printf("  100n = %zu ns\n", lat1);
printf("  50u = %zu ns (%.3f μs)\n", lat2, lat2 / 1000.0);
printf("  10m = %zu ns (%.3f ms)\n", lat3, lat3 / 1000000.0);
printf("  1s = %zu ns (%.3f s)\n", lat4, lat4 / 1000000000.0);
```

### Custom Number Parsing

```cpp
// Parse numbers with generic types
int int_val = hshm::ConfigParse::ParseNumber<int>("42");
double double_val = hshm::ConfigParse::ParseNumber<double>("3.14159");
float float_val = hshm::ConfigParse::ParseNumber<float>("2.718");
long long_val = hshm::ConfigParse::ParseNumber<long>("1234567890");

// Special infinity value
double inf_double = hshm::ConfigParse::ParseNumber<double>("inf");
int inf_int = hshm::ConfigParse::ParseNumber<int>("inf");  // Returns INT_MAX

// Extract suffixes from number strings
std::string suffix1 = hshm::ConfigParse::ParseNumberSuffix("100MB");   // "MB"
std::string suffix2 = hshm::ConfigParse::ParseNumberSuffix("3.14");    // ""
std::string suffix3 = hshm::ConfigParse::ParseNumberSuffix("50ms");    // "ms"
std::string suffix4 = hshm::ConfigParse::ParseNumberSuffix("1.5GHz");  // "GHz"
```

## Path Expansion

### Environment Variable Expansion

```cpp
// Expand environment variables in paths
std::string ExpandConfigPath(const std::string& template_path) {
    return hshm::ConfigParse::ExpandPath(template_path);
}

// Examples
std::string home_config = ExpandConfigPath("${HOME}/.config/myapp");
std::string data_path = ExpandConfigPath("${XDG_DATA_HOME}/myapp/data");
std::string temp_file = ExpandConfigPath("${TMPDIR}/myapp_${USER}.tmp");

// Complex expansion with multiple variables
std::string complex = ExpandConfigPath(
    "${HOME}/.cache/${APPLICATION_NAME}-${VERSION}/data"
);

// Set up environment and expand
hshm::SystemInfo::Setenv("APP_ROOT", "/opt/myapp", 1);
hshm::SystemInfo::Setenv("APP_VERSION", "2.1.0", 1);
std::string app_config = ExpandConfigPath("${APP_ROOT}/config-${APP_VERSION}.yaml");
```

## Complex Configuration Example

### Distributed System Configuration

```cpp
class DistributedSystemConfig : public hshm::BaseConfig {
public:
    // Cluster configuration
    struct ClusterConfig {
        std::vector<std::string> nodes;
        std::string coordinator;
        int replication_factor;
    };
    
    // Storage configuration
    struct StorageConfig {
        size_t cache_size;
        size_t block_size;
        std::string data_directory;
        std::vector<std::string> storage_nodes;
    };
    
    // Network configuration
    struct NetworkConfig {
        size_t bandwidth_limit;
        size_t latency_ns;
        int port_range_start;
        int port_range_end;
    };
    
    ClusterConfig cluster;
    StorageConfig storage;
    NetworkConfig network;
    std::map<std::string, std::string> advanced_options;
    
    void LoadDefault() override {
        // Cluster defaults
        cluster.nodes.clear();
        cluster.coordinator = "localhost";
        cluster.replication_factor = 3;
        
        // Storage defaults
        storage.cache_size = hshm::Unit<size_t>::Gigabytes(1);
        storage.block_size = hshm::Unit<size_t>::Megabytes(1);
        storage.data_directory = "/var/lib/myapp";
        storage.storage_nodes.clear();
        
        // Network defaults
        network.bandwidth_limit = hshm::Unit<size_t>::Gigabytes(10);
        network.latency_ns = 1000000;  // 1ms
        network.port_range_start = 9000;
        network.port_range_end = 9100;
        
        advanced_options.clear();
    }
    
private:
    void ParseYAML(YAML::Node &yaml_conf) override {
        ParseCluster(yaml_conf["cluster"]);
        ParseStorage(yaml_conf["storage"]);
        ParseNetwork(yaml_conf["network"]);
        ParseAdvanced(yaml_conf["advanced"]);
    }
    
    void ParseCluster(YAML::Node node) {
        if (!node) return;
        
        if (node["nodes"]) {
            cluster.nodes.clear();
            for (auto n : node["nodes"]) {
                std::string pattern = n.as<std::string>();
                hshm::ConfigParse::ParseHostNameString(pattern, cluster.nodes);
            }
        }
        
        if (node["coordinator"]) {
            cluster.coordinator = node["coordinator"].as<std::string>();
        }
        
        if (node["replication_factor"]) {
            cluster.replication_factor = node["replication_factor"].as<int>();
        }
    }
    
    void ParseStorage(YAML::Node node) {
        if (!node) return;
        
        if (node["cache_size"]) {
            storage.cache_size = hshm::ConfigParse::ParseSize(
                node["cache_size"].as<std::string>());
        }
        
        if (node["block_size"]) {
            storage.block_size = hshm::ConfigParse::ParseSize(
                node["block_size"].as<std::string>());
        }
        
        if (node["data_directory"]) {
            storage.data_directory = hshm::ConfigParse::ExpandPath(
                node["data_directory"].as<std::string>());
        }
        
        if (node["storage_nodes"]) {
            storage.storage_nodes.clear();
            for (auto n : node["storage_nodes"]) {
                std::string pattern = n.as<std::string>();
                hshm::ConfigParse::ParseHostNameString(pattern, storage.storage_nodes);
            }
        }
    }
    
    void ParseNetwork(YAML::Node node) {
        if (!node) return;
        
        if (node["bandwidth_limit"]) {
            network.bandwidth_limit = hshm::ConfigParse::ParseBandwidth(
                node["bandwidth_limit"].as<std::string>());
        }
        
        if (node["latency"]) {
            network.latency_ns = hshm::ConfigParse::ParseLatency(
                node["latency"].as<std::string>());
        }
        
        if (node["port_range"]) {
            auto range = node["port_range"];
            if (range["start"]) {
                network.port_range_start = range["start"].as<int>();
            }
            if (range["end"]) {
                network.port_range_end = range["end"].as<int>();
            }
        }
    }
    
    void ParseAdvanced(YAML::Node node) {
        if (!node) return;
        
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            std::string value = it->second.as<std::string>();
            
            // Expand environment variables in values
            value = hshm::ConfigParse::ExpandPath(value);
            advanced_options[key] = value;
        }
    }
    
public:
    void DisplayConfiguration() {
        printf("=== Distributed System Configuration ===\n");
        
        printf("\nCluster:\n");
        printf("  Nodes: %zu total\n", cluster.nodes.size());
        for (size_t i = 0; i < std::min(size_t(3), cluster.nodes.size()); ++i) {
            printf("    - %s\n", cluster.nodes[i].c_str());
        }
        if (cluster.nodes.size() > 3) {
            printf("    ... and %zu more\n", cluster.nodes.size() - 3);
        }
        printf("  Coordinator: %s\n", cluster.coordinator.c_str());
        printf("  Replication: %d\n", cluster.replication_factor);
        
        printf("\nStorage:\n");
        printf("  Cache Size: %.2f GB\n", storage.cache_size / (1024.0*1024.0*1024.0));
        printf("  Block Size: %.2f MB\n", storage.block_size / (1024.0*1024.0));
        printf("  Data Dir: %s\n", storage.data_directory.c_str());
        printf("  Storage Nodes: %zu\n", storage.storage_nodes.size());
        
        printf("\nNetwork:\n");
        printf("  Bandwidth: %.2f GB/s\n", 
               network.bandwidth_limit / (1024.0*1024.0*1024.0));
        printf("  Latency: %.3f ms\n", network.latency_ns / 1000000.0);
        printf("  Port Range: %d-%d\n", 
               network.port_range_start, network.port_range_end);
        
        if (!advanced_options.empty()) {
            printf("\nAdvanced Options:\n");
            for (const auto& [key, value] : advanced_options) {
                printf("  %s: %s\n", key.c_str(), value.c_str());
            }
        }
    }
};
```

### Example Configuration File

```yaml
# distributed_system.yaml
cluster:
  nodes:
    - "compute[001-032]-ib"
    - "compute[033-064]-ib"
  coordinator: "master01"
  replication_factor: 3

storage:
  cache_size: "16GB"
  block_size: "4MB"
  data_directory: "${DATA_ROOT}/distributed_storage"
  storage_nodes:
    - "storage[01-08]-40g"

network:
  bandwidth_limit: "40GB"
  latency: "100us"
  port_range:
    start: 9000
    end: 9500

advanced:
  compression: "lz4"
  encryption: "aes256"
  log_directory: "${LOG_ROOT}/distributed_system"
  checkpoint_interval: "300s"
  max_connections: "1000"
```

## Vector Parsing Utilities

```cpp
// Using BaseConfig's vector parsing helpers
class VectorConfig : public hshm::BaseConfig {
public:
    std::vector<int> integers;
    std::vector<double> doubles;
    std::vector<std::string> strings;
    std::list<std::string> string_list;
    
    void LoadDefault() override {
        integers = {1, 2, 3};
        doubles = {1.0, 2.0, 3.0};
        strings = {"default1", "default2"};
        string_list.clear();
    }
    
private:
    void ParseYAML(YAML::Node &yaml_conf) override {
        // Parse and append to existing vector
        if (yaml_conf["integers"]) {
            ParseVector<int>(yaml_conf["integers"], integers);
        }
        
        // Clear and parse vector
        if (yaml_conf["doubles"]) {
            ClearParseVector<double>(yaml_conf["doubles"], doubles);
        }
        
        // Parse strings
        if (yaml_conf["strings"]) {
            ClearParseVector<std::string>(yaml_conf["strings"], strings);
        }
        
        // Works with other STL containers too
        if (yaml_conf["string_list"]) {
            ClearParseVector<std::string>(yaml_conf["string_list"], string_list);
        }
    }
};
```

## Best Practices

1. **Default Values**: Always implement `LoadDefault()` with sensible defaults
2. **Environment Variables**: Use `ExpandPath()` for all file paths to support `${VAR}` expansion
3. **Size Parsing**: Use `ParseSize()` for memory/storage values for human-readable configs
4. **Hostname Patterns**: Leverage range syntax `[start-end]` for cluster configurations
5. **Error Handling**: Wrap configuration loading in try-catch blocks
6. **Validation**: Validate parsed values against system capabilities and constraints
7. **Documentation**: Document all configuration options and their formats
8. **Type Safety**: Use appropriate parsing functions for each data type
9. **Modularity**: Split large configurations into logical sections
10. **Version Control**: Consider configuration versioning for backward compatibility

# HSHM Dynamic Libraries Guide

## Overview

The Dynamic Libraries API in Hermes Shared Memory (HSHM) provides cross-platform functionality for loading shared libraries at runtime, enabling plugin architectures and modular application design. This guide covers the `SharedLibrary` class and related patterns for dynamic library management.

## SharedLibrary Class

### Basic Library Loading

```cpp
#include "hermes_shm/introspect/system_info.h"

// Load a shared library
hshm::SharedLibrary math_lib("./libmymath.so");      // Linux
// hshm::SharedLibrary math_lib("libmymath.dylib");   // macOS
// hshm::SharedLibrary math_lib("mymath.dll");        // Windows

// Check if loading succeeded
if (!math_lib.IsNull()) {
    printf("Library loaded successfully\n");
} else {
    printf("Failed to load library: %s\n", math_lib.GetError().c_str());
}

// Load library with full path
hshm::SharedLibrary lib("/usr/local/lib/libcustom.so");

// Delayed loading
hshm::SharedLibrary delayed_lib;
// ... some time later ...
delayed_lib.Load("./plugins/myplugin.so");
```

### Getting Symbols

```cpp
// Get function pointer
typedef double (*calculate_fn)(double, double);
calculate_fn calculate = (calculate_fn)math_lib.GetSymbol("calculate");

if (calculate != nullptr) {
    double result = calculate(10.0, 20.0);
    printf("Calculation result: %f\n", result);
} else {
    printf("Function 'calculate' not found: %s\n", math_lib.GetError().c_str());
}

// Get global variable
int* library_version = (int*)math_lib.GetSymbol("library_version");
if (library_version != nullptr) {
    printf("Library version: %d\n", *library_version);
    *library_version = 42;  // Modify shared library global
}

// Get struct or class
struct LibraryInfo {
    char name[64];
    int major_version;
    int minor_version;
};

LibraryInfo* info = (LibraryInfo*)math_lib.GetSymbol("library_info");
if (info != nullptr) {
    printf("Library: %s v%d.%d\n", info->name, 
           info->major_version, info->minor_version);
}
```

### Error Handling

```cpp
class SafeLibraryLoader {
public:
    static bool LoadLibraryWithFallback(
        hshm::SharedLibrary& lib,
        const std::vector<std::string>& paths) {
        
        for (const auto& path : paths) {
            lib.Load(path);
            if (!lib.IsNull()) {
                printf("Loaded library from: %s\n", path.c_str());
                return true;
            }
            printf("Failed to load %s: %s\n", path.c_str(), lib.GetError().c_str());
        }
        
        return false;
    }
    
    static void* GetRequiredSymbol(
        hshm::SharedLibrary& lib,
        const std::string& symbol_name) {
        
        void* symbol = lib.GetSymbol(symbol_name);
        if (symbol == nullptr) {
            throw std::runtime_error(
                "Required symbol '" + symbol_name + "' not found: " + lib.GetError()
            );
        }
        return symbol;
    }
};

// Usage
hshm::SharedLibrary my_lib;
std::vector<std::string> search_paths = {
    "./libmylib.so",
    "/usr/local/lib/libmylib.so",
    "/usr/lib/libmylib.so"
};

if (SafeLibraryLoader::LoadLibraryWithFallback(my_lib, search_paths)) {
    try {
        auto init_fn = (void(*)())SafeLibraryLoader::GetRequiredSymbol(my_lib, "initialize");
        init_fn();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
```

## Plugin Architecture

### Plugin Interface Definition

```cpp
// plugin_interface.h - Shared between application and plugins
#pragma once

class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // Plugin identification
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual const char* GetDescription() const = 0;
    
    // Lifecycle
    virtual bool Initialize(void* context) = 0;
    virtual void Execute() = 0;
    virtual void Shutdown() = 0;
    
    // Optional capabilities
    virtual bool SupportsFeature(const char* feature) const { return false; }
    virtual void* GetInterface(const char* interface_name) { return nullptr; }
};

// Plugin factory function types
typedef IPlugin* (*CreatePluginFunc)();
typedef void (*DestroyPluginFunc)(IPlugin*);
typedef const char* (*GetPluginAPIVersionFunc)();

// Current plugin API version
#define PLUGIN_API_VERSION "1.0.0"
```

### Plugin Manager Implementation

```cpp
class PluginManager {
public:
    struct PluginInfo {
        std::string path;
        std::string name;
        std::string version;
        std::string description;
        bool enabled;
    };
    
private:
    struct LoadedPlugin {
        hshm::SharedLibrary library;
        IPlugin* instance;
        DestroyPluginFunc destroy_func;
        PluginInfo info;
        
        LoadedPlugin(hshm::SharedLibrary&& lib, IPlugin* inst, 
                    DestroyPluginFunc destroy, const PluginInfo& info)
            : library(std::move(lib)), instance(inst), 
              destroy_func(destroy), info(info) {}
    };
    
    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    std::map<std::string, size_t> plugin_index_;  // name -> index mapping
    void* app_context_;
    
public:
    explicit PluginManager(void* context = nullptr) : app_context_(context) {}
    
    bool LoadPlugin(const std::string& plugin_path) {
        printf("Loading plugin: %s\n", plugin_path.c_str());
        
        // Check if already loaded
        if (IsPluginLoaded(plugin_path)) {
            printf("Plugin already loaded: %s\n", plugin_path.c_str());
            return true;
        }
        
        // Load the library
        hshm::SharedLibrary lib(plugin_path);
        if (lib.IsNull()) {
            fprintf(stderr, "Failed to load plugin library: %s\n", 
                    lib.GetError().c_str());
            return false;
        }
        
        // Check API version
        if (!CheckAPIVersion(lib)) {
            fprintf(stderr, "Plugin API version mismatch\n");
            return false;
        }
        
        // Get factory functions
        CreatePluginFunc create = (CreatePluginFunc)lib.GetSymbol("CreatePlugin");
        DestroyPluginFunc destroy = (DestroyPluginFunc)lib.GetSymbol("DestroyPlugin");
        
        if (!create || !destroy) {
            fprintf(stderr, "Plugin missing required factory functions\n");
            return false;
        }
        
        // Create plugin instance
        IPlugin* plugin = create();
        if (!plugin) {
            fprintf(stderr, "Failed to create plugin instance\n");
            return false;
        }
        
        // Get plugin information
        PluginInfo info;
        info.path = plugin_path;
        info.name = plugin->GetName();
        info.version = plugin->GetVersion();
        info.description = plugin->GetDescription();
        info.enabled = false;
        
        // Initialize plugin
        if (!plugin->Initialize(app_context_)) {
            fprintf(stderr, "Plugin initialization failed: %s\n", info.name.c_str());
            destroy(plugin);
            return false;
        }
        
        info.enabled = true;
        printf("Plugin loaded successfully: %s v%s\n", 
               info.name.c_str(), info.version.c_str());
        printf("  Description: %s\n", info.description.c_str());
        
        // Store plugin
        size_t index = plugins_.size();
        plugin_index_[info.name] = index;
        plugins_.emplace_back(std::make_unique<LoadedPlugin>(
            std::move(lib), plugin, destroy, info));
        
        return true;
    }
    
    void LoadAllPlugins(const std::string& plugin_dir) {
        printf("Scanning for plugins in: %s\n", plugin_dir.c_str());
        
        std::vector<std::string> plugin_files = ScanPluginDirectory(plugin_dir);
        
        for (const auto& file : plugin_files) {
            LoadPlugin(file);
        }
        
        printf("Loaded %zu plugins\n", plugins_.size());
    }
    
    void ExecutePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            auto& plugin = plugins_[it->second];
            if (plugin->info.enabled) {
                printf("Executing plugin: %s\n", plugin_name.c_str());
                plugin->instance->Execute();
            } else {
                printf("Plugin %s is disabled\n", plugin_name.c_str());
            }
        } else {
            printf("Plugin not found: %s\n", plugin_name.c_str());
        }
    }
    
    void ExecuteAllPlugins() {
        for (auto& loaded : plugins_) {
            if (loaded->info.enabled) {
                printf("Executing plugin: %s\n", loaded->info.name.c_str());
                loaded->instance->Execute();
            }
        }
    }
    
    void DisablePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            plugins_[it->second]->info.enabled = false;
            printf("Plugin disabled: %s\n", plugin_name.c_str());
        }
    }
    
    void EnablePlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            plugins_[it->second]->info.enabled = true;
            printf("Plugin enabled: %s\n", plugin_name.c_str());
        }
    }
    
    std::vector<PluginInfo> GetPluginList() const {
        std::vector<PluginInfo> list;
        for (const auto& loaded : plugins_) {
            list.push_back(loaded->info);
        }
        return list;
    }
    
    IPlugin* GetPlugin(const std::string& plugin_name) {
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            return plugins_[it->second]->instance;
        }
        return nullptr;
    }
    
    ~PluginManager() {
        // Clean shutdown of all plugins
        for (auto& loaded : plugins_) {
            printf("Shutting down plugin: %s\n", loaded->info.name.c_str());
            loaded->instance->Shutdown();
            loaded->destroy_func(loaded->instance);
        }
    }
    
private:
    bool CheckAPIVersion(hshm::SharedLibrary& lib) {
        GetPluginAPIVersionFunc get_version = 
            (GetPluginAPIVersionFunc)lib.GetSymbol("GetPluginAPIVersion");
        
        if (get_version) {
            const char* version = get_version();
            if (strcmp(version, PLUGIN_API_VERSION) != 0) {
                fprintf(stderr, "API version mismatch: expected %s, got %s\n",
                        PLUGIN_API_VERSION, version);
                return false;
            }
        }
        return true;
    }
    
    bool IsPluginLoaded(const std::string& path) {
        for (const auto& loaded : plugins_) {
            if (loaded->info.path == path) {
                return true;
            }
        }
        return false;
    }
    
    std::vector<std::string> ScanPluginDirectory(const std::string& dir) {
        std::vector<std::string> plugin_files;
        
#ifdef __linux__
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                std::string filename = entry->d_name;
                if (filename.find(".so") != std::string::npos) {
                    plugin_files.push_back(dir + "/" + filename);
                }
            }
            closedir(d);
        }
#elif __APPLE__
        // Scan for .dylib files on macOS
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = readdir(d)) != nullptr) {
                std::string filename = entry->d_name;
                if (filename.find(".dylib") != std::string::npos) {
                    plugin_files.push_back(dir + "/" + filename);
                }
            }
            closedir(d);
        }
#elif _WIN32
        // Scan for .dll files on Windows
        std::string pattern = dir + "\\*.dll";
        WIN32_FIND_DATA fd;
        HANDLE hFind = FindFirstFile(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                plugin_files.push_back(dir + "\\" + fd.cFileName);
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
#endif
        
        return plugin_files;
    }
};
```

### Example Plugin Implementation

```cpp
// myplugin.cpp - Compile as shared library
#include "plugin_interface.h"
#include <cstring>

class MyPlugin : public IPlugin {
    std::string name_ = "MyPlugin";
    std::string version_ = "1.0.0";
    std::string description_ = "Example plugin implementation";
    void* app_context_;
    
public:
    const char* GetName() const override { 
        return name_.c_str(); 
    }
    
    const char* GetVersion() const override { 
        return version_.c_str(); 
    }
    
    const char* GetDescription() const override { 
        return description_.c_str(); 
    }
    
    bool Initialize(void* context) override {
        printf("MyPlugin: Initializing...\n");
        app_context_ = context;
        
        // Perform initialization
        if (!LoadConfiguration()) {
            return false;
        }
        
        if (!AllocateResources()) {
            return false;
        }
        
        printf("MyPlugin: Initialization complete\n");
        return true;
    }
    
    void Execute() override {
        printf("MyPlugin: Executing main functionality\n");
        
        // Perform plugin work
        ProcessData();
        GenerateOutput();
    }
    
    void Shutdown() override {
        printf("MyPlugin: Cleaning up resources\n");
        
        // Clean up resources
        FreeResources();
    }
    
    bool SupportsFeature(const char* feature) const override {
        // Check for specific features
        if (strcmp(feature, "data_processing") == 0) return true;
        if (strcmp(feature, "report_generation") == 0) return true;
        return false;
    }
    
    void* GetInterface(const char* interface_name) override {
        // Return specialized interfaces
        if (strcmp(interface_name, "IDataProcessor") == 0) {
            return static_cast<IDataProcessor*>(this);
        }
        return nullptr;
    }
    
private:
    bool LoadConfiguration() {
        // Load plugin-specific configuration
        return true;
    }
    
    bool AllocateResources() {
        // Allocate necessary resources
        return true;
    }
    
    void FreeResources() {
        // Free allocated resources
    }
    
    void ProcessData() {
        // Main processing logic
    }
    
    void GenerateOutput() {
        // Generate output/reports
    }
};

// Factory functions (must be extern "C" to prevent name mangling)
extern "C" {
    IPlugin* CreatePlugin() {
        return new MyPlugin();
    }
    
    void DestroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
    
    const char* GetPluginAPIVersion() {
        return PLUGIN_API_VERSION;
    }
}
```

## Cross-Platform Library Loading

### Platform-Agnostic Loader

```cpp
class CrossPlatformLoader {
public:
    static std::string GetLibraryExtension() {
#ifdef _WIN32
        return ".dll";
#elif __APPLE__
        return ".dylib";
#else
        return ".so";
#endif
    }
    
    static std::string GetLibraryPrefix() {
#ifdef _WIN32
        return "";  // No prefix on Windows
#else
        return "lib";  // Unix convention
#endif
    }
    
    static std::string MakeLibraryName(const std::string& base_name) {
        return GetLibraryPrefix() + base_name + GetLibraryExtension();
    }
    
    static std::string GetSystemLibraryPath() {
#ifdef _WIN32
        return "C:\\Windows\\System32";
#elif __APPLE__
        return "/usr/lib:/usr/local/lib";
#else
        return "/usr/lib:/usr/local/lib:/lib";
#endif
    }
    
    static bool LoadLibrary(const std::string& base_name, 
                          hshm::SharedLibrary& lib) {
        // Build search paths
        std::vector<std::string> search_paths = BuildSearchPaths(base_name);
        
        // Try to load from each path
        for (const auto& path : search_paths) {
            lib.Load(path);
            
            if (!lib.IsNull()) {
                printf("Loaded library from: %s\n", path.c_str());
                return true;
            }
        }
        
        fprintf(stderr, "Failed to find library: %s\n", base_name.c_str());
        return false;
    }
    
private:
    static std::vector<std::string> BuildSearchPaths(const std::string& base_name) {
        std::vector<std::string> paths;
        std::string lib_name = MakeLibraryName(base_name);
        
        // Current directory
        paths.push_back("./" + lib_name);
        
        // Application library directory
        std::string app_lib = hshm::SystemInfo::Getenv("APP_LIB_DIR");
        if (!app_lib.empty()) {
            paths.push_back(app_lib + "/" + lib_name);
        }
        
        // LD_LIBRARY_PATH / DYLD_LIBRARY_PATH / PATH
#ifdef _WIN32
        std::string env_path = hshm::SystemInfo::Getenv("PATH");
#elif __APPLE__
        std::string env_path = hshm::SystemInfo::Getenv("DYLD_LIBRARY_PATH");
#else
        std::string env_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
#endif
        
        if (!env_path.empty()) {
            AddPathsFromEnvironment(env_path, lib_name, paths);
        }
        
        // System paths
        AddSystemPaths(lib_name, paths);
        
        return paths;
    }
    
    static void AddPathsFromEnvironment(const std::string& env_path,
                                       const std::string& lib_name,
                                       std::vector<std::string>& paths) {
        std::stringstream ss(env_path);
        std::string path;
        
#ifdef _WIN32
        const char delimiter = ';';
#else
        const char delimiter = ':';
#endif
        
        while (std::getline(ss, path, delimiter)) {
            if (!path.empty()) {
                paths.push_back(path + "/" + lib_name);
            }
        }
    }
    
    static void AddSystemPaths(const std::string& lib_name,
                              std::vector<std::string>& paths) {
#ifdef _WIN32
        paths.push_back("C:\\Windows\\System32\\" + lib_name);
        paths.push_back("C:\\Windows\\SysWOW64\\" + lib_name);
#elif __APPLE__
        paths.push_back("/usr/local/lib/" + lib_name);
        paths.push_back("/usr/lib/" + lib_name);
        paths.push_back("/opt/homebrew/lib/" + lib_name);  // Apple Silicon
#else
        paths.push_back("/usr/local/lib/" + lib_name);
        paths.push_back("/usr/lib/" + lib_name);
        paths.push_back("/lib/" + lib_name);
        paths.push_back("/usr/lib/x86_64-linux-gnu/" + lib_name);  // Debian/Ubuntu
#endif
    }
};
```

### Version-Aware Loading

```cpp
class VersionedLibraryLoader {
public:
    struct Version {
        int major;
        int minor;
        int patch;
        
        std::string ToString() const {
            return std::to_string(major) + "." + 
                   std::to_string(minor) + "." + 
                   std::to_string(patch);
        }
    };
    
    static bool LoadVersionedLibrary(const std::string& base_name,
                                    const Version& min_version,
                                    hshm::SharedLibrary& lib) {
        // Try exact version first
        std::string versioned_name = base_name + "-" + min_version.ToString();
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try major.minor version
        versioned_name = base_name + "-" + 
                        std::to_string(min_version.major) + "." + 
                        std::to_string(min_version.minor);
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try major version only
        versioned_name = base_name + "-" + std::to_string(min_version.major);
        if (CrossPlatformLoader::LoadLibrary(versioned_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        // Try unversioned
        if (CrossPlatformLoader::LoadLibrary(base_name, lib)) {
            if (CheckVersion(lib, min_version)) {
                return true;
            }
        }
        
        return false;
    }
    
private:
    static bool CheckVersion(hshm::SharedLibrary& lib, const Version& min_version) {
        typedef void (*GetVersionFunc)(int*, int*, int*);
        GetVersionFunc get_version = (GetVersionFunc)lib.GetSymbol("GetLibraryVersion");
        
        if (get_version) {
            Version lib_version;
            get_version(&lib_version.major, &lib_version.minor, &lib_version.patch);
            
            if (lib_version.major > min_version.major) return true;
            if (lib_version.major < min_version.major) return false;
            
            if (lib_version.minor > min_version.minor) return true;
            if (lib_version.minor < min_version.minor) return false;
            
            return lib_version.patch >= min_version.patch;
        }
        
        // No version function, assume compatible
        return true;
    }
};
```

## Advanced Plugin Features

### Hot-Reloading Plugins

```cpp
class HotReloadablePluginManager : public PluginManager {
    std::map<std::string, std::time_t> plugin_timestamps_;
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_;
    
public:
    void StartHotReload(int check_interval_seconds = 5) {
        monitoring_ = true;
        monitor_thread_ = std::thread([this, check_interval_seconds]() {
            MonitorPlugins(check_interval_seconds);
        });
    }
    
    void StopHotReload() {
        monitoring_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
    
private:
    void MonitorPlugins(int interval) {
        while (monitoring_) {
            CheckForUpdates();
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }
    
    void CheckForUpdates() {
        auto plugin_list = GetPluginList();
        
        for (const auto& info : plugin_list) {
            struct stat st;
            if (stat(info.path.c_str(), &st) == 0) {
                auto it = plugin_timestamps_.find(info.path);
                if (it != plugin_timestamps_.end()) {
                    if (st.st_mtime > it->second) {
                        printf("Plugin %s has been updated, reloading...\n", 
                               info.name.c_str());
                        ReloadPlugin(info.name);
                        plugin_timestamps_[info.path] = st.st_mtime;
                    }
                } else {
                    plugin_timestamps_[info.path] = st.st_mtime;
                }
            }
        }
    }
    
    void ReloadPlugin(const std::string& plugin_name) {
        // Find and unload the plugin
        auto it = plugin_index_.find(plugin_name);
        if (it != plugin_index_.end()) {
            auto& plugin = plugins_[it->second];
            std::string path = plugin->info.path;
            
            // Shutdown and destroy
            plugin->instance->Shutdown();
            plugin->destroy_func(plugin->instance);
            
            // Remove from list
            plugins_.erase(plugins_.begin() + it->second);
            plugin_index_.erase(it);
            
            // Reload
            LoadPlugin(path);
        }
    }
};
```

## Complete Example: Extensible Application

```cpp
#include "hermes_shm/introspect/system_info.h"
#include <iostream>
#include <memory>

class ExtensibleApplication {
    std::unique_ptr<PluginManager> plugin_manager_;
    std::string plugin_directory_;
    
public:
    ExtensibleApplication() {
        plugin_manager_ = std::make_unique<PluginManager>(this);
        plugin_directory_ = GetPluginDirectory();
    }
    
    int Run(int argc, char* argv[]) {
        try {
            // Initialize application
            if (!Initialize()) {
                return 1;
            }
            
            // Load plugins
            LoadPlugins();
            
            // Display loaded plugins
            DisplayPlugins();
            
            // Execute plugins
            ExecutePlugins();
            
            // Run main application loop
            return MainLoop();
            
        } catch (const std::exception& e) {
            std::cerr << "Application error: " << e.what() << std::endl;
            return 1;
        }
    }
    
private:
    bool Initialize() {
        printf("Initializing extensible application...\n");
        
        // Set up plugin environment
        SetupPluginEnvironment();
        
        return true;
    }
    
    void SetupPluginEnvironment() {
        // Add plugin directory to library path
        std::string ld_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
        if (!ld_path.empty()) {
            ld_path = plugin_directory_ + ":" + ld_path;
        } else {
            ld_path = plugin_directory_;
        }
        hshm::SystemInfo::Setenv("LD_LIBRARY_PATH", ld_path, 1);
        
        // Set plugin-specific environment
        hshm::SystemInfo::Setenv("PLUGIN_API_VERSION", PLUGIN_API_VERSION, 1);
        hshm::SystemInfo::Setenv("APP_PLUGIN_DIR", plugin_directory_, 1);
    }
    
    std::string GetPluginDirectory() {
        // Check environment variable
        std::string dir = hshm::SystemInfo::Getenv("APP_PLUGIN_DIR");
        if (!dir.empty()) {
            return dir;
        }
        
        // Check relative to executable
        std::string exe_dir = GetExecutableDirectory();
        if (!exe_dir.empty()) {
            return exe_dir + "/plugins";
        }
        
        // Default
        return "./plugins";
    }
    
    std::string GetExecutableDirectory() {
#ifdef __linux__
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) {
            path[len] = '\0';
            std::string exe_path(path);
            return exe_path.substr(0, exe_path.find_last_of('/'));
        }
#endif
        return "";
    }
    
    void LoadPlugins() {
        printf("Loading plugins from: %s\n", plugin_directory_.c_str());
        
        // Load all plugins from directory
        plugin_manager_->LoadAllPlugins(plugin_directory_);
        
        // Load specific required plugins
        LoadRequiredPlugin("core_plugin");
        LoadRequiredPlugin("ui_plugin");
    }
    
    void LoadRequiredPlugin(const std::string& plugin_name) {
        if (!plugin_manager_->GetPlugin(plugin_name)) {
            std::string plugin_file = plugin_directory_ + "/" + 
                CrossPlatformLoader::MakeLibraryName(plugin_name);
            
            if (!plugin_manager_->LoadPlugin(plugin_file)) {
                fprintf(stderr, "Required plugin %s not found\n", plugin_name.c_str());
            }
        }
    }
    
    void DisplayPlugins() {
        auto plugins = plugin_manager_->GetPluginList();
        
        printf("\nLoaded Plugins (%zu):\n", plugins.size());
        printf("%-20s %-10s %-10s %s\n", "Name", "Version", "Status", "Description");
        printf("%-20s %-10s %-10s %s\n", "----", "-------", "------", "-----------");
        
        for (const auto& info : plugins) {
            printf("%-20s %-10s %-10s %s\n",
                   info.name.c_str(),
                   info.version.c_str(),
                   info.enabled ? "Enabled" : "Disabled",
                   info.description.c_str());
        }
        printf("\n");
    }
    
    void ExecutePlugins() {
        printf("Executing all enabled plugins...\n");
        plugin_manager_->ExecuteAllPlugins();
    }
    
    int MainLoop() {
        printf("Application running. Press 'q' to quit.\n");
        
        char command;
        while (std::cin >> command) {
            if (command == 'q') {
                break;
            } else if (command == 'r') {
                // Reload plugins
                LoadPlugins();
                DisplayPlugins();
            } else if (command == 'e') {
                // Execute plugins
                ExecutePlugins();
            } else if (command == 'l') {
                // List plugins
                DisplayPlugins();
            }
        }
        
        printf("Application shutting down...\n");
        return 0;
    }
};

int main(int argc, char* argv[]) {
    ExtensibleApplication app;
    return app.Run(argc, argv);
}
```

## Best Practices

1. **Error Handling**: Always check `IsNull()` and use `GetError()` for diagnostics
2. **Symbol Verification**: Verify function pointers are not null before calling
3. **Name Mangling**: Use `extern "C"` for plugin factory functions to prevent C++ name mangling
4. **RAII Pattern**: Use move semantics and automatic cleanup via destructors
5. **Version Checking**: Implement API version checking for plugin compatibility
6. **Search Paths**: Implement flexible library search paths for deployment flexibility
7. **Platform Abstraction**: Use wrapper functions to handle platform differences
8. **Resource Management**: Ensure plugins properly clean up resources in shutdown
9. **Thread Safety**: Consider thread safety when loading/unloading plugins
10. **Documentation**: Document plugin interfaces thoroughly for third-party developers

# HSHM Environment Variables Guide

## Overview

The Environment Variables API in Hermes Shared Memory (HSHM) provides cross-platform functionality for managing environment variables, enabling runtime configuration and dynamic application behavior. This guide covers the `SystemInfo` class methods for environment variable operations.

## Basic Environment Operations

### Getting Environment Variables

```cpp
#include "hermes_shm/introspect/system_info.h"

// Get environment variables with optional size limits
std::string home_dir = hshm::SystemInfo::Getenv("HOME");
std::string path = hshm::SystemInfo::Getenv("PATH", hshm::Unit<size_t>::Kilobytes(64));
std::string user = hshm::SystemInfo::Getenv("USER");

// Check if variable exists
std::string config_path = hshm::SystemInfo::Getenv("MY_APP_CONFIG");
if (config_path.empty()) {
    printf("MY_APP_CONFIG not set, using default\n");
    config_path = "/etc/myapp/default.conf";
}

// Get with size limit (important for potentially large variables)
size_t max_size = hshm::Unit<size_t>::Megabytes(1);
std::string large_var = hshm::SystemInfo::Getenv("LARGE_DATA", max_size);
```

### Setting Environment Variables

```cpp
// Set environment variables with overwrite flag
hshm::SystemInfo::Setenv("MY_APP_VERSION", "2.1.0", 1);        // overwrite=1 (always set)
hshm::SystemInfo::Setenv("MY_APP_DEBUG", "true", 0);           // overwrite=0 (don't overwrite if exists)
hshm::SystemInfo::Setenv("MY_APP_LOG_LEVEL", "INFO", 1);

// Setting paths
std::string app_home = "/opt/myapp";
hshm::SystemInfo::Setenv("MY_APP_HOME", app_home, 1);
hshm::SystemInfo::Setenv("MY_APP_CONFIG", app_home + "/config", 1);
hshm::SystemInfo::Setenv("MY_APP_DATA", app_home + "/data", 1);

// Setting numeric values
hshm::SystemInfo::Setenv("MAX_THREADS", std::to_string(8), 1);
hshm::SystemInfo::Setenv("BUFFER_SIZE", std::to_string(1024*1024), 1);
```

### Unsetting Environment Variables

```cpp
// Remove environment variables
hshm::SystemInfo::Unsetenv("TEMP_VAR");
hshm::SystemInfo::Unsetenv("OLD_CONFIG");
hshm::SystemInfo::Unsetenv("DEPRECATED_OPTION");

// Clean up temporary variables
std::vector<std::string> temp_vars = {
    "TMP_BUILD_DIR",
    "TMP_CACHE",
    "TMP_SESSION_ID"
};

for (const auto& var : temp_vars) {
    hshm::SystemInfo::Unsetenv(var.c_str());
}
```

## Configuration from Environment

### Application Configuration Class

```cpp
class AppConfiguration {
private:
    std::string config_dir_;
    std::string data_dir_;
    std::string log_file_;
    int log_level_;
    bool debug_mode_;
    size_t max_memory_;
    int thread_count_;
    
public:
    void LoadFromEnvironment() {
        // Configuration directory with XDG compliance
        config_dir_ = GetConfigDirectory();
        
        // Data directory with fallback chain
        data_dir_ = GetDataDirectory();
        
        // Logging configuration
        ConfigureLogging();
        
        // Runtime parameters
        ConfigureRuntime();
        
        // Display loaded configuration
        DisplayConfiguration();
    }
    
private:
    std::string GetConfigDirectory() {
        // Priority: APP_CONFIG_DIR > XDG_CONFIG_HOME > HOME/.config
        std::string dir = hshm::SystemInfo::Getenv("APP_CONFIG_DIR");
        if (!dir.empty()) return dir;
        
        dir = hshm::SystemInfo::Getenv("XDG_CONFIG_HOME");
        if (!dir.empty()) return dir + "/myapp";
        
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (!home.empty()) return home + "/.config/myapp";
        
        return "/etc/myapp";  // System fallback
    }
    
    std::string GetDataDirectory() {
        // Priority: APP_DATA_DIR > XDG_DATA_HOME > HOME/.local/share
        std::string dir = hshm::SystemInfo::Getenv("APP_DATA_DIR");
        if (!dir.empty()) return dir;
        
        dir = hshm::SystemInfo::Getenv("XDG_DATA_HOME");
        if (!dir.empty()) return dir + "/myapp";
        
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (!home.empty()) return home + "/.local/share/myapp";
        
        return "/var/lib/myapp";  // System fallback
    }
    
    void ConfigureLogging() {
        // Log file location
        log_file_ = hshm::SystemInfo::Getenv("APP_LOG_FILE");
        if (log_file_.empty()) {
            std::string log_dir = hshm::SystemInfo::Getenv("APP_LOG_DIR");
            if (log_dir.empty()) {
                log_dir = "/var/log";
            }
            log_file_ = log_dir + "/myapp.log";
        }
        
        // Log level parsing
        std::string level_str = hshm::SystemInfo::Getenv("APP_LOG_LEVEL");
        log_level_ = ParseLogLevel(level_str);
        
        // Debug mode
        std::string debug_str = hshm::SystemInfo::Getenv("APP_DEBUG");
        debug_mode_ = IsTrue(debug_str);
    }
    
    void ConfigureRuntime() {
        // Memory limit
        std::string mem_str = hshm::SystemInfo::Getenv("APP_MAX_MEMORY");
        if (!mem_str.empty()) {
            max_memory_ = ParseSize(mem_str);
        } else {
            max_memory_ = hshm::Unit<size_t>::Gigabytes(1);  // Default 1GB
        }
        
        // Thread count
        std::string thread_str = hshm::SystemInfo::Getenv("APP_THREADS");
        if (!thread_str.empty()) {
            thread_count_ = std::stoi(thread_str);
        } else {
            thread_count_ = std::thread::hardware_concurrency();
        }
    }
    
    int ParseLogLevel(const std::string& level) {
        if (level == "ERROR" || level == "0") return 0;
        if (level == "WARNING" || level == "1") return 1;
        if (level == "INFO" || level == "2") return 2;
        if (level == "DEBUG" || level == "3") return 3;
        if (level == "TRACE" || level == "4") return 4;
        return 2;  // Default to INFO
    }
    
    bool IsTrue(const std::string& value) {
        return value == "1" || value == "true" || 
               value == "TRUE" || value == "yes" || 
               value == "YES" || value == "on" || value == "ON";
    }
    
    size_t ParseSize(const std::string& size_str) {
        // Simple size parsing (enhance as needed)
        size_t value = std::stoull(size_str);
        if (size_str.find("K") != std::string::npos) value *= 1024;
        if (size_str.find("M") != std::string::npos) value *= 1024*1024;
        if (size_str.find("G") != std::string::npos) value *= 1024*1024*1024;
        return value;
    }
    
    void DisplayConfiguration() {
        printf("Application Configuration (from environment):\n");
        printf("  Config Dir: %s\n", config_dir_.c_str());
        printf("  Data Dir: %s\n", data_dir_.c_str());
        printf("  Log File: %s\n", log_file_.c_str());
        printf("  Log Level: %d\n", log_level_);
        printf("  Debug Mode: %s\n", debug_mode_ ? "enabled" : "disabled");
        printf("  Max Memory: %zu MB\n", max_memory_ / (1024*1024));
        printf("  Thread Count: %d\n", thread_count_);
    }
    
public:
    // Getters for configuration values
    const std::string& GetConfigDir() const { return config_dir_; }
    const std::string& GetDataDir() const { return data_dir_; }
    const std::string& GetLogFile() const { return log_file_; }
    int GetLogLevel() const { return log_level_; }
    bool IsDebugMode() const { return debug_mode_; }
    size_t GetMaxMemory() const { return max_memory_; }
    int GetThreadCount() const { return thread_count_; }
};
```

## Environment Variable Expansion

### Basic Variable Expansion

```cpp
class EnvironmentExpander {
public:
    // Expand ${VAR} patterns in strings
    static std::string ExpandVariables(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_name = result.substr(pos + 2, end - pos - 2);
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
    
    // Expand $VAR patterns (without braces)
    static std::string ExpandSimpleVariables(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("$", pos)) != std::string::npos) {
            if (pos + 1 < result.length() && result[pos + 1] == '{') {
                pos++;  // Skip ${} patterns
                continue;
            }
            
            size_t end = pos + 1;
            while (end < result.length() && 
                   (std::isalnum(result[end]) || result[end] == '_')) {
                end++;
            }
            
            if (end > pos + 1) {
                std::string var_name = result.substr(pos + 1, end - pos - 1);
                std::string var_value = hshm::SystemInfo::Getenv(var_name);
                result.replace(pos, end - pos, var_value);
                pos += var_value.length();
            } else {
                pos++;
            }
        }
        
        return result;
    }
};

// Usage examples
std::string path1 = EnvironmentExpander::ExpandVariables("${HOME}/data/${USER}/files");
std::string path2 = EnvironmentExpander::ExpandSimpleVariables("$HOME/data/$USER/files");
```

### Advanced Expansion with Defaults

```cpp
class AdvancedEnvironmentExpander {
public:
    // Expand with default values: ${VAR:-default}
    static std::string ExpandWithDefaults(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_expr = result.substr(pos + 2, end - pos - 2);
            std::string var_name, default_value;
            
            size_t default_pos = var_expr.find(":-");
            if (default_pos != std::string::npos) {
                var_name = var_expr.substr(0, default_pos);
                default_value = var_expr.substr(default_pos + 2);
            } else {
                var_name = var_expr;
            }
            
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            if (var_value.empty() && !default_value.empty()) {
                var_value = default_value;
            }
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
    
    // Expand with alternative: ${VAR:+alternative}
    static std::string ExpandWithAlternative(const std::string& input) {
        std::string result = input;
        size_t pos = 0;
        
        while ((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string var_expr = result.substr(pos + 2, end - pos - 2);
            std::string var_name, alt_value;
            
            size_t alt_pos = var_expr.find(":+");
            if (alt_pos != std::string::npos) {
                var_name = var_expr.substr(0, alt_pos);
                alt_value = var_expr.substr(alt_pos + 2);
            } else {
                var_name = var_expr;
            }
            
            std::string var_value = hshm::SystemInfo::Getenv(var_name);
            if (!var_value.empty() && !alt_value.empty()) {
                var_value = alt_value;  // Use alternative if var is set
            }
            
            result.replace(pos, end - pos + 1, var_value);
            pos += var_value.length();
        }
        
        return result;
    }
};

// Usage examples
std::string config = AdvancedEnvironmentExpander::ExpandWithDefaults(
    "${CONFIG_DIR:-/etc/myapp}/config.yaml"
);
std::string message = AdvancedEnvironmentExpander::ExpandWithAlternative(
    "${DEBUG:+Debug mode is enabled}"
);
```

## Environment Setup Patterns

### Application Environment Initialization

```cpp
class EnvironmentSetup {
public:
    static void InitializeApplicationEnvironment(const std::string& app_name) {
        // Set application identification
        hshm::SystemInfo::Setenv("APP_NAME", app_name, 1);
        hshm::SystemInfo::Setenv("APP_VERSION", GetVersion(), 1);
        hshm::SystemInfo::Setenv("APP_PID", std::to_string(getpid()), 1);
        
        // Set up directory structure
        SetupDirectories(app_name);
        
        // Configure runtime paths
        ConfigureRuntimePaths();
        
        // Set up locale if not set
        ConfigureLocale();
        
        // Set process-specific variables
        SetProcessVariables();
        
        printf("Environment initialized for %s\n", app_name.c_str());
    }
    
private:
    static std::string GetVersion() {
        // Read from version file or return compiled version
        return "2.1.0";
    }
    
    static void SetupDirectories(const std::string& app_name) {
        std::string home = hshm::SystemInfo::Getenv("HOME");
        if (home.empty()) home = "/tmp";
        
        std::string app_home = home + "/." + app_name;
        hshm::SystemInfo::Setenv("APP_HOME", app_home, 1);
        hshm::SystemInfo::Setenv("APP_CONFIG_DIR", app_home + "/config", 0);
        hshm::SystemInfo::Setenv("APP_DATA_DIR", app_home + "/data", 0);
        hshm::SystemInfo::Setenv("APP_CACHE_DIR", app_home + "/cache", 0);
        hshm::SystemInfo::Setenv("APP_LOG_DIR", app_home + "/logs", 0);
        hshm::SystemInfo::Setenv("APP_TMP_DIR", app_home + "/tmp", 0);
    }
    
    static void ConfigureRuntimePaths() {
        // Get executable path (platform-specific)
        std::string exe_path = GetExecutablePath();
        std::string exe_dir = GetDirectoryFromPath(exe_path);
        
        hshm::SystemInfo::Setenv("APP_BIN_DIR", exe_dir, 1);
        hshm::SystemInfo::Setenv("APP_LIB_DIR", exe_dir + "/../lib", 1);
        hshm::SystemInfo::Setenv("APP_SHARE_DIR", exe_dir + "/../share", 1);
        
        // Update library path
        UpdateLibraryPath(exe_dir + "/../lib");
    }
    
    static void ConfigureLocale() {
        if (hshm::SystemInfo::Getenv("LANG").empty()) {
            hshm::SystemInfo::Setenv("LANG", "en_US.UTF-8", 1);
        }
        if (hshm::SystemInfo::Getenv("LC_ALL").empty()) {
            hshm::SystemInfo::Setenv("LC_ALL", "C", 0);
        }
    }
    
    static void SetProcessVariables() {
        // Set process start time
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        hshm::SystemInfo::Setenv("APP_START_TIME", std::to_string(timestamp), 1);
        
        // Set hostname
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hshm::SystemInfo::Setenv("APP_HOSTNAME", hostname, 1);
        }
        
        // Set user info
        hshm::SystemInfo::Setenv("APP_UID", std::to_string(getuid()), 1);
        hshm::SystemInfo::Setenv("APP_GID", std::to_string(getgid()), 1);
    }
    
    static std::string GetExecutablePath() {
#ifdef __linux__
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) {
            path[len] = '\0';
            return std::string(path);
        }
#elif __APPLE__
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::string(path);
        }
#endif
        return "";
    }
    
    static std::string GetDirectoryFromPath(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
        return ".";
    }
    
    static void UpdateLibraryPath(const std::string& new_path) {
        std::string current_ld_path = hshm::SystemInfo::Getenv("LD_LIBRARY_PATH");
        std::string updated_path = new_path;
        if (!current_ld_path.empty()) {
            updated_path += ":" + current_ld_path;
        }
        hshm::SystemInfo::Setenv("LD_LIBRARY_PATH", updated_path, 1);
        
#ifdef __APPLE__
        // Also update DYLD_LIBRARY_PATH on macOS
        std::string current_dyld = hshm::SystemInfo::Getenv("DYLD_LIBRARY_PATH");
        std::string updated_dyld = new_path;
        if (!current_dyld.empty()) {
            updated_dyld += ":" + current_dyld;
        }
        hshm::SystemInfo::Setenv("DYLD_LIBRARY_PATH", updated_dyld, 1);
#endif
    }
};
```

## Environment Variable Security

```cpp
class SecureEnvironment {
public:
    // Remove sensitive variables
    static void ClearSensitiveVariables() {
        std::vector<std::string> sensitive_vars = {
            "PASSWORD",
            "SECRET_KEY",
            "API_TOKEN",
            "DB_PASSWORD",
            "PRIVATE_KEY",
            "CREDENTIALS"
        };
        
        for (const auto& var : sensitive_vars) {
            // Check for common prefixes
            for (const auto& prefix : {"", "APP_", "MY_", "SYSTEM_"}) {
                std::string full_var = prefix + var;
                hshm::SystemInfo::Unsetenv(full_var.c_str());
            }
        }
    }
    
    // Save and restore environment
    static std::map<std::string, std::string> SaveEnvironment(
        const std::vector<std::string>& vars) {
        std::map<std::string, std::string> saved;
        
        for (const auto& var : vars) {
            std::string value = hshm::SystemInfo::Getenv(var);
            if (!value.empty()) {
                saved[var] = value;
            }
        }
        
        return saved;
    }
    
    static void RestoreEnvironment(
        const std::map<std::string, std::string>& saved) {
        for (const auto& [var, value] : saved) {
            hshm::SystemInfo::Setenv(var.c_str(), value, 1);
        }
    }
    
    // Create isolated environment
    static void CreateIsolatedEnvironment() {
        // Clear all non-essential variables
        extern char **environ;
        if (environ) {
            std::vector<std::string> to_remove;
            for (char **env = environ; *env; env++) {
                std::string var(*env);
                size_t eq_pos = var.find('=');
                if (eq_pos != std::string::npos) {
                    std::string name = var.substr(0, eq_pos);
                    // Keep only essential variables
                    if (!IsEssentialVariable(name)) {
                        to_remove.push_back(name);
                    }
                }
            }
            
            for (const auto& var : to_remove) {
                hshm::SystemInfo::Unsetenv(var.c_str());
            }
        }
        
        // Set minimal environment
        hshm::SystemInfo::Setenv("PATH", "/usr/bin:/bin", 1);
        hshm::SystemInfo::Setenv("HOME", "/tmp", 1);
        hshm::SystemInfo::Setenv("USER", "nobody", 1);
    }
    
private:
    static bool IsEssentialVariable(const std::string& name) {
        static const std::set<std::string> essential = {
            "PATH", "HOME", "USER", "SHELL", "TERM",
            "LANG", "LC_ALL", "TZ", "TMPDIR"
        };
        return essential.count(name) > 0;
    }
};
```

## Complete Example: Environment-Driven Application

```cpp
#include "hermes_shm/introspect/system_info.h"
#include <iostream>
#include <map>

class EnvironmentDrivenApp {
    AppConfiguration config_;
    std::map<std::string, std::string> original_env_;
    
public:
    int Run() {
        try {
            // Save original environment
            SaveOriginalEnvironment();
            
            // Initialize application environment
            InitializeEnvironment();
            
            // Load configuration from environment
            config_.LoadFromEnvironment();
            
            // Validate environment
            if (!ValidateEnvironment()) {
                std::cerr << "Environment validation failed\n";
                return 1;
            }
            
            // Run application
            return RunApplication();
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }
    
private:
    void SaveOriginalEnvironment() {
        // Save important variables
        std::vector<std::string> important_vars = {
            "PATH", "LD_LIBRARY_PATH", "HOME", "USER"
        };
        
        for (const auto& var : important_vars) {
            std::string value = hshm::SystemInfo::Getenv(var);
            if (!value.empty()) {
                original_env_[var] = value;
            }
        }
    }
    
    void InitializeEnvironment() {
        // Set application-specific environment
        EnvironmentSetup::InitializeApplicationEnvironment("myapp");
        
        // Set feature flags from command line or config
        SetFeatureFlags();
        
        // Configure debugging
        ConfigureDebugging();
    }
    
    void SetFeatureFlags() {
        // Enable features based on environment
        std::string features = hshm::SystemInfo::Getenv("APP_FEATURES");
        if (features.find("experimental") != std::string::npos) {
            hshm::SystemInfo::Setenv("ENABLE_EXPERIMENTAL", "1", 1);
        }
        if (features.find("verbose") != std::string::npos) {
            hshm::SystemInfo::Setenv("VERBOSE_LOGGING", "1", 1);
        }
        if (features.find("profiling") != std::string::npos) {
            hshm::SystemInfo::Setenv("ENABLE_PROFILING", "1", 1);
        }
    }
    
    void ConfigureDebugging() {
        if (config_.IsDebugMode()) {
            hshm::SystemInfo::Setenv("MALLOC_CHECK_", "3", 1);  // glibc malloc debugging
            hshm::SystemInfo::Setenv("G_DEBUG", "fatal-warnings", 1);  // GLib debugging
        }
    }
    
    bool ValidateEnvironment() {
        // Check required variables
        std::vector<std::string> required = {
            "APP_HOME", "APP_CONFIG_DIR", "APP_DATA_DIR"
        };
        
        for (const auto& var : required) {
            if (hshm::SystemInfo::Getenv(var).empty()) {
                std::cerr << "Required variable " << var << " not set\n";
                return false;
            }
        }
        
        // Validate paths exist
        std::string config_dir = hshm::SystemInfo::Getenv("APP_CONFIG_DIR");
        if (!DirectoryExists(config_dir)) {
            std::cerr << "Config directory does not exist: " << config_dir << "\n";
            return false;
        }
        
        return true;
    }
    
    int RunApplication() {
        printf("Application running with configuration:\n");
        printf("  Config Dir: %s\n", config_.GetConfigDir().c_str());
        printf("  Data Dir: %s\n", config_.GetDataDir().c_str());
        printf("  Debug Mode: %s\n", config_.IsDebugMode() ? "ON" : "OFF");
        printf("  Max Memory: %zu MB\n", config_.GetMaxMemory() / (1024*1024));
        printf("  Threads: %d\n", config_.GetThreadCount());
        
        // Main application logic here...
        
        return 0;
    }
    
    bool DirectoryExists(const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    
public:
    ~EnvironmentDrivenApp() {
        // Restore original environment if needed
        for (const auto& [var, value] : original_env_) {
            hshm::SystemInfo::Setenv(var.c_str(), value, 1);
        }
    }
};

int main() {
    EnvironmentDrivenApp app;
    return app.Run();
}
```

## Best Practices

1. **Namespace Variables**: Use application-specific prefixes (e.g., `APP_`, `MYAPP_`) to avoid conflicts
2. **Default Values**: Always provide sensible defaults when environment variables are not set
3. **Size Limits**: Use size limits when reading potentially large environment variables
4. **Security**: Never store passwords or sensitive data in environment variables
5. **Documentation**: Document all environment variables your application uses
6. **Validation**: Validate environment variable values before use
7. **XDG Compliance**: Follow XDG Base Directory specification for Unix systems
8. **Cleanup**: Unset temporary variables when no longer needed
9. **Overwrite Policy**: Be careful with the overwrite flag when setting variables
10. **Platform Awareness**: Consider platform differences in environment variable handling

# Creating HSHM Data Structures: A Complete Guide

## Overview

This guide explains how to create custom shared memory data structures using the Hermes Shared Memory (HSHM) framework. HSHM data structures are designed to work seamlessly in shared memory environments while maintaining compatibility with standard C++ containers.

## Core Concepts

### 1. ShmContainer Base Class

All HSHM data structures inherit from `ShmContainer`, which marks them as shared memory-compatible:

```cpp
namespace hshm::ipc {
    class ShmContainer {};
}
```

### 2. Template System Architecture

HSHM uses an advanced macro-based template system with several key components:

#### Template Parameters
```cpp
#define HSHM_CLASS_TEMPL typename AllocT, hipc::ShmFlagField HSHM_FLAGS
#define HSHM_CLASS_TEMPL_ARGS AllocT, HSHM_FLAGS
```

**AllocT**: The allocator type (e.g., `StackAllocator`, `ScalablePageAllocator`)  
**HSHM_FLAGS**: Bitfield controlling behavior (private vs shared memory, destructible vs undestructible)

#### Flag System
```cpp
struct ShmFlag {
    CLS_CONST ShmFlagField kIsPrivate = BIT_OPT(ShmFlagField, 0);        // Use process-local memory
    CLS_CONST ShmFlagField kIsUndestructable = BIT_OPT(ShmFlagField, 1); // Skip destructor calls
    CLS_CONST ShmFlagField kIsThreadLocal = kIsPrivate | kIsUndestructable;
};
```

### 3. The IS_SHM_ARCHIVEABLE Macro

This critical macro determines if a type is shared memory compatible:

```cpp
#define IS_SHM_ARCHIVEABLE(T) \
  std::is_base_of<hshm::ipc::ShmContainer, TYPE_UNWRAP(T)>::value
```

**Usage**: Enables compile-time decisions about memory operations:
- **true**: Type inherits from `ShmContainer` - use shared memory serialization
- **false**: Regular POD type - use simple `memcpy`

### 4. The TYPE_UNWRAP and __TU Macros

These macros handle template parameters with complex types:

```cpp
#define TYPE_UNWRAP(X) ESC(ISH X)
#define __TU(X) TYPE_UNWRAP(X)
```

**Purpose**: Remove parentheses from macro parameters containing commas
**Example**: `__TU((std::vector<int, MyAlloc>))` → `std::vector<int, MyAlloc>`

### 5. The HIPC_CONTAINER_TEMPLATE Macro

This is the core macro that generates the shared memory infrastructure:

```cpp
#define HIPC_CONTAINER_TEMPLATE(CLASS_NAME, CLASS_NEW_ARGS)                   \
  HIPC_CONTAINER_TEMPLATE_BASE(                                               \
      CLASS_NAME,                                                             \
      (__TU(CLASS_NAME) < __TU(CLASS_NEW_ARGS), HSHM_CLASS_TEMPL_ARGS >),     \
      (__TU(CLASS_NAME) < __TU(CLASS_NEW_ARGS), HSHM_CLASS_TEMPL_TLS_ARGS >), \
      (__TU(CLASS_NAME) < __TU(CLASS_NEW_ARGS), AllocT, OTHER_FLAGS >))
```

**Generated functionality**:
- Allocator management
- Constructor/destructor handling
- Thread-local storage support
- Memory context management
- Pointer conversion utilities

## Step-by-Step Guide: Creating a Custom Data Structure

### Step 1: Basic Structure Setup

```cpp
#include "hermes_shm/data_structures/internal/shm_internal.h"

namespace hshm::ipc {

// Forward declaration
template <typename T, HSHM_CLASS_TEMPL_WITH_DEFAULTS>
class my_container;

// Define macros for container template
#define CLASS_NAME my_container
#define CLASS_NEW_ARGS T

template <typename T, HSHM_CLASS_TEMPL>
class my_container : public ShmContainer {
 public:
  HIPC_CONTAINER_TEMPLATE((CLASS_NAME), (CLASS_NEW_ARGS))

 private:
  // Your data members here
  OffsetPointer data_ptr_;
  size_t size_;
  size_t capacity_;

 public:
  // Your implementation here...
};

// Cleanup macros
#undef CLASS_NAME
#undef CLASS_NEW_ARGS

} // namespace hshm::ipc
```

### Step 2: Implement Required Interface Methods

Every HSHM data structure must implement:

#### IsNull() and SetNull()
```cpp
HSHM_INLINE_CROSS_FUN bool IsNull() const { 
    return data_ptr_.IsNull(); 
}

HSHM_INLINE_CROSS_FUN void SetNull() {
    data_ptr_.SetNull();
    size_ = 0;
    capacity_ = 0;
}
```

#### shm_destroy_main()
```cpp
HSHM_INLINE_CROSS_FUN void shm_destroy_main() {
    // Clean up all allocated elements
    clear_all_elements();
    
    // Free the main data buffer
    if (!data_ptr_.IsNull()) {
        CtxAllocator<AllocT> alloc = GetCtxAllocator();
        FullPtr<void, OffsetPointer> full_ptr(
            alloc->template Convert<void>(data_ptr_), data_ptr_);
        alloc->template Free<void>(alloc.ctx_, full_ptr);
    }
}
```

### Step 3: Implement Constructors

#### Default Constructor
```cpp
HSHM_CROSS_FUN explicit my_container() {
    init_shm_container(HSHM_MEMORY_MANAGER->GetDefaultAllocator<AllocT>());
    SetNull();
}

HSHM_CROSS_FUN explicit my_container(const hipc::CtxAllocator<AllocT> &alloc) {
    init_shm_container(alloc);
    SetNull();
}
```

#### Parameterized Constructor
```cpp
template <typename... Args>
HSHM_CROSS_FUN explicit my_container(size_t initial_capacity, Args &&...args) {
    shm_init(HSHM_MEMORY_MANAGER->GetDefaultAllocator<AllocT>(), 
             initial_capacity, std::forward<Args>(args)...);
}

template <typename... Args>
HSHM_CROSS_FUN void shm_init(const CtxAllocator<AllocT> &alloc, 
                             size_t initial_capacity, Args &&...args) {
    init_shm_container(alloc);
    SetNull();
    reserve(initial_capacity);
    // Initialize with args...
}
```

### Step 4: Implement Copy Semantics

The HSHM framework provides patterns for handling different copy scenarios:

```cpp
// Copy constructor
HSHM_CROSS_FUN explicit my_container(const my_container &other) {
    init_shm_container(other.GetCtxAllocator());
    SetNull();
    shm_strong_copy_main(other);
}

// Main copy implementation
template <typename ContainerT>
HSHM_CROSS_FUN void shm_strong_copy_main(const ContainerT &other) {
    reserve(other.size());
    
    if constexpr (std::is_pod<T>() && !IS_SHM_ARCHIVEABLE(T)) {
        // Fast path: Plain old data types
        memcpy(data(), other.data(), other.size() * sizeof(T));
        size_ = other.size();
    } else {
        // Slow path: Complex types requiring construction
        for (const auto &item : other) {
            emplace_back(item);
        }
    }
}
```

### Step 5: Implement Core Operations

#### Memory Management
```cpp
template <typename... Args>
HSHM_CROSS_FUN void reserve(size_t new_capacity, Args &&...args) {
    if (new_capacity <= capacity_) return;
    
    CtxAllocator<AllocT> alloc = GetCtxAllocator();
    
    if constexpr (std::is_pod<T>() && !IS_SHM_ARCHIVEABLE(T)) {
        // Use reallocation for POD types
        if (!data_ptr_.IsNull()) {
            FullPtr<T, OffsetPointer> old_ptr(
                alloc->template Convert<T>(data_ptr_), data_ptr_);
            auto new_ptr = alloc->template ReallocateObjs<T>(
                alloc.ctx_, old_ptr, new_capacity);
            data_ptr_ = new_ptr.shm_;
        } else {
            auto new_ptr = alloc->template AllocateObjs<T, OffsetPointer>(
                alloc.ctx_, new_capacity);
            data_ptr_ = new_ptr.shm_;
        }
    } else {
        // Manual copy for complex types
        auto new_ptr = alloc->template AllocateObjs<T, OffsetPointer>(
            alloc.ctx_, new_capacity);
        T* new_data = new_ptr.ptr_;
        OffsetPointer new_data_ptr = new_ptr.shm_;
        
        // Move existing elements
        if (!data_ptr_.IsNull()) {
            T* old_data = alloc->template Convert<T>(data_ptr_);
            for (size_t i = 0; i < size_; ++i) {
                new (new_data + i) T(std::move(old_data[i]));
                old_data[i].~T();
            }
            
            FullPtr<void, OffsetPointer> old_ptr(
                alloc->template Convert<void>(data_ptr_), data_ptr_);
            alloc->template Free<void>(alloc.ctx_, old_ptr);
        }
        
        data_ptr_ = new_data_ptr;
    }
    
    capacity_ = new_capacity;
}
```

#### Element Access
```cpp
HSHM_INLINE_CROSS_FUN T& operator[](size_t index) {
    return data()[index];
}

HSHM_INLINE_CROSS_FUN const T& operator[](size_t index) const {
    return data()[index];
}

HSHM_INLINE_CROSS_FUN T* data() {
    return GetAllocator()->template Convert<T>(data_ptr_);
}

HSHM_INLINE_CROSS_FUN const T* data() const {
    return GetAllocator()->template Convert<T>(data_ptr_);
}
```

### Step 6: Advanced Features

#### Iterator Support
```cpp
// Define iterator types
typedef T* iterator_t;
typedef const T* const_iterator_t;

// Implement iterator methods
HSHM_INLINE_CROSS_FUN iterator_t begin() { return data(); }
HSHM_INLINE_CROSS_FUN iterator_t end() { return data() + size_; }
HSHM_INLINE_CROSS_FUN const_iterator_t cbegin() const { return data(); }
HSHM_INLINE_CROSS_FUN const_iterator_t cend() const { return data() + size_; }
```

#### Serialization Support
```cpp
template <typename Ar>
HSHM_CROSS_FUN void save(Ar &ar) const {
    ar << size_;
    for (size_t i = 0; i < size_; ++i) {
        ar << (*this)[i];
    }
}

template <typename Ar>
HSHM_CROSS_FUN void load(Ar &ar) {
    size_t loaded_size;
    ar >> loaded_size;
    reserve(loaded_size);
    size_ = loaded_size;
    for (size_t i = 0; i < size_; ++i) {
        ar >> (*this)[i];
    }
}
```

## Complete Example: Custom Stack Container

Here's a complete implementation of a stack data structure:

```cpp
#include "hermes_shm/data_structures/internal/shm_internal.h"
#include "hermes_shm/data_structures/serialization/serialize_common.h"

namespace hshm::ipc {

// Forward declaration
template <typename T, HSHM_CLASS_TEMPL_WITH_DEFAULTS>
class stack;

// Template helper macros
#define CLASS_NAME stack
#define CLASS_NEW_ARGS T

/**
 * A stack data structure optimized for shared memory
 */
template <typename T, HSHM_CLASS_TEMPL>
class stack : public ShmContainer {
 public:
  HIPC_CONTAINER_TEMPLATE((CLASS_NAME), (CLASS_NEW_ARGS))

 private:
  OffsetPointer data_ptr_;
  size_t size_;
  size_t capacity_;

 public:
  /**====================================
   * Default Constructors
   * ===================================*/
   
  HSHM_CROSS_FUN explicit stack() {
    init_shm_container(HSHM_MEMORY_MANAGER->GetDefaultAllocator<AllocT>());
    SetNull();
  }

  HSHM_CROSS_FUN explicit stack(const hipc::CtxAllocator<AllocT> &alloc) {
    init_shm_container(alloc);
    SetNull();
  }

  HSHM_CROSS_FUN explicit stack(size_t initial_capacity) {
    shm_init(HSHM_MEMORY_MANAGER->GetDefaultAllocator<AllocT>(), initial_capacity);
  }

  HSHM_CROSS_FUN explicit stack(const hipc::CtxAllocator<AllocT> &alloc, 
                               size_t initial_capacity) {
    shm_init(alloc, initial_capacity);
  }

  HSHM_CROSS_FUN void shm_init(const CtxAllocator<AllocT> &alloc, 
                               size_t initial_capacity) {
    init_shm_container(alloc);
    SetNull();
    reserve(initial_capacity);
  }

  /**====================================
   * Copy Constructors
   * ===================================*/
   
  HSHM_CROSS_FUN explicit stack(const stack &other) {
    init_shm_container(other.GetCtxAllocator());
    SetNull();
    shm_strong_copy_main(other);
  }

  HSHM_CROSS_FUN explicit stack(const hipc::CtxAllocator<AllocT> &alloc,
                               const stack &other) {
    init_shm_container(alloc);
    SetNull();
    shm_strong_copy_main(other);
  }

  HSHM_CROSS_FUN stack &operator=(const stack &other) {
    if (this != &other) {
      shm_destroy();
      shm_strong_copy_main(other);
    }
    return *this;
  }

  template <typename ContainerT>
  HSHM_CROSS_FUN void shm_strong_copy_main(const ContainerT &other) {
    reserve(other.size());
    if constexpr (std::is_pod<T>() && !IS_SHM_ARCHIVEABLE(T)) {
      memcpy(data(), other.data(), other.size() * sizeof(T));
      size_ = other.size();
    } else {
      for (size_t i = 0; i < other.size(); ++i) {
        push(other[i]);
      }
    }
  }

  /**====================================
   * Move Constructors
   * ===================================*/
   
  HSHM_CROSS_FUN stack(stack &&other) {
    shm_move_op<false>(HSHM_MEMORY_MANAGER->GetDefaultAllocator<AllocT>(), 
                       std::move(other));
  }

  HSHM_CROSS_FUN stack(const hipc::CtxAllocator<AllocT> &alloc, stack &&other) {
    shm_move_op<false>(alloc, std::move(other));
  }

  HSHM_CROSS_FUN stack &operator=(stack &&other) noexcept {
    if (this != &other) {
      shm_move_op<true>(other.GetCtxAllocator(), std::move(other));
    }
    return *this;
  }

  template <bool IS_ASSIGN>
  HSHM_CROSS_FUN void shm_move_op(const hipc::CtxAllocator<AllocT> &alloc,
                                  stack &&other) noexcept {
    if constexpr (!IS_ASSIGN) {
      init_shm_container(alloc);
    }
    if (GetAllocator() == other.GetAllocator()) {
      // Same allocator: simple move
      memcpy((void *)this, (void *)&other, sizeof(*this));
      other.SetNull();
    } else {
      // Different allocator: copy and destroy
      shm_strong_copy_main(other);
      other.shm_destroy();
    }
  }

  /**====================================
   * Destructors
   * ===================================*/
   
  HSHM_INLINE_CROSS_FUN bool IsNull() const { 
    return data_ptr_.IsNull(); 
  }

  HSHM_INLINE_CROSS_FUN void SetNull() {
    size_ = 0;
    capacity_ = 0;
    data_ptr_.SetNull();
  }

  HSHM_INLINE_CROSS_FUN void shm_destroy_main() {
    clear();
    if (!data_ptr_.IsNull()) {
      CtxAllocator<AllocT> alloc = GetCtxAllocator();
      FullPtr<void, OffsetPointer> full_ptr(
        alloc->template Convert<void>(data_ptr_), data_ptr_);
      alloc->template Free<void>(alloc.ctx_, full_ptr);
    }
  }

  /**====================================
   * Stack Operations
   * ===================================*/
   
  template <typename... Args>
  HSHM_CROSS_FUN void emplace(Args &&...args) {
    if (size_ == capacity_) {
      size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
      reserve(new_capacity);
    }
    T* data = this->data();
    new (data + size_) T(std::forward<Args>(args)...);
    ++size_;
  }

  HSHM_CROSS_FUN void push(const T &item) {
    emplace(item);
  }

  HSHM_CROSS_FUN void push(T &&item) {
    emplace(std::move(item));
  }

  HSHM_CROSS_FUN void pop() {
    if (size_ > 0) {
      --size_;
      data()[size_].~T();
    }
  }

  HSHM_INLINE_CROSS_FUN T &top() { 
    return data()[size_ - 1]; 
  }

  HSHM_INLINE_CROSS_FUN const T &top() const { 
    return data()[size_ - 1]; 
  }

  HSHM_INLINE_CROSS_FUN bool empty() const { 
    return size_ == 0; 
  }

  HSHM_INLINE_CROSS_FUN size_t size() const { 
    return size_; 
  }

  HSHM_INLINE_CROSS_FUN size_t capacity() const { 
    return capacity_; 
  }

  HSHM_CROSS_FUN void clear() {
    T* data = this->data();
    for (size_t i = 0; i < size_; ++i) {
      data[i].~T();
    }
    size_ = 0;
  }

  HSHM_CROSS_FUN void reserve(size_t new_capacity) {
    if (new_capacity <= capacity_) return;

    CtxAllocator<AllocT> alloc = GetCtxAllocator();
    
    if constexpr (std::is_pod<T>() && !IS_SHM_ARCHIVEABLE(T)) {
      // Use reallocation for POD types
      if (!data_ptr_.IsNull()) {
        FullPtr<T, OffsetPointer> old_ptr(
          alloc->template Convert<T>(data_ptr_), data_ptr_);
        auto new_ptr = alloc->template ReallocateObjs<T>(
          alloc.ctx_, old_ptr, new_capacity);
        data_ptr_ = new_ptr.shm_;
      } else {
        auto new_ptr = alloc->template AllocateObjs<T, OffsetPointer>(
          alloc.ctx_, new_capacity);
        data_ptr_ = new_ptr.shm_;
      }
    } else {
      // Manual move for complex types
      auto new_ptr = alloc->template AllocateObjs<T, OffsetPointer>(
        alloc.ctx_, new_capacity);
      T* new_data = new_ptr.ptr_;
      OffsetPointer new_data_ptr = new_ptr.shm_;
      
      if (!data_ptr_.IsNull()) {
        T* old_data = alloc->template Convert<T>(data_ptr_);
        for (size_t i = 0; i < size_; ++i) {
          new (new_data + i) T(std::move(old_data[i]));
          old_data[i].~T();
        }
        
        FullPtr<void, OffsetPointer> old_ptr(
          alloc->template Convert<void>(data_ptr_), data_ptr_);
        alloc->template Free<void>(alloc.ctx_, old_ptr);
      }
      
      data_ptr_ = new_data_ptr;
    }
    
    capacity_ = new_capacity;
  }

  /**====================================
   * Utility Methods
   * ===================================*/
   
  HSHM_INLINE_CROSS_FUN T* data() {
    return GetAllocator()->template Convert<T>(data_ptr_);
  }

  HSHM_INLINE_CROSS_FUN const T* data() const {
    return GetAllocator()->template Convert<T>(data_ptr_);
  }

  HSHM_INLINE_CROSS_FUN T& operator[](size_t index) {
    return data()[index];
  }

  HSHM_INLINE_CROSS_FUN const T& operator[](size_t index) const {
    return data()[index];
  }

  /**====================================
   * Serialization
   * ===================================*/
   
  template <typename Ar>
  HSHM_CROSS_FUN void save(Ar &ar) const {
    ar << size_;
    for (size_t i = 0; i < size_; ++i) {
      ar << (*this)[i];
    }
  }

  template <typename Ar>
  HSHM_CROSS_FUN void load(Ar &ar) {
    size_t loaded_size;
    ar >> loaded_size;
    clear();
    reserve(loaded_size);
    size_ = loaded_size;
    for (size_t i = 0; i < size_; ++i) {
      ar >> (*this)[i];
    }
  }
};

#undef CLASS_NAME
#undef CLASS_NEW_ARGS

} // namespace hshm::ipc

// Namespace alias for convenience
namespace hshm {
  template <typename T, HSHM_CLASS_TEMPL_WITH_PRIV_DEFAULTS>
  using stack = hipc::stack<T, HSHM_CLASS_TEMPL_ARGS>;
}
```

## Usage Example

```cpp
#include "your_stack.h"

int main() {
    // Create shared memory allocator
    auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<hipc::ScalablePageAllocator>();
    
    // Create stack in shared memory
    hipc::stack<int> shared_stack(alloc, 100);  // Initial capacity of 100
    
    // Use the stack
    shared_stack.push(42);
    shared_stack.push(24);
    shared_stack.emplace(99);
    
    printf("Top element: %d\n", shared_stack.top());  // Prints: 99
    printf("Stack size: %zu\n", shared_stack.size());  // Prints: 3
    
    // Pop elements
    shared_stack.pop();
    printf("New top: %d\n", shared_stack.top());  // Prints: 24
    
    // Create a local stack (for comparison)
    hshm::stack<int> local_stack;  // Uses private memory allocator
    local_stack.push(1);
    local_stack.push(2);
    
    return 0;
}
```

## Best Practices

### 1. Memory Safety
- Always check `IsNull()` before operations
- Implement proper `shm_destroy_main()` cleanup
- Use RAII principles with proper constructors/destructors

### 2. Performance Optimization
- Use `IS_SHM_ARCHIVEABLE` to optimize for POD vs complex types
- Prefer `emplace` over `push` when possible
- Batch allocations to reduce fragmentation

### 3. Template Design
- Use `HSHM_CLASS_TEMPL_WITH_DEFAULTS` for public interfaces
- Always provide both shared and private memory versions
- Clean up macros with `#undef` at the end of headers

### 4. Error Handling
- Throw appropriate HSHM exceptions for error conditions
- Validate inputs in public methods
- Handle out-of-memory conditions gracefully

### 5. Serialization
- Implement `save()` and `load()` methods for network compatibility
- Consider endianness for cross-platform deployments
- Handle version compatibility in serialization formats

## Advanced Topics

### Custom Allocators
```cpp
// Custom allocator-aware container
template <typename T, typename AllocT = hipc::ScalablePageAllocator>
class my_custom_container : public ShmContainer {
    // Implementation using specific allocator constraints...
};
```

### Thread-Local Containers
```cpp
// Thread-local version using flags
hipc::stack<int, hipc::MallocAllocator, hipc::ShmFlag::kIsThreadLocal> tls_stack;
```

### Heterogeneous Data Structures
```cpp
// Container holding other HSHM containers
hipc::vector<hipc::string> string_vector;
hipc::stack<hipc::vector<int>> nested_container;
```

This comprehensive guide provides everything needed to create sophisticated shared memory data structures that integrate seamlessly with the HSHM ecosystem while maintaining high performance and memory safety.

# HSHM Singleton Utilities Guide

## Overview

The Singleton Utilities API in Hermes Shared Memory (HSHM) provides multiple singleton patterns optimized for different use cases, including thread safety, cross-device compatibility, and performance requirements. These utilities enable global state management across complex applications and shared memory systems.

## Singleton Variants

### Basic Singleton (Thread-Safe)

```cpp
#include "hermes_shm/util/singleton.h"

class DatabaseConfig {
public:
    std::string connection_string;
    int max_connections;
    
    DatabaseConfig() {
        connection_string = "localhost:5432";
        max_connections = 100;
    }
    
    void Configure(const std::string& host, int max_conn) {
        connection_string = host;
        max_connections = max_conn;
    }
};

// Thread-safe singleton access
DatabaseConfig* config = hshm::Singleton<DatabaseConfig>::GetInstance();
config->Configure("prod-db:5432", 200);

// Multiple access from different threads
void worker_thread() {
    DatabaseConfig* cfg = hshm::Singleton<DatabaseConfig>::GetInstance();
    printf("Connecting to: %s\n", cfg->connection_string.c_str());
}
```

### Lockfree Singleton (High Performance)

```cpp
class MetricsCollector {
    std::atomic<size_t> counter_;
    
public:
    MetricsCollector() : counter_(0) {}
    
    void Increment() {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }
    
    size_t GetCount() const {
        return counter_.load(std::memory_order_relaxed);
    }
};

// High-performance singleton without locking overhead
void hot_path_function() {
    auto* metrics = hshm::LockfreeSingleton<MetricsCollector>::GetInstance();
    metrics->Increment();  // Very fast, no locks
}
```

### Cross-Device Singleton

```cpp
class GPUManager {
public:
    int device_count;
    std::vector<int> available_devices;
    
    GPUManager() {
        device_count = GetGPUCount();
        InitializeDevices();
    }
    
private:
    int GetGPUCount();
    void InitializeDevices();
};

// Works on both host and GPU code
HSHM_CROSS_FUN
void initialize_cuda_context() {
    GPUManager* gpu_mgr = hshm::CrossSingleton<GPUManager>::GetInstance();
    printf("Found %d GPU devices\n", gpu_mgr->device_count);
}

// Lockfree version for GPU performance
HSHM_CROSS_FUN
void gpu_kernel_function() {
    auto* gpu_mgr = hshm::LockfreeCrossSingleton<GPUManager>::GetInstance();
    // Access without locking overhead in GPU kernels
}
```

### Global Singleton (Eager Initialization)

```cpp
class Logger {
public:
    std::ofstream log_file;
    std::mutex log_mutex;
    
    Logger() {
        log_file.open("/var/log/application.log", std::ios::app);
        printf("Logger initialized during program startup\n");
    }
    
    void Log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_file << "[" << GetTimestamp() << "] " << message << std::endl;
    }
    
private:
    std::string GetTimestamp();
};

// Initialized immediately when program starts
Logger* logger = hshm::GlobalSingleton<Logger>::GetInstance();

void application_function() {
    // Logger already exists and is ready
    hshm::GlobalSingleton<Logger>::GetInstance()->Log("Function called");
}
```

### Platform-Aware Global Singleton

```cpp
class NetworkManager {
public:
    std::string local_hostname;
    std::vector<std::string> network_interfaces;
    
    NetworkManager() {
        DiscoverNetworkInterfaces();
        printf("Network manager initialized\n");
    }
    
private:
    void DiscoverNetworkInterfaces();
};

// Automatically chooses best implementation for platform
HSHM_CROSS_FUN
void network_operation() {
    auto* net_mgr = hshm::GlobalCrossSingleton<NetworkManager>::GetInstance();
    printf("Local hostname: %s\n", net_mgr->local_hostname.c_str());
}
```

## C-Style Global Variable Singletons

### Basic Global Variables

```cpp
// Header declaration
HSHM_DEFINE_GLOBAL_VAR_H(DatabaseConfig, g_db_config);

// Source file definition  
HSHM_DEFINE_GLOBAL_VAR_CC(DatabaseConfig, g_db_config);

// Usage
void configure_database() {
    DatabaseConfig* config = HSHM_GET_GLOBAL_VAR(DatabaseConfig, g_db_config);
    config->Configure("prod:5432", 500);
}
```

### Cross-Platform Global Variables

```cpp
class SharedMemoryPool {
public:
    size_t pool_size;
    void* memory_base;
    
    SharedMemoryPool() : pool_size(0), memory_base(nullptr) {
        InitializePool();
    }
    
private:
    void InitializePool();
};

// Header - works on host and device
HSHM_DEFINE_GLOBAL_CROSS_VAR_H(SharedMemoryPool, g_memory_pool);

// Source file
HSHM_DEFINE_GLOBAL_CROSS_VAR_CC(SharedMemoryPool, g_memory_pool);

// Usage in cross-platform code
HSHM_CROSS_FUN
void allocate_from_pool(size_t size) {
    SharedMemoryPool* pool = HSHM_GET_GLOBAL_CROSS_VAR(SharedMemoryPool, g_memory_pool);
    // Allocation logic here
}
```

### Pointer-Based Global Variables

```cpp
class TaskScheduler {
public:
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    bool running;
    
    TaskScheduler() : running(true) {
        printf("Task scheduler created\n");
    }
    
    void SubmitTask(std::function<void()> task);
    void ProcessTasks();
    void Shutdown();
};

// Header - pointer version for lazy initialization
HSHM_DEFINE_GLOBAL_PTR_VAR_H(TaskScheduler, g_task_scheduler);

// Source file
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(TaskScheduler, g_task_scheduler);

// Usage - automatically creates instance on first access
void submit_work() {
    TaskScheduler* scheduler = HSHM_GET_GLOBAL_PTR_VAR(TaskScheduler, g_task_scheduler);
    
    scheduler->SubmitTask([]() {
        printf("Task executing\n");
    });
}
```

### Cross-Platform Pointer Variables

```cpp
class DeviceMemoryManager {
public:
    size_t total_memory;
    size_t available_memory;
    std::map<void*, size_t> allocations;
    
    DeviceMemoryManager() {
        QueryDeviceMemory();
    }
    
private:
    void QueryDeviceMemory();
};

// Header
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(DeviceMemoryManager, g_device_memory);

// Source file  
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(DeviceMemoryManager, g_device_memory);

// Cross-platform usage
HSHM_CROSS_FUN
void* allocate_device_memory(size_t size) {
    DeviceMemoryManager* mgr = HSHM_GET_GLOBAL_CROSS_PTR_VAR(DeviceMemoryManager, g_device_memory);
    // Device-specific allocation
    return nullptr; // Implementation specific
}
```

## Best Practices

1. **Thread Safety**: Use `Singleton<T>` for thread-safe access, `LockfreeSingleton<T>` only with thread-safe types
2. **Cross-Platform Code**: Use `CrossSingleton<T>` and `GlobalCrossSingleton<T>` for code that runs on both host and device
3. **Python Compatibility**: Avoid standard singletons in code called by Python; use global variables instead
4. **Eager vs Lazy**: Use `GlobalSingleton<T>` for resources needed at startup, regular singletons for lazy initialization
5. **Resource Management**: Implement proper destructors and cleanup in singleton classes
6. **Configuration**: Use singletons for application-wide configuration and settings
7. **Performance**: Use lockfree variants in performance-critical paths with appropriate atomic types
8. **Memory Management**: Be aware that singletons live for the entire program duration
9. **Testing**: Design singleton classes to be testable by allowing dependency injection where possible
10. **Documentation**: Document singleton lifetime and thread safety guarantees for each singleton class

# HSHM Thread System Guide

## Overview

The Thread System API in Hermes Shared Memory (HSHM) provides a unified interface for threading across different platforms and execution environments. The system abstracts pthread, std::thread, Argobots, CUDA, and ROCm threading models, allowing applications to work seamlessly across different environments with the same code.

## Thread Model Architecture

### Available Thread Models

The HSHM thread system supports multiple backend implementations:

- **Pthread** (`ThreadType::kPthread`) - POSIX threads for Unix-like systems
- **StdThread** (`ThreadType::kStdThread`) - Standard C++ threading
- **Argobots** (`ThreadType::kArgobots`) - User-level threading library
- **CUDA** (`ThreadType::kCuda`) - NVIDIA GPU threading
- **ROCm** (`ThreadType::kRocm`) - AMD GPU threading

### Default Thread Models

The system automatically selects appropriate thread models based on the platform:

```cpp
// Default thread models (configured at compile time):
// Host: HSHM_DEFAULT_THREAD_MODEL = hshm::thread::Pthread
// GPU:  HSHM_DEFAULT_THREAD_MODEL_GPU = hshm::thread::StdThread

// Access the current thread model
auto* thread_model = HSHM_THREAD_MODEL;
printf("Using thread model: %s\n", GetThreadTypeName(thread_model->GetType()));

// Get thread model type
HSHM_THREAD_MODEL_T thread_model_ptr = HSHM_THREAD_MODEL;
```

## Basic Threading Operations

### Thread Creation and Management

```cpp
#include "hermes_shm/thread/thread_model_manager.h"

void basic_threading_example() {
    // Get the current thread model
    auto* tm = HSHM_THREAD_MODEL;
    
    // Create a thread group (optional context for organizing threads)
    hshm::ThreadGroupContext group_ctx;
    hshm::ThreadGroup group = tm->CreateThreadGroup(group_ctx);
    
    // Define work function
    auto worker_function = [](int thread_id, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            printf("Thread %d: iteration %d\n", thread_id, i);
            HSHM_THREAD_MODEL->SleepForUs(100000);  // Sleep for 100ms
        }
        printf("Thread %d completed\n", thread_id);
    };
    
    // Spawn threads
    const int num_threads = 4;
    std::vector<hshm::Thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        hshm::Thread thread = tm->Spawn(group, worker_function, i, 10);
        threads.push_back(std::move(thread));
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        tm->Join(thread);
    }
    
    printf("All threads completed\n");
}
```

### Thread Local Storage

```cpp
class ThreadLocalData : public hshm::thread::ThreadLocalData {
public:
    int thread_id;
    std::string thread_name;
    size_t operation_count;
    
    ThreadLocalData(int id, const std::string& name) 
        : thread_id(id), thread_name(name), operation_count(0) {
        printf("TLS created for thread %d (%s)\n", thread_id, thread_name.c_str());
    }
    
    ~ThreadLocalData() {
        printf("TLS destroyed for thread %d, operations: %zu\n", 
               thread_id, operation_count);
    }
};

void thread_local_storage_example() {
    auto* tm = HSHM_THREAD_MODEL;
    
    // Create TLS key
    hshm::ThreadLocalKey tls_key;
    
    auto worker_with_tls = [&tls_key](int thread_id) {
        // Create thread-local data
        ThreadLocalData* tls_data = new ThreadLocalData(thread_id, 
                                                        "Worker-" + std::to_string(thread_id));
        
        // Store in TLS
        HSHM_THREAD_MODEL->SetTls(tls_key, tls_data);
        
        // Use TLS throughout thread execution
        for (int i = 0; i < 5; ++i) {
            ThreadLocalData* my_data = HSHM_THREAD_MODEL->GetTls<ThreadLocalData>(tls_key);
            my_data->operation_count++;
            
            printf("Thread %s: operation %zu\n", 
                   my_data->thread_name.c_str(), my_data->operation_count);
            
            HSHM_THREAD_MODEL->SleepForUs(50000);
        }
        
        // Cleanup is handled automatically by the thread model
    };
    
    // Initialize TLS key
    tm->CreateTls<ThreadLocalData>(tls_key, nullptr);
    
    // Create threads
    hshm::ThreadGroup group = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
    std::vector<hshm::Thread> threads;
    
    for (int i = 0; i < 3; ++i) {
        threads.push_back(tm->Spawn(group, worker_with_tls, i));
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        tm->Join(thread);
    }
}
```

## Cross-Platform Thread Operations

### Thread Utilities

```cpp
void thread_utilities_example() {
    auto* tm = HSHM_THREAD_MODEL;
    
    // Get current thread ID
    hshm::ThreadId current_tid = tm->GetTid();
    printf("Current thread ID: %zu\n", current_tid.tid_);
    
    // Yield current thread
    printf("Yielding thread...\n");
    tm->Yield();
    
    // Sleep for specific duration
    printf("Sleeping for 1 second...\n");
    tm->SleepForUs(1000000);  // 1 second in microseconds
    
    printf("Sleep completed\n");
}

void cpu_affinity_example() {
    auto* tm = HSHM_THREAD_MODEL;
    hshm::ThreadGroup group = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
    
    auto cpu_bound_worker = [](int cpu_id) {
        printf("Worker starting on CPU %d\n", cpu_id);
        
        // CPU-intensive work
        volatile double result = 0.0;
        for (int i = 0; i < 1000000; ++i) {
            result += sin(i * 0.001);
        }
        
        printf("Worker on CPU %d completed, result: %f\n", cpu_id, result);
    };
    
    const int num_cpus = std::thread::hardware_concurrency();
    std::vector<hshm::Thread> threads;
    
    for (int i = 0; i < std::min(4, num_cpus); ++i) {
        hshm::Thread thread = tm->Spawn(group, cpu_bound_worker, i);
        
        // Set CPU affinity (if supported by thread model)
        tm->SetAffinity(thread, i);
        
        threads.push_back(std::move(thread));
    }
    
    for (auto& thread : threads) {
        tm->Join(thread);
    }
}
```

## Producer-Consumer Pattern

```cpp
#include "hermes_shm/types/atomic.h"
#include <queue>
#include <mutex>

template<typename T>
class ThreadSafeQueue {
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    hshm::ipc::atomic<bool> shutdown_;
    
public:
    ThreadSafeQueue() : shutdown_(false) {}
    
    void Push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.notify_one();
    }
    
    bool Pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        condition_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_.load(); 
        });
        
        if (shutdown_.load() && queue_.empty()) {
            return false;  // Shutdown and no more items
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_.store(true);
        }
        condition_.notify_all();
    }
    
    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

void producer_consumer_example() {
    auto* tm = HSHM_THREAD_MODEL;
    ThreadSafeQueue<int> work_queue;
    hshm::ipc::atomic<int> total_produced(0);
    hshm::ipc::atomic<int> total_consumed(0);
    
    // Producer function
    auto producer = [&](int producer_id, int items_to_produce) {
        for (int i = 0; i < items_to_produce; ++i) {
            int item = producer_id * 1000 + i;
            work_queue.Push(item);
            total_produced.fetch_add(1);
            
            printf("Producer %d produced item %d\n", producer_id, item);
            HSHM_THREAD_MODEL->SleepForUs(10000);  // 10ms
        }
        printf("Producer %d finished\n", producer_id);
    };
    
    // Consumer function
    auto consumer = [&](int consumer_id) {
        int item;
        int consumed_count = 0;
        
        while (work_queue.Pop(item)) {
            // Process item
            HSHM_THREAD_MODEL->SleepForUs(20000);  // 20ms processing time
            
            consumed_count++;
            total_consumed.fetch_add(1);
            
            printf("Consumer %d processed item %d (total: %d)\n", 
                   consumer_id, item, consumed_count);
        }
        
        printf("Consumer %d finished, consumed %d items\n", 
               consumer_id, consumed_count);
    };
    
    // Create thread group
    hshm::ThreadGroup group = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
    std::vector<hshm::Thread> threads;
    
    // Start producers
    const int num_producers = 2;
    const int items_per_producer = 10;
    for (int i = 0; i < num_producers; ++i) {
        threads.push_back(tm->Spawn(group, producer, i, items_per_producer));
    }
    
    // Start consumers
    const int num_consumers = 3;
    for (int i = 0; i < num_consumers; ++i) {
        threads.push_back(tm->Spawn(group, consumer, i));
    }
    
    // Wait for producers to finish
    for (int i = 0; i < num_producers; ++i) {
        tm->Join(threads[i]);
    }
    
    // Allow consumers to finish processing remaining items
    while (work_queue.Size() > 0 && total_consumed.load() < total_produced.load()) {
        tm->SleepForUs(10000);
    }
    
    // Shutdown queue and wait for consumers
    work_queue.Shutdown();
    for (int i = num_producers; i < threads.size(); ++i) {
        tm->Join(threads[i]);
    }
    
    printf("Final stats - Produced: %d, Consumed: %d\n", 
           total_produced.load(), total_consumed.load());
}
```

## Thread Pool Implementation

```cpp
class ThreadPool {
    std::vector<hshm::Thread> workers_;
    ThreadSafeQueue<std::function<void()>> task_queue_;
    hshm::ipc::atomic<bool> running_;
    hshm::ThreadGroup group_;
    
public:
    explicit ThreadPool(size_t num_threads) : running_(true) {
        auto* tm = HSHM_THREAD_MODEL;
        group_ = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
        
        // Create worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.push_back(tm->Spawn(group_, [this, i]() {
                WorkerLoop(i);
            }));
        }
        
        printf("Thread pool started with %zu threads\n", num_threads);
    }
    
    ~ThreadPool() {
        Shutdown();
    }
    
    template<typename F>
    void Submit(F&& task) {
        if (running_.load()) {
            task_queue_.Push(std::forward<F>(task));
        }
    }
    
    void Shutdown() {
        if (running_.load()) {
            running_.store(false);
            task_queue_.Shutdown();
            
            auto* tm = HSHM_THREAD_MODEL;
            for (auto& worker : workers_) {
                tm->Join(worker);
            }
            
            printf("Thread pool shutdown complete\n");
        }
    }
    
private:
    void WorkerLoop(size_t worker_id) {
        printf("Worker %zu started\n", worker_id);
        
        std::function<void()> task;
        while (running_.load() || !task_queue_.Size() == 0) {
            if (task_queue_.Pop(task)) {
                try {
                    task();
                } catch (const std::exception& e) {
                    printf("Worker %zu caught exception: %s\n", 
                           worker_id, e.what());
                }
            }
        }
        
        printf("Worker %zu finished\n", worker_id);
    }
};

void thread_pool_example() {
    ThreadPool pool(4);
    
    // Submit various tasks
    for (int i = 0; i < 20; ++i) {
        pool.Submit([i]() {
            printf("Executing task %d on thread %zu\n", 
                   i, HSHM_THREAD_MODEL->GetTid().tid_);
            
            // Simulate work
            HSHM_THREAD_MODEL->SleepForUs(100000 + (i % 5) * 50000);
            
            printf("Task %d completed\n", i);
        });
    }
    
    // Let tasks complete
    HSHM_THREAD_MODEL->SleepForUs(2000000);  // 2 seconds
    
    // Pool automatically shuts down on destruction
}
```

## Platform-Specific Thread Models

### Pthread Implementation

```cpp
#if HSHM_ENABLE_PTHREADS

void pthread_specific_example() {
    // Create a pthread-based thread model explicitly
    hshm::thread::Pthread pthread_model;
    
    printf("Using pthread model\n");
    printf("Thread type: %d\n", static_cast<int>(pthread_model.GetType()));
    
    // Pthread-specific operations
    pthread_model.Init();
    
    // Create thread with pthread model
    hshm::ThreadGroup group = pthread_model.CreateThreadGroup(hshm::ThreadGroupContext{});
    
    auto pthread_worker = []() {
        printf("Running in pthread worker\n");
        
        // Get pthread-specific thread ID
        auto tid = HSHM_THREAD_MODEL->GetTid();
        printf("Pthread TID: %zu\n", tid.tid_);
        
        // Use pthread-specific sleep
        HSHM_THREAD_MODEL->SleepForUs(500000);
    };
    
    hshm::Thread thread = pthread_model.Spawn(group, pthread_worker);
    pthread_model.Join(thread);
}

#endif
```

### Standard Thread Implementation

```cpp
void std_thread_example() {
    // Create std::thread-based model
    hshm::thread::StdThread std_model;
    
    printf("Using std::thread model\n");
    
    // Standard thread operations
    hshm::ThreadGroup group = std_model.CreateThreadGroup(hshm::ThreadGroupContext{});
    
    auto std_worker = [](const std::string& message) {
        printf("std::thread worker: %s\n", message.c_str());
        
        // Use std::thread sleep mechanisms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Get thread ID
        auto tid = std::this_thread::get_id();
        std::cout << "Thread ID: " << tid << std::endl;
    };
    
    std::vector<hshm::Thread> threads;
    for (int i = 0; i < 3; ++i) {
        std::string msg = "Message from thread " + std::to_string(i);
        threads.push_back(std_model.Spawn(group, std_worker, msg));
    }
    
    for (auto& thread : threads) {
        std_model.Join(thread);
    }
}
```

## Cross-Device Compatibility

### Host and GPU Thread Coordination

```cpp
HSHM_CROSS_FUN void cross_device_function() {
    // This function works on both host and GPU
    auto* tm = HSHM_THREAD_MODEL;
    
#if HSHM_IS_HOST
    printf("Running on host with thread model: %d\n", 
           static_cast<int>(tm->GetType()));
#elif HSHM_IS_GPU
    // GPU-specific operations
    int thread_id = threadIdx.x + blockIdx.x * blockDim.x;
    printf("Running on GPU, thread %d\n", thread_id);
#endif
    
    // Common operations that work on both
    tm->Yield();
}

void cross_device_example() {
    // Host execution
    cross_device_function();
    
#if HSHM_ENABLE_CUDA
    // Launch on GPU
    cross_device_function<<<1, 32>>>();
    cudaDeviceSynchronize();
#endif
}
```

## Thread Synchronization Patterns

### Barrier Implementation

```cpp
class ThreadBarrier {
    std::mutex mutex_;
    std::condition_variable condition_;
    size_t thread_count_;
    size_t waiting_count_;
    size_t barrier_generation_;
    
public:
    explicit ThreadBarrier(size_t count) 
        : thread_count_(count), waiting_count_(0), barrier_generation_(0) {}
    
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        size_t current_generation = barrier_generation_;
        
        if (++waiting_count_ == thread_count_) {
            // Last thread to arrive
            waiting_count_ = 0;
            barrier_generation_++;
            condition_.notify_all();
        } else {
            // Wait for all threads to arrive
            condition_.wait(lock, [this, current_generation] {
                return current_generation != barrier_generation_;
            });
        }
    }
};

void barrier_example() {
    const int num_threads = 4;
    ThreadBarrier barrier(num_threads);
    hshm::ipc::atomic<int> phase(0);
    
    auto barrier_worker = [&](int worker_id) {
        for (int i = 0; i < 3; ++i) {
            // Phase 1: Different amounts of work
            HSHM_THREAD_MODEL->SleepForUs(100000 + worker_id * 50000);
            printf("Worker %d completed phase %d work\n", worker_id, i + 1);
            
            // Synchronize at barrier
            printf("Worker %d waiting at barrier for phase %d\n", worker_id, i + 1);
            barrier.Wait();
            
            // All threads continue together
            if (worker_id == 0) {
                int current_phase = phase.fetch_add(1) + 1;
                printf("=== All threads synchronized, starting phase %d ===\n", 
                       current_phase);
            }
        }
        
        printf("Worker %d finished all phases\n", worker_id);
    };
    
    auto* tm = HSHM_THREAD_MODEL;
    hshm::ThreadGroup group = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
    std::vector<hshm::Thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(tm->Spawn(group, barrier_worker, i));
    }
    
    for (auto& thread : threads) {
        tm->Join(thread);
    }
    
    printf("All workers completed\n");
}
```

## Performance Monitoring

```cpp
class ThreadPerformanceMonitor {
    struct ThreadStats {
        hshm::ipc::atomic<size_t> tasks_completed{0};
        hshm::ipc::atomic<size_t> total_execution_time_us{0};
        hshm::ipc::atomic<size_t> max_execution_time_us{0};
        std::chrono::high_resolution_clock::time_point start_time;
    };
    
    std::unordered_map<size_t, std::unique_ptr<ThreadStats>> thread_stats_;
    std::mutex stats_mutex_;
    
public:
    void StartTask(size_t thread_id) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (thread_stats_.find(thread_id) == thread_stats_.end()) {
            thread_stats_[thread_id] = std::make_unique<ThreadStats>();
        }
        thread_stats_[thread_id]->start_time = std::chrono::high_resolution_clock::now();
    }
    
    void EndTask(size_t thread_id) {
        auto end_time = std::chrono::high_resolution_clock::now();
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto it = thread_stats_.find(thread_id);
        if (it != thread_stats_.end()) {
            auto& stats = *it->second;
            
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - stats.start_time).count();
            
            stats.tasks_completed.fetch_add(1);
            stats.total_execution_time_us.fetch_add(duration);
            
            // Update max execution time
            size_t current_max = stats.max_execution_time_us.load();
            while (duration > current_max && 
                   !stats.max_execution_time_us.compare_exchange_weak(current_max, duration)) {
            }
        }
    }
    
    void PrintStatistics() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        printf("\n=== Thread Performance Statistics ===\n");
        printf("%-10s %-10s %-15s %-15s %-15s\n", 
               "ThreadID", "Tasks", "Total(μs)", "Avg(μs)", "Max(μs)");
        
        for (const auto& [thread_id, stats] : thread_stats_) {
            size_t tasks = stats->tasks_completed.load();
            size_t total_time = stats->total_execution_time_us.load();
            size_t max_time = stats->max_execution_time_us.load();
            double avg_time = tasks > 0 ? double(total_time) / tasks : 0.0;
            
            printf("%-10zu %-10zu %-15zu %-15.1f %-15zu\n",
                   thread_id, tasks, total_time, avg_time, max_time);
        }
    }
};

void performance_monitoring_example() {
    ThreadPerformanceMonitor monitor;
    auto* tm = HSHM_THREAD_MODEL;
    
    auto monitored_worker = [&](int worker_id) {
        size_t thread_id = tm->GetTid().tid_;
        
        for (int i = 0; i < 5; ++i) {
            monitor.StartTask(thread_id);
            
            // Simulate variable work
            size_t work_time = 100000 + (rand() % 200000);  // 100-300ms
            tm->SleepForUs(work_time);
            
            monitor.EndTask(thread_id);
            
            printf("Worker %d (TID %zu) completed task %d\n", 
                   worker_id, thread_id, i + 1);
        }
    };
    
    hshm::ThreadGroup group = tm->CreateThreadGroup(hshm::ThreadGroupContext{});
    std::vector<hshm::Thread> threads;
    
    const int num_workers = 3;
    for (int i = 0; i < num_workers; ++i) {
        threads.push_back(tm->Spawn(group, monitored_worker, i));
    }
    
    for (auto& thread : threads) {
        tm->Join(thread);
    }
    
    monitor.PrintStatistics();
}
```

## Best Practices

1. **Thread Model Selection**: Use `HSHM_THREAD_MODEL` for automatic platform-appropriate threading
2. **Cross-Platform Code**: Use `HSHM_CROSS_FUN` for functions that work on both host and device
3. **Thread Local Storage**: Implement proper cleanup in TLS destructors
4. **Resource Management**: Always join threads before destroying thread groups
5. **Error Handling**: Wrap thread operations in try-catch blocks for robust error handling
6. **Performance**: Use appropriate thread models - Pthread for system integration, StdThread for portability
7. **Synchronization**: Prefer atomic operations over locks when possible for performance
8. **Debugging**: Use thread IDs and names for easier debugging in multi-threaded applications
9. **Memory Management**: Be careful with shared data - use atomic types or proper synchronization
10. **Testing**: Test threading code under high load and stress conditions to verify correctness

## Thread Model Configuration

The thread models are configured at compile time through CMake defines:

- `HSHM_DEFAULT_THREAD_MODEL=hshm::thread::Pthread` (Host default)
- `HSHM_DEFAULT_THREAD_MODEL_GPU=hshm::thread::StdThread` (GPU default)
- Enable specific models: `HSHM_ENABLE_PTHREADS`, `HSHM_ENABLE_CUDA`, `HSHM_ENABLE_THALLIUM`

Different thread models can be enabled or disabled based on system capabilities and requirements.

# HSHM Timer Utilities Guide

## Overview

The Timer Utilities API in Hermes Shared Memory (HSHM) provides high-resolution timing capabilities for performance measurement, profiling, and benchmarking. The API includes basic timers, MPI-aware distributed timing, thread-local timing, and periodic execution utilities.

## Core Timer Classes

### Basic High-Resolution Timer

```cpp
#include "hermes_shm/util/timer.h"

void basic_timing_example() {
    // Create a high-resolution timer
    hshm::Timer timer;
    
    // Start timing
    timer.Reset();  // Starts timer and resets accumulated time
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Pause and get elapsed time
    timer.Pause();  // Adds elapsed time to accumulated total
    
    // Access timing results
    double elapsed_ns = timer.GetNsec();      // Nanoseconds
    double elapsed_us = timer.GetUsec();      // Microseconds  
    double elapsed_ms = timer.GetMsec();      // Milliseconds
    double elapsed_s = timer.GetSec();        // Seconds
    
    printf("Operation took %.2f milliseconds\n", elapsed_ms);
    
    // Resume timing for additional work
    timer.Resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.Pause();
    
    printf("Total time: %.2f milliseconds\n", timer.GetMsec());
}
```

### Timer Types Available

```cpp
// Different timer implementations
hshm::HighResCpuTimer cpu_timer;           // std::chrono::high_resolution_clock
hshm::HighResMonotonicTimer mono_timer;    // std::chrono::steady_clock (recommended)
hshm::Timer default_timer;                 // Alias for HighResMonotonicTimer

// Timepoint classes for manual timing
hshm::HighResCpuTimepoint cpu_timepoint;
hshm::HighResMonotonicTimepoint mono_timepoint;
hshm::Timepoint default_timepoint;         // Alias for HighResMonotonicTimepoint

void timepoint_example() {
    hshm::Timepoint start, end;
    
    start.Now();  // Capture current time
    
    // Do work
    expensive_computation();
    
    end.Now();
    double elapsed_ms = end.GetMsecFromStart(start);
    printf("Computation took %.2f milliseconds\n", elapsed_ms);
}
```

## MPI Distributed Timing

```cpp
#include "hermes_shm/util/timer_mpi.h"

#if HSHM_ENABLE_MPI
void mpi_timing_example(MPI_Comm comm) {
    hshm::MpiTimer mpi_timer(comm);
    
    // Each rank performs timing
    mpi_timer.Reset();
    
    // Simulate different work on each rank
    int rank;
    MPI_Comm_rank(comm, &rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    
    mpi_timer.Pause();
    
    // Collect timing statistics across all ranks
    
    // Get maximum time across all ranks
    mpi_timer.CollectMax();
    if (rank == 0) {
        printf("Max time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
    
    // Get minimum time across all ranks  
    mpi_timer.Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    mpi_timer.Pause();
    mpi_timer.CollectMin();
    if (rank == 0) {
        printf("Min time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
    
    // Get average time across all ranks (default)
    mpi_timer.Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    mpi_timer.Pause();
    mpi_timer.Collect();  // Same as CollectAvg()
    if (rank == 0) {
        printf("Average time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
}
#endif
```

## Thread-Local Timing

```cpp
#include "hermes_shm/util/timer_thread.h"

class WorkerPool {
    std::vector<std::thread> workers_;
    hshm::ThreadTimer thread_timer_;
    
public:
    explicit WorkerPool(int num_threads) : thread_timer_(num_threads) {
        for (int i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i]() {
                WorkerThread(i);
            });
        }
    }
    
    void Join() {
        for (auto& worker : workers_) {
            worker.join();
        }
        
        // Collect timing from all threads
        thread_timer_.Collect();
        printf("Max thread time: %.2f ms\n", thread_timer_.GetMsec());
    }
    
private:
    void WorkerThread(int thread_id) {
        // Set thread rank for timing
        thread_timer_.SetRank(thread_id);
        
        // Perform timed work
        thread_timer_.Reset();
        
        // Simulate different amounts of work per thread
        for (int i = 0; i < 1000 * (thread_id + 1); ++i) {
            // Some computation
            volatile double x = sin(i * 0.001);
            (void)x;  // Prevent optimization
        }
        
        thread_timer_.Pause();
        
        printf("Thread %d completed in %.2f ms\n", 
               thread_id, thread_timer_.timers_[thread_id].GetMsec());
    }
};

void thread_timing_example() {
    const int num_threads = 4;
    WorkerPool pool(num_threads);
    
    pool.Join();
}
```

## Best Practices

1. **Timer Choice**: Use `hshm::Timer` (monotonic) for measuring durations, avoid CPU timers that can be affected by frequency scaling
2. **MPI Timing**: Use `MpiTimer` for measuring distributed operations and getting consistent timing across ranks
3. **Thread Safety**: `ThreadTimer` provides thread-local timing; use `TimerPool` for complex multi-threaded scenarios
4. **Periodic Operations**: Use `HSHM_PERIODIC` macros for regular maintenance tasks without additional timer overhead
5. **Warm-up**: Always perform warm-up runs before benchmarking to account for CPU frequency scaling and cache effects
6. **Statistical Analysis**: Use multiple measurements and calculate statistics for reliable performance characterization
7. **Overhead Awareness**: Be aware of timing overhead (typically 10-100ns) when measuring very short operations
8. **Cross-Platform**: All timers work consistently across different platforms and provide nanosecond precision
9. **Memory Management**: Timers are lightweight but consider pooling for high-frequency timing scenarios
10. **Integration**: Combine with profiling tools and performance monitoring systems for comprehensive analysis

# delay_ar: Delayed Archive for Shared Memory Objects

## Overview

The `delay_ar<T>` class (located in `include/hermes_shm/data_structures/internal/shm_archive.h`) is a fundamental building block in HSHM that provides delayed initialization for objects in shared memory. It acts as a wrapper that delays the construction of an object until it's explicitly initialized, which is crucial for shared memory data structures that need careful initialization timing.

## Key Features

- **Delayed Initialization**: Objects are not constructed until `shm_init()` is called
- **Type-Aware Allocation**: Automatically handles both SHM-aware and primitive types
- **Memory Layout Control**: Provides exact control over object placement in shared memory
- **Serialization Support**: Integrated with serialization frameworks like Cereal
- **Zero-Overhead Access**: Direct pointer access with minimal overhead

## Template Parameters

- `T`: The type of object to be stored and managed

## Core Methods

### Initialization

```cpp
// Initialize with no arguments (for default constructible types)
void shm_init();

// Initialize with arguments
template<typename... Args>
void shm_init(Args&&... args);

// Initialize with allocator (for SHM-aware types)
template<typename AllocT, typename... Args>  
void shm_init(AllocT&& alloc, Args&&... args);

// Initialize piecewise (advanced usage)
template<typename ArgPackT_1, typename ArgPackT_2>
void shm_init_piecewise(ArgPackT_1&& args1, ArgPackT_2&& args2);
```

### Access

```cpp
T* get();                    // Get pointer to object
const T* get() const;        // Get const pointer
T& get_ref();               // Get reference to object  
const T& get_ref() const;   // Get const reference
T& operator*();             // Dereference operator
T* operator->();            // Arrow operator
```

### Destruction

```cpp
void shm_destroy();  // Explicitly destroy the contained object
```

## Usage Examples

### Basic Primitive Types

```cpp
#include "hermes_shm/data_structures/internal/shm_archive.h"

// Integer storage
hipc::delay_ar<int> int_archive;
int_archive.shm_init(42);
std::cout << *int_archive << std::endl;  // Prints: 42

// String storage  
hipc::delay_ar<std::string> string_archive;
string_archive.shm_init("Hello, World!");
std::cout << string_archive->c_str() << std::endl;  // Prints: Hello, World!
```

### SHM-Aware Container Types

```cpp
#include "hermes_shm/data_structures/ipc/vector.h"

// Vector with allocator
auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<hipc::StackAllocator>();
hipc::delay_ar<hipc::vector<int>> vec_archive;

// For SHM-aware types, pass allocator first
vec_archive.shm_init(alloc, 100);  // Create vector with capacity 100
vec_archive->push_back(1);
vec_archive->push_back(2);
vec_archive->push_back(3);

std::cout << "Vector size: " << vec_archive->size() << std::endl;
```

### Custom Object Initialization

```cpp
struct CustomObject {
    int value;
    std::string name;
    
    CustomObject(int v, const std::string& n) : value(v), name(n) {}
};

hipc::delay_ar<CustomObject> custom_archive;
custom_archive.shm_init(100, "MyObject");

std::cout << custom_archive->name << ": " << custom_archive->value << std::endl;
```

### Pair Example (Common Pattern)

```cpp
#include "hermes_shm/data_structures/ipc/pair.h"

// Pair of integers
hipc::delay_ar<hipc::pair<int, std::string>> pair_archive;
auto alloc = GetSomeAllocator();
pair_archive.shm_init(alloc);

// Access pair elements
pair_archive->first_.shm_init(alloc, 42);
pair_archive->second_.shm_init(alloc, "Hello");

std::cout << "First: " << *pair_archive->first_ << std::endl;
std::cout << "Second: " << *pair_archive->second_ << std::endl;
```

### Array of Objects

```cpp
// Array of delay_ar objects
constexpr size_t ARRAY_SIZE = 10;
hipc::delay_ar<int> int_array[ARRAY_SIZE];

// Initialize each element
for (size_t i = 0; i < ARRAY_SIZE; ++i) {
    int_array[i].shm_init(static_cast<int>(i * 10));
}

// Access elements
for (size_t i = 0; i < ARRAY_SIZE; ++i) {
    std::cout << "Element " << i << ": " << *int_array[i] << std::endl;
}
```

### Cleanup Example

```cpp
hipc::delay_ar<std::vector<int>> vector_archive;
vector_archive.shm_init();
vector_archive->push_back(1);

// Explicit cleanup when done
vector_archive.shm_destroy();
```

## Type Behavior Differences

### SHM-Aware Types (inherit from ShmContainer)

SHM-aware types like `hipc::vector`, `hipc::string`, `hipc::pair` require an allocator:

```cpp
auto alloc = GetAllocator();
hipc::delay_ar<hipc::vector<int>> shm_vec;
shm_vec.shm_init(alloc, initial_capacity);  // Allocator passed as first argument
```

### Non-SHM-Aware Types (primitives, STL types)

Regular types don't need allocators:

```cpp
hipc::delay_ar<std::vector<int>> std_vec;
std_vec.shm_init(initial_capacity);  // No allocator needed
```

## Advanced Usage: Piecewise Construction

For complex initialization scenarios:

```cpp
hipc::delay_ar<std::pair<std::string, int>> complex_pair;
complex_pair.shm_init_piecewise(
    make_argpack("First string"),    // Arguments for first element
    make_argpack(42)                 // Arguments for second element
);
```

## Serialization Support

The `delay_ar` class integrates with serialization frameworks:

```cpp
#include <cereal/archives/binary.hpp>

hipc::delay_ar<hipc::vector<int>> vec_archive;
vec_archive.shm_init(alloc, 10);
vec_archive->push_back(1);
vec_archive->push_back(2);

// Serialize
std::ostringstream os;
cereal::BinaryOutputArchive output_archive(os);
output_archive << vec_archive;

// Deserialize  
hipc::delay_ar<hipc::vector<int>> restored_archive;
std::istringstream is(os.str());
cereal::BinaryInputArchive input_archive(is);
input_archive >> restored_archive;  // Automatically calls shm_init
```

## Best Practices

1. **Always Initialize**: Never access a `delay_ar` object without calling `shm_init()` first
2. **Match Constructor Signatures**: Ensure arguments to `shm_init()` match the target type's constructor
3. **Allocator Consistency**: For SHM-aware types, use the same allocator throughout related objects
4. **Explicit Cleanup**: Call `shm_destroy()` when manual cleanup is needed
5. **Exception Safety**: Be aware that initialization can throw exceptions

## Common Patterns in HSHM Codebase

### Container Member Variables

```cpp
class MyDataStructure : public hipc::ShmContainer {
private:
    hipc::delay_ar<hipc::vector<int>> data_;
    hipc::delay_ar<hipc::string> name_;

public:
    void shm_init(hipc::Allocator* alloc, const std::string& name) {
        init_shm_container(alloc);
        data_.shm_init(GetCtxAllocator(), 100);
        name_.shm_init(GetCtxAllocator(), name);
    }
};
```

### Array Initialization in Allocators

```cpp
class PageAllocator {
    hipc::delay_ar<LifoListQueue> free_lists_[NUM_CACHES];

public:
    void Initialize(hipc::Allocator* alloc) {
        for (size_t i = 0; i < NUM_CACHES; ++i) {
            free_lists_[i].shm_init(alloc);
        }
    }
};
```

# Hermes SHM Logging Guide

This guide covers the HILOG and HELOG logging macros provided by Hermes Shared Memory (HSHM) for structured logging and error reporting.

## Overview

The Hermes SHM logging system provides two main macros for different types of logging:
- `HILOG`: For informational logging
- `HELOG`: For error logging

Both macros are built on top of the underlying `HLOG` macro and provide structured, thread-safe logging with configurable verbosity levels.

## Log Levels

The system defines several predefined log levels:

| Level     | Code | Description                        | Output  |
|-----------|------|------------------------------------|---------|
| `kInfo`   | 251  | Useful information for users       | stdout  |
| `kWarning`| 252  | Something might be wrong           | stderr  |
| `kError`  | 253  | A non-fatal error has occurred     | stderr  |
| `kFatal`  | 254  | A fatal error (causes program exit)| stderr  |
| `kDebug`  | 255/-1| Low-priority debugging info       | stdout  |

## HILOG (Hermes Info Log)

### Syntax
```cpp
HILOG(SUB_CODE, format_string, ...args)
```

### Purpose
Logs informational messages at the `kInfo` level. These messages are displayed on stdout and provide useful information to users about program execution.

### Parameters
- `SUB_CODE`: A sub-category code to further classify the log message
- `format_string`: Printf-style format string
- `...args`: Arguments for the format string

### Output Format
```
filepath:line INFO thread_id function_name message
```

### Examples

#### Basic Information Logging
```cpp
HILOG(kInfo, "Server started on port {}", 8080);
// Output: /path/to/file.cc:45 INFO 12345 main Server started on port 8080
```

#### Performance Metrics
```cpp
HILOG(kInfo, "{},{},{},{},{},{} ms,{} KOps", 
      test_name, alloc_type, obj_size, msec, nthreads, count, kops);
// Output: /path/to/file.cc:170 INFO 12345 benchmark_func test_malloc,malloc,1024,50 ms,4,1000000 KOps
```

#### Debug Logging (Debug Builds Only)
```cpp
HILOG(kDebug, "Acquired read lock for {}", owner);
// Output (debug builds): /path/to/file.cc:108 INFO 12345 acquire_lock Acquired read lock for thread_123
```

#### Status Messages
```cpp
HILOG(kInfo, "Lz4: output buffer is potentially too small");
HILOG(kInfo, "test_name,alloc_type,obj_size,msec,nthreads,count,KOps");
```

## HELOG (Hermes Error Log)

### Syntax
```cpp
HELOG(LOG_CODE, format_string, ...args)
```

### Purpose
Logs error messages using the same code for both the primary log code and sub-code. These messages are displayed on stderr and indicate various levels of problems.

### Parameters
- `LOG_CODE`: Error level (`kError`, `kFatal`, `kWarning`)
- `format_string`: Printf-style format string  
- `...args`: Arguments for the format string

### Output Format
```
filepath:line LEVEL thread_id function_name message
```

### Examples

#### Fatal Errors (Program Termination)
```cpp
HELOG(kFatal, "Could not find this allocator type");
// Output: /path/to/file.cc:63 FATAL 12345 init_allocator Could not find this allocator type
// Program exits after this message

HELOG(kFatal, "Failed to find the memory allocator?");
HELOG(kFatal, "Exception: {}", e.what());
```

#### Non-Fatal Errors
```cpp
HELOG(kError, "shm_open failed: {}", err_buf);
// Output: /path/to/file.cc:66 ERROR 12345 open_shared_memory shm_open failed: Permission denied

HELOG(kError, "Failed to generate key");
```

#### System/Hardware Errors
```cpp
// CUDA error handling
HELOG(kFatal, "CUDA Error {}: {}", cudaErr, cudaGetErrorString(cudaErr));

// HIP error handling  
HELOG(kFatal, "HIP Error {}: {}", hipErr, hipGetErrorString(hipErr));
```

## Advanced Features

### Periodic Logging
For messages that might be called frequently, use `HILOG_PERIODIC` to limit output frequency:

```cpp
HILOG_PERIODIC(kInfo, unique_id, interval_seconds, "Status update: {}", status);
```

### Environment Configuration

#### Disabling Log Codes
Set `HSHM_LOG_EXCLUDE` to a comma-separated list of log codes to disable:
```bash
export HSHM_LOG_EXCLUDE="251,252"  # Disable kInfo and kWarning
```

#### Log File Output
Set `HSHM_LOG_OUT` to write logs to a file (in addition to console):
```bash
export HSHM_LOG_OUT="/tmp/hermes_shm.log"
```

### Debug Builds
- In release builds: `kDebug` is defined as -1, and debug logs are compiled out
- In debug builds: `kDebug` is defined as 255, and debug logs are active

## Best Practices

1. **Use appropriate log levels**:
   - `HILOG(kInfo, ...)` for normal operational messages
   - `HELOG(kError, ...)` for recoverable errors
   - `HELOG(kFatal, ...)` for unrecoverable errors that should terminate the program

2. **Include context in error messages**:
   ```cpp
   HELOG(kError, "Failed to allocate {} bytes: {}", size, strerror(errno));
   ```

3. **Use meaningful sub-codes** for `HILOG` to categorize different types of information

4. **Format structured data consistently**:
   ```cpp
   HILOG(kInfo, "operation={},duration_ms={},status={}", op_name, duration, status);
   ```

5. **Avoid logging in tight loops** - use `HILOG_PERIODIC` instead

## Thread Safety

The logging system is thread-safe and automatically includes thread IDs in log output, making it suitable for multi-threaded applications.

## Performance Considerations

- Log messages are formatted only when the log level is enabled
- Disabled log codes (via `HSHM_LOG_EXCLUDE`) have minimal runtime overhead
- Debug logs have zero overhead in release builds due to compile-time optimization