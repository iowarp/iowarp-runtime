# Ring Queue Documentation

This document describes the ring queue data structures in Hermes SHM, including `ring_queue_base` and `ring_ptr_queue_base`, both of which now support optional header storage.

## Overview

Ring queues are circular buffer implementations optimized for high-performance producer-consumer scenarios. They support various synchronization patterns:
- Single Producer Single Consumer (SPSC)
- Multiple Producer Single Consumer (MPSC)
- Fixed-size variants
- Circular variants
- Extensible variants

## Template Parameters

Both `ring_queue_base` and `ring_ptr_queue_base` now support the following template parameters:

### `ring_queue_base<T, HDR, RQ_FLAGS, AllocT, HSHM_FLAGS>`

- **`T`**: The type of data elements stored in the queue
- **`HDR`**: Optional header type for storing additional metadata (default: `EmptyHeader`)
- **`RQ_FLAGS`**: Configuration flags controlling queue behavior
- **`AllocT`**: Allocator type for shared memory management
- **`HSHM_FLAGS`**: Shared memory flags

### `ring_ptr_queue_base<T, HDR, RQ_FLAGS, AllocT, HSHM_FLAGS>`

Same template parameters as `ring_queue_base`, but optimized for storing pointers and pointer-like types.

## Header Support

The `HDR` template parameter allows you to store additional metadata alongside the queue. This header is stored once per queue instance and can contain configuration data, statistics, or any other per-queue information.

### Empty Header (Default)

When no header is needed, use the default `EmptyHeader` struct:

```cpp
#include "hermes_shm/data_structures/ipc/ring_queue.h"

// Uses EmptyHeader by default - no additional memory overhead
hshm::spsc_queue<int> queue(1024);
```

### Custom Header

Define a custom header structure to store additional data:

```cpp
struct MyHeader {
    uint64_t creation_time;
    uint32_t producer_id;
    uint32_t version;
    char name[64];
};

// Queue with custom header
hshm::spsc_queue<int, MyHeader> queue(1024);

// Access header
MyHeader& header = queue.GetHeader();
header.creation_time = std::time(nullptr);
header.producer_id = 42;
strncpy(header.name, "my_queue", sizeof(header.name));

// Read header
const MyHeader& const_header = queue.GetHeader();
std::cout << "Producer ID: " << const_header.producer_id << std::endl;
```

## Queue Variants

### Standard Ring Queues (ring_queue_base)

These queues store actual data elements and support arbitrary types:

#### SPSC Queue
```cpp
// Single Producer Single Consumer
hshm::spsc_queue<MyData> queue(depth);
hshm::spsc_queue<MyData, MyHeader> queue_with_header(depth);
```

#### MPSC Queue
```cpp
// Multiple Producer Single Consumer
hshm::mpsc_queue<MyData> queue(depth);
hshm::mpsc_queue<MyData, MyHeader> queue_with_header(depth);
```

#### Fixed Size Variants
```cpp
// Fixed size - no dynamic resizing
hshm::fixed_spsc_queue<MyData> queue(depth);
hshm::fixed_mpsc_queue<MyData, MyHeader> queue(depth);
```

#### Circular Variants
```cpp
// Circular behavior on overflow
hshm::circular_spsc_queue<MyData> queue(depth);
hshm::circular_mpsc_queue<MyData, MyHeader> queue(depth);
```

#### Extensible Variant
```cpp
// Automatically grows when full
hshm::ext_ring_buffer<MyData> queue(initial_depth);
hshm::ext_ring_buffer<MyData, MyHeader> queue(initial_depth);
```

### Pointer Ring Queues (ring_ptr_queue_base)

These queues are optimized for storing pointers, raw pointers, and pointer-like types using bit manipulation for synchronization:

```cpp
// For raw pointers
hshm::spsc_ptr_queue<int*> ptr_queue(depth);
hshm::spsc_ptr_queue<int*, MyHeader> ptr_queue_with_header(depth);

// For shared memory pointers
hshm::spsc_ptr_queue<hipc::ShmPtr<MyData>> shm_ptr_queue(depth);

// For arithmetic types (uses bit marking)
hshm::spsc_ptr_queue<uint64_t> int_queue(depth);
```

All variants support headers:
```cpp
hshm::mpsc_ptr_queue<void*, MyHeader> ptr_queue(depth);
hshm::fixed_spsc_ptr_queue<int*, MyHeader> fixed_ptr_queue(depth);
hshm::circular_mpsc_ptr_queue<uint64_t, MyHeader> circular_ptr_queue(depth);
```

## Multi-Lane Ring Queues

Multi-lane ring queues provide high-performance concurrent access by partitioning data across multiple lanes and priority levels. They are built on top of `multi_ring_buffer` and support all the same header functionality.

### Multi-Queue Variants

#### Multi-Lane MPSC Queue
```cpp
// Basic multi-lane queue with 4 lanes, 2 priorities, depth 1024 per queue
hshm::multi_mpsc_queue<WorkItem> multi_queue(4, 2, 1024);

// With custom header
hshm::multi_mpsc_queue<WorkItem, MyHeader> multi_queue_with_header(4, 2, 1024);
```

#### Multi-Lane SPSC Queue
```cpp
// Single producer per lane, single consumer overall
hshm::multi_spsc_queue<WorkItem> multi_queue(num_lanes, num_priorities, depth);
hshm::multi_spsc_queue<WorkItem, MyHeader> multi_queue(num_lanes, num_priorities, depth);
```

#### Multi-Lane Fixed and Circular Variants
```cpp
// Fixed-size multi-lane queues (no dynamic resizing)
hshm::multi_fixed_mpsc_queue<WorkItem> fixed_multi_queue(4, 2, 512);
hshm::multi_fixed_mpsc_queue<WorkItem, MyHeader> fixed_multi_queue(4, 2, 512);

// Circular multi-lane queues (overwrite on overflow)
hshm::multi_circular_mpsc_queue<WorkItem> circular_multi_queue(4, 2, 256);
hshm::multi_circular_mpsc_queue<WorkItem, MyHeader> circular_multi_queue(4, 2, 256);
```

### Multi-Queue Operations

```cpp
// Initialize multi-queue with allocator
auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator();
hshm::multi_mpsc_queue<int, MyHeader> multi_queue(alloc, 4, 3, 1024);

// Access header for the entire multi-queue
auto& header = multi_queue.GetHeader();
header.creation_time = std::time(nullptr);

// Enqueue to specific lane and priority
qtok_t token = multi_queue.Enqueue(42, 1, 0); // lane_id=1, priority=0

// Enqueue using round-robin lane selection
qtok_t token2 = multi_queue.EnqueueRoundRobin(100, 0); // priority=0, auto lane

// Dequeue from highest priority across all lanes
int value;
qtok_t result = multi_queue.Dequeue(value);
if (!result.IsNull()) {
    std::cout << "Dequeued: " << value << std::endl;
}

// Dequeue from specific lane and priority
qtok_t specific_result = multi_queue.Dequeue(value, 2, 1); // lane_id=2, priority=1

// Get direct access to a specific lane's queue
const auto& lane_queue = multi_queue.GetLane(1, 0); // lane_id=1, priority=0
size_t lane_size = lane_queue.GetSize();

// Get multi-queue dimensions
size_t num_lanes = multi_queue.GetNumLanes();
size_t num_priorities = multi_queue.GetNumPriorities();
```

### Multi-Queue Use Cases

Multi-lane queues are ideal for:
- **Producer Partitioning**: Different producers can use different lanes to reduce contention
- **Priority-based Processing**: Higher priority items can be processed first
- **Load Balancing**: Round-robin distribution across lanes
- **Hierarchical Workloads**: Different types of work can use different priority levels

### Example: Multi-Lane Work Queue with Statistics Header

```cpp
struct WorkQueueStats {
    std::atomic<uint64_t> total_jobs{0};
    std::atomic<uint64_t> high_priority_jobs{0};
    std::atomic<uint64_t> jobs_per_lane[8] = {0}; // Support up to 8 lanes
    uint64_t created_timestamp;
    
    WorkQueueStats() : created_timestamp(std::time(nullptr)) {}
    
    void on_enqueue(size_t lane_id, size_t priority) {
        total_jobs.fetch_add(1);
        if (lane_id < 8) {
            jobs_per_lane[lane_id].fetch_add(1);
        }
        if (priority > 0) {
            high_priority_jobs.fetch_add(1);
        }
    }
    
    double get_lane_utilization(size_t lane_id) const {
        if (lane_id >= 8) return 0.0;
        uint64_t total = total_jobs.load();
        if (total == 0) return 0.0;
        return static_cast<double>(jobs_per_lane[lane_id].load()) / total;
    }
};

struct WorkItem {
    int task_id;
    int data;
    int priority_level;
};

class MultiWorkQueue {
    hshm::multi_mpsc_queue<WorkItem, WorkQueueStats> queue_;
    size_t num_lanes_;
    
public:
    MultiWorkQueue(size_t num_lanes, size_t num_priorities, size_t depth_per_queue)
        : queue_(num_lanes, num_priorities, depth_per_queue), num_lanes_(num_lanes) {
        // Header is automatically initialized
    }
    
    qtok_t submit_work(const WorkItem& item, size_t target_lane = SIZE_MAX) {
        size_t lane_id;
        if (target_lane == SIZE_MAX) {
            // Use round-robin if no specific lane requested
            qtok_t result = queue_.EnqueueRoundRobin(item, item.priority_level);
            if (!result.IsNull()) {
                // Estimate lane from round-robin counter (approximate)
                lane_id = result.id_ % num_lanes_;
                queue_.GetHeader().on_enqueue(lane_id, item.priority_level);
            }
            return result;
        } else {
            qtok_t result = queue_.Enqueue(item, target_lane, item.priority_level);
            if (!result.IsNull()) {
                queue_.GetHeader().on_enqueue(target_lane, item.priority_level);
            }
            return result;
        }
    }
    
    qtok_t get_work(WorkItem& item) {
        return queue_.Dequeue(item);
    }
    
    const WorkQueueStats& stats() const {
        return queue_.GetHeader();
    }
    
    void print_stats() const {
        const auto& stats = queue_.GetHeader();
        std::cout << "Total jobs: " << stats.total_jobs.load() << std::endl;
        std::cout << "High priority jobs: " << stats.high_priority_jobs.load() << std::endl;
        
        for (size_t i = 0; i < num_lanes_; ++i) {
            double util = stats.get_lane_utilization(i);
            std::cout << "Lane " << i << " utilization: " << (util * 100.0) << "%" << std::endl;
        }
    }
};

// Usage example
MultiWorkQueue work_queue(4, 3, 1000); // 4 lanes, 3 priorities, 1000 depth each

// Submit work to different lanes
work_queue.submit_work({1, 100, 1}, 0); // Specific lane 0, priority 1
work_queue.submit_work({2, 200, 2});    // Round-robin lane, priority 2
work_queue.submit_work({3, 300, 0}, 2); // Specific lane 2, priority 0

// Process work
WorkItem item;
while (!work_queue.get_work(item).IsNull()) {
    std::cout << "Processing task " << item.task_id 
              << " with priority " << item.priority_level << std::endl;
    // Process work...
}

// Show statistics
work_queue.print_stats();
```

## Basic Operations

### Initialization

```cpp
// Create allocator
auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator();

// Initialize queue
hshm::spsc_queue<int, MyHeader> queue(alloc, 1024);

// Initialize header if using custom header
if constexpr (!std::is_same_v<MyHeader, hshm::ipc::EmptyHeader>) {
    auto& header = queue.GetHeader();
    // Initialize header fields...
}
```

### Producer Operations

```cpp
// Push/emplace elements
qtok_t token1 = queue.push(42);
qtok_t token2 = queue.emplace(100);

// Check if operation succeeded
if (token1.IsNull()) {
    // Queue was full (for fixed-size queues)
}
```

### Consumer Operations

```cpp
// Pop elements
int value;
qtok_t token = queue.pop(value);

if (!token.IsNull()) {
    // Successfully popped value
    std::cout << "Popped: " << value << std::endl;
}

// Pop without retrieving value (ring_queue_base only)
qtok_t token2 = queue.pop();
```

### Peek Operations (ring_queue_base only)

```cpp
// Peek at elements without removing them
int* value_ptr;
qtok_t token = queue.peek(value_ptr);

if (!token.IsNull()) {
    std::cout << "Next value: " << *value_ptr << std::endl;
}

// Peek at specific offset
qtok_t token2 = queue.peek(value_ptr, 2); // Look ahead 2 elements
```

### Size and Status Operations

```cpp
// Check queue status
size_t current_size = queue.GetSize();
size_t max_depth = queue.GetDepth();
bool is_empty = queue.IsNull();

// Resize (for extensible queues)
queue.Resize(new_depth);
```

## Configuration Flags

The `RQ_FLAGS` parameter controls queue behavior:

- **`RING_BUFFER_SPSC_FLAGS`**: Single producer, single consumer
- **`RING_BUFFER_MPSC_FLAGS`**: Multiple producer, single consumer
- **`RING_BUFFER_FIXED_SPSC_FLAGS`**: Fixed-size SPSC
- **`RING_BUFFER_FIXED_MPMC_FLAGS`**: Fixed-size MPMC
- **`RING_BUFFER_CIRCULAR_SPSC_FLAGS`**: Circular SPSC
- **`RING_BUFFER_CIRCULAR_MPMC_FLAGS`**: Circular MPMC
- **`RING_BUFFER_EXTENSIBLE_FLAGS`**: Auto-growing buffer

## Memory Layout

### Without Header
```
[queue_vector][tail_][head_][flags_]
```

### With Header
```
[queue_vector][tail_][head_][flags_][header_]
```

The header is stored as a direct member of the queue class, so it's always accessible without additional pointer indirection.

## Thread Safety

- **SPSC queues**: Lock-free for single producer, single consumer
- **MPSC queues**: Lock-free for multiple producers, single consumer
- **Header access**: Headers should be initialized once and accessed primarily by the producer or using external synchronization

## Performance Considerations

1. **Header Size**: Keep headers small as they're stored with each queue instance
2. **EmptyHeader Optimization**: When using `EmptyHeader`, the compiler optimizes away storage (Empty Base Optimization)
3. **Pointer Queues**: More efficient for pointer-like data due to bit manipulation synchronization
4. **Fixed vs Extensible**: Fixed-size queues are faster but limited; extensible queues can grow but have resize overhead

## Example: Queue with Statistics Header

```cpp
struct QueueStats {
    std::atomic<uint64_t> total_enqueued{0};
    std::atomic<uint64_t> total_dequeued{0};
    uint64_t created_timestamp;
    
    QueueStats() : created_timestamp(std::time(nullptr)) {}
    
    void on_enqueue() { total_enqueued.fetch_add(1); }
    void on_dequeue() { total_dequeued.fetch_add(1); }
    
    uint64_t pending_count() const {
        return total_enqueued.load() - total_dequeued.load();
    }
};

class StatsQueue {
    hshm::mpsc_queue<WorkItem, QueueStats> queue_;
    
public:
    StatsQueue(size_t depth) : queue_(depth) {
        // Header is automatically initialized
    }
    
    qtok_t enqueue(const WorkItem& item) {
        qtok_t result = queue_.push(item);
        if (!result.IsNull()) {
            queue_.GetHeader().on_enqueue();
        }
        return result;
    }
    
    qtok_t dequeue(WorkItem& item) {
        qtok_t result = queue_.pop(item);
        if (!result.IsNull()) {
            queue_.GetHeader().on_dequeue();
        }
        return result;
    }
    
    const QueueStats& stats() const {
        return queue_.GetHeader();
    }
};
```

## Error Handling

- **Null tokens**: Operations return `qtok_t::GetNull()` on failure
- **Fixed queue overflow**: `push()` fails when queue is full
- **Empty queue**: `pop()` fails when queue is empty
- **Invalid peek**: `peek()` fails for out-of-range or invalid tokens

## Best Practices

1. **Choose appropriate queue type**: 
   - SPSC for single producer/consumer
   - MPSC for multiple producers  
   - Multi-lane queues for high-contention scenarios with multiple producers

2. **Size queues appropriately**: Balance memory usage with contention

3. **Initialize headers early**: Set up header data during queue creation

4. **Handle null tokens**: Always check return values for failure cases

5. **Use pointer queues for pointers**: More efficient than storing pointers in regular queues

6. **Consider header thread safety**: Synchronize header access if modified by multiple threads

7. **Multi-queue specific**:
   - **Lane allocation**: Use round-robin for load balancing, specific lanes for producer affinity
   - **Priority design**: Keep number of priorities small (2-4) for best performance
   - **Memory planning**: Total memory = num_lanes × num_priorities × depth_per_queue × element_size
   - **Producer partitioning**: Assign producers to specific lanes when possible to reduce contention
   - **Consumer strategy**: Process highest priority first, but avoid starvation of lower priorities