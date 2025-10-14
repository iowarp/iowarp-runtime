# HSHM Lightbeam Networking Guide

## Overview

Lightbeam is HSHM's high-performance networking abstraction layer that provides a unified interface for distributed data transfer. The current implementation supports ZeroMQ as the transport mechanism, with a two-phase messaging protocol that separates metadata from bulk data transfers.

## Core Concepts

### Two-Phase Messaging Protocol

Lightbeam uses a two-phase approach to message transmission:

1. **Metadata Phase**: Sends message metadata including bulk descriptors
2. **Bulk Data Phase**: Transfers the actual data payloads

This separation allows receivers to:
- Inspect message metadata before allocating buffers
- Allocate appropriately sized buffers based on incoming data sizes
- Handle multiple data chunks efficiently

### Transport Types

Currently supported transport:

```cpp
#include <hermes_shm/lightbeam/zmq_transport.h>

namespace hshm::lbm {
    enum class Transport {
        kZeroMq      // ZeroMQ messaging
    };
}
```

## Data Structures

### hshm::lbm::Bulk

Describes a memory region for data transfer:

```cpp
namespace hshm::lbm {
// Bulk flags
#define BULK_EXPOSE  // Bulk is exposed (metadata only, no data transfer)
#define BULK_XFER   // Bulk is marked for data transmission

struct Bulk {
    hipc::FullPtr<char> data;       // Pointer to data (supports shared memory)
    size_t size;                    // Size of data in bytes
    hshm::bitfield32_t flags;       // BULK_EXPOSE or BULK_XFER
    void* desc = nullptr;           // RDMA memory registration descriptor
    void* mr = nullptr;             // Memory region handle (for future RDMA support)
};
}
```

**Key Features:**
- Uses `hipc::FullPtr` for shared memory compatibility
- Can be created from raw pointers, `hipc::Pointer`, or `hipc::FullPtr`
- Flags control bulk behavior:
  - **BULK_EXPOSE**: Bulk metadata is sent but no data is transferred (useful for shared memory)
  - **BULK_XFER**: Bulk marked for data transmission (data is transferred over network)
  - Sender's `send` vector can contain bulks with either flag
  - Only BULK_XFER bulks are actually transmitted via Send() and received via RecvBulks()
- Prepared for future RDMA transport extensions

### hshm::lbm::LbmMeta

Base class for message metadata:

```cpp
namespace hshm::lbm {
class LbmMeta {
 public:
    std::vector<Bulk> send;  // Bulks marked BULK_XFER (sender side)
    std::vector<Bulk> recv;  // Bulks marked BULK_EXPOSE (receiver side)
};
}
```

**Usage:**
- Extend `LbmMeta` to include custom metadata fields
- Must implement cereal serialization for custom fields
- **send vector**: Contains sender's bulk descriptors (can have BULK_EXPOSE or BULK_XFER flags)
  - Only bulks marked BULK_XFER will be transmitted over the network
  - Sender populates this vector with bulks to send
- **recv vector**: Receiver's copy of send with local data pointers
  - Server receives metadata, inspects all bulks in `send` (regardless of flag) to see data sizes
  - Server allocates local buffers and creates `recv` bulks copying flags from `send`
  - Only bulks marked BULK_XFER will receive data via `RecvBulks()`
  - `recv` should mirror `send` structure but with receiver's local pointers

## API Reference

### hshm::lbm::Client Interface

The client initiates data transfers:

```cpp
namespace hshm::lbm {
class Client {
 public:
    // Expose memory for transfer (creates Bulk descriptor)
    virtual Bulk Expose(const char* data, size_t data_size, u32 flags) = 0;
    virtual Bulk Expose(const hipc::Pointer& ptr, size_t data_size, u32 flags) = 0;
    virtual Bulk Expose(const hipc::FullPtr<char>& ptr, size_t data_size, u32 flags) = 0;

    // Send metadata and bulk data
    template<typename MetaT>
    int Send(MetaT &meta);
};
}
```

**Methods:**
- `Expose()`: Registers memory for transfer, returns `Bulk` descriptor
  - Accepts raw pointers, `hipc::Pointer`, or `hipc::FullPtr`
  - **flags**: Use `BULK_XFER` to mark bulk for transmission
  - Returns immediately (no actual data transfer)
- `Send()`: Transmits metadata and bulks in the send vector
  - Template method accepting any `LbmMeta`-derived type
  - Serializes metadata using cereal (includes both send and recv vectors)
  - **Only transmits bulks in `meta.send` vector**
  - Validates all send bulks have `BULK_XFER` flag
  - **Synchronous**: Blocks until send completes
  - **Returns**: `0` on success, `-1` if send bulk missing BULK_XFER, other error codes on failure

### hshm::lbm::Server Interface

The server receives data transfers:

```cpp
namespace hshm::lbm {
class Server {
 public:
    // Expose memory for receiving data
    virtual Bulk Expose(char* data, size_t data_size, u32 flags) = 0;
    virtual Bulk Expose(const hipc::Pointer& ptr, size_t data_size, u32 flags) = 0;
    virtual Bulk Expose(const hipc::FullPtr<char>& ptr, size_t data_size, u32 flags) = 0;

    // Two-phase receive
    template<typename MetaT>
    int RecvMetadata(MetaT &meta);

    template<typename MetaT>
    int RecvBulks(MetaT &meta);

    // Get server address
    virtual std::string GetAddress() const = 0;
};
}
```

**Methods:**
- `Expose()`: Registers receive buffers, returns `Bulk` descriptor
  - **flags**: Copy flags from corresponding `send` bulk to maintain consistency
  - Must be called after `RecvMetadata()` to populate `meta.recv` with local buffers
- `RecvMetadata()`: Receives and deserializes message metadata
  - **Non-blocking**: Returns immediately if no message available
  - Populates `meta.send` with sender's bulk descriptors (size and flags)
  - Server can inspect all bulks in `meta.send` (regardless of flag) to determine buffer sizes
  - **Returns**: `0` on success, `EAGAIN` if no message, other error codes on failure
  - Typically used in polling loop until message arrives
- `RecvBulks()`: Receives actual data into exposed buffers
  - Must be called after `RecvMetadata()` succeeds and `meta.recv` is populated
  - **Only receives data into bulks marked BULK_XFER in `meta.recv` vector**
  - Iterates over `meta.recv` and receives only into bulks with BULK_XFER flag
  - Bulks marked BULK_EXPOSE in recv will be skipped (no data transfer)
  - **Synchronous**: Blocks until all WRITE bulks received
  - **Returns**: `0` on success, error codes on failure
- `GetAddress()`: Returns the server's bind address

### hshm::lbm::TransportFactory

Factory for creating client and server instances:

```cpp
namespace hshm::lbm {
class TransportFactory {
 public:
    static std::unique_ptr<Client> GetClient(
        const std::string& addr, Transport t,
        const std::string& protocol = "", int port = 0);

    static std::unique_ptr<Server> GetServer(
        const std::string& addr, Transport t,
        const std::string& protocol = "", int port = 0);
};
}
```

## Examples

### Basic Client-Server Communication

```cpp
#include <hermes_shm/lightbeam/zmq_transport.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <errno.h>

using namespace hshm::lbm;

void basic_example() {
    // Server setup
    std::string addr = "127.0.0.1";
    std::string protocol = "tcp";
    int port = 8888;

    auto server = hshm::lbm::TransportFactory::GetServer(addr, hshm::lbm::Transport::kZeroMq,
                                             protocol, port);
    auto client = hshm::lbm::TransportFactory::GetClient(addr, hshm::lbm::Transport::kZeroMq,
                                             protocol, port);

    // Give ZMQ time to establish connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // CLIENT: Prepare and send data
    const char* message = "Hello, Lightbeam!";
    size_t message_size = strlen(message);

    LbmMeta send_meta;
    Bulk bulk = client->Expose(message, message_size, BULK_XFER);
    send_meta.send.push_back(bulk);

    int rc = client->Send(send_meta);
    if (rc != 0) {
        std::cerr << "Send failed with error: " << rc << "\n";
        return;
    }
    std::cout << "Client sent data successfully\n";

    // SERVER: Receive metadata (poll until available)
    LbmMeta recv_meta;
    while (true) {
        rc = server->RecvMetadata(recv_meta);
        if (rc == 0) break;
        if (rc != EAGAIN) {
            std::cerr << "RecvMetadata failed with error: " << rc << "\n";
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "Server received metadata with "
              << recv_meta.send.size() << " bulks\n";

    // SERVER: Allocate buffer based on sender's bulk size and copy flags from send
    std::vector<char> buffer(recv_meta.send[0].size);
    recv_meta.recv.push_back(server->Expose(buffer.data(), buffer.size(),
                                            recv_meta.send[0].flags.bits_));

    rc = server->RecvBulks(recv_meta);
    if (rc != 0) {
        std::cerr << "RecvBulks failed with error: " << rc << "\n";
        return;
    }
    std::cout << "Server received: "
              << std::string(buffer.data(), buffer.size()) << "\n";
}
```

### Custom Metadata with Multiple Bulks

```cpp
#include <hermes_shm/lightbeam/zmq_transport.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

using namespace hshm::lbm;

// Custom metadata class
class RequestMeta : public LbmMeta {
 public:
    int request_id;
    std::string operation;
    std::string client_name;
};

// Cereal serialization
namespace cereal {
    template<class Archive>
    void serialize(Archive& ar, RequestMeta& meta) {
        ar(meta.send, meta.recv, meta.request_id, meta.operation, meta.client_name);
    }
}

void custom_metadata_example() {
    auto server = std::make_unique<hshm::lbm::ZeroMqServer>("127.0.0.1", "tcp", 8889);
    auto client = std::make_unique<hshm::lbm::ZeroMqClient>("127.0.0.1", "tcp", 8889);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // CLIENT: Send multiple data chunks with metadata
    const char* data1 = "First chunk";
    const char* data2 = "Second chunk";

    RequestMeta send_meta;
    send_meta.request_id = 42;
    send_meta.operation = "write";
    send_meta.client_name = "client_01";

    send_meta.send.push_back(client->Expose(data1, strlen(data1), BULK_XFER));
    send_meta.send.push_back(client->Expose(data2, strlen(data2), BULK_XFER));

    int rc = client->Send(send_meta);
    if (rc != 0) {
        std::cerr << "Send failed\n";
        return;
    }

    // SERVER: Receive metadata (poll until available)
    RequestMeta recv_meta;
    while (true) {
        rc = server->RecvMetadata(recv_meta);
        if (rc == 0) break;
        if (rc != EAGAIN) {
            std::cerr << "RecvMetadata failed: " << rc << "\n";
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Request ID: " << recv_meta.request_id << "\n";
    std::cout << "Operation: " << recv_meta.operation << "\n";
    std::cout << "Client: " << recv_meta.client_name << "\n";
    std::cout << "Number of bulks: " << recv_meta.send.size() << "\n";

    // SERVER: Allocate buffers based on sender's bulk sizes and copy flags from send
    std::vector<std::vector<char>> buffers;
    for (size_t i = 0; i < recv_meta.send.size(); ++i) {
        buffers.emplace_back(recv_meta.send[i].size);
        recv_meta.recv.push_back(server->Expose(buffers[i].data(),
                                                 buffers[i].size(),
                                                 recv_meta.send[i].flags.bits_));
    }

    rc = server->RecvBulks(recv_meta);
    if (rc != 0) {
        std::cerr << "RecvBulks failed\n";
        return;
    }

    for (size_t i = 0; i < buffers.size(); ++i) {
        std::cout << "Chunk " << i << ": "
                  << std::string(buffers[i].begin(), buffers[i].end()) << "\n";
    }
}
```

### Working with Shared Memory Pointers

```cpp
#include <hermes_shm/lightbeam/zmq_transport.h>
#include <hermes_shm/memory/memory_manager.h>

using namespace hshm::lbm;

void shared_memory_example() {
    // Assume memory manager is initialized
    hipc::Allocator* alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator();

    // Allocate shared memory
    size_t data_size = 1024;
    hipc::Pointer shm_ptr = alloc->Allocate(data_size);
    hipc::FullPtr<char> full_ptr(shm_ptr);

    // Write data to shared memory
    memcpy(full_ptr.ptr_, "Shared memory data", 18);

    // Create client and expose shared memory
    auto client = std::make_unique<hshm::lbm::ZeroMqClient>("127.0.0.1", "tcp", 8890);

    LbmMeta meta;
    // Can use either hipc::Pointer or hipc::FullPtr directly
    meta.send.push_back(client->Expose(full_ptr, data_size, BULK_XFER));

    int rc = client->Send(meta);
    if (rc != 0) {
        std::cerr << "Send failed\n";
    }

    // Free shared memory
    alloc->Free(shm_ptr);
}
```

### Distributed MPI Communication

```cpp
#include <hermes_shm/lightbeam/zmq_transport.h>
#include <mpi.h>

using namespace hshm::lbm;

void distributed_example() {
    MPI_Init(nullptr, nullptr);

    int my_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::string addr = "127.0.0.1";
    int base_port = 9000;

    // Each rank creates a server on a unique port
    auto server = hshm::lbm::TransportFactory::GetServer(
        addr, hshm::lbm::Transport::kZeroMq, "tcp", base_port + my_rank);

    // Rank 0 sends to all other ranks
    if (my_rank == 0) {
        std::vector<std::unique_ptr<hshm::lbm::Client>> clients;
        for (int i = 1; i < world_size; ++i) {
            clients.push_back(hshm::lbm::TransportFactory::GetClient(
                addr, hshm::lbm::Transport::kZeroMq, "tcp", base_port + i));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        for (size_t i = 0; i < clients.size(); ++i) {
            std::string msg = "Message to rank " + std::to_string(i + 1);

            LbmMeta meta;
            meta.send.push_back(clients[i]->Expose(msg.data(), msg.size(), BULK_XFER));

            int rc = clients[i]->Send(meta);
            if (rc != 0) {
                std::cerr << "Send failed to rank " << (i + 1) << "\n";
            }
        }
    } else {
        // Other ranks receive from rank 0
        LbmMeta meta;
        int rc = server->RecvMetadata(meta);
        while (rc == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            rc = server->RecvMetadata(meta);
        }
        if (rc != 0) {
            std::cerr << "RecvMetadata failed\n";
            MPI_Finalize();
            return;
        }

        std::vector<char> buffer(meta.send[0].size);
        meta.recv.push_back(server->Expose(buffer.data(), buffer.size(), meta.send[0].flags.bits_));

        rc = server->RecvBulks(meta);
        if (rc != 0) {
            std::cerr << "RecvBulks failed\n";
            MPI_Finalize();
            return;
        }

        std::cout << "Rank " << my_rank << " received: "
                  << std::string(buffer.begin(), buffer.end()) << "\n";
    }

    MPI_Finalize();
}
```

## Best Practices

### 1. Connection Management

```cpp
// Give ZMQ time to establish connections
std::this_thread::sleep_for(std::chrono::milliseconds(100));

// Store clients/servers in containers for reuse
std::vector<std::unique_ptr<Client>> client_pool;
```

### 2. Error Handling

```cpp
int rc = client->Send(meta);
if (rc != 0) {
    std::cerr << "Send failed with error code: " << rc << "\n";
    // Implement retry logic
}
```

### 3. Polling for Receive

```cpp
// Poll for metadata until available
int rc = server->RecvMetadata(meta);
while (rc == EAGAIN) {
    // Do other work or sleep briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    rc = server->RecvMetadata(meta);
}
if (rc != 0) {
    std::cerr << "Error: " << rc << "\n";
}
```

### 4. Memory Management

```cpp
// Ensure data lifetime during transfer
{
    std::vector<char> data(1024);
    Bulk bulk = client->Expose(data.data(), data.size(), BULK_XFER);
    LbmMeta meta;
    meta.send.push_back(bulk);
    // data must remain valid until Send() completes
    int rc = client->Send(meta);
} // data destroyed after Send completes
```

### 5. Send and Recv Vector Usage

```cpp
// CLIENT: Populate send vector with BULK_XFER bulks
LbmMeta send_meta;
send_meta.send.push_back(client->Expose(data1, size1, BULK_XFER));
send_meta.send.push_back(client->Expose(data2, size2, BULK_XFER));

// Send transmits only bulks in send vector
int rc = client->Send(send_meta);

// SERVER: Receive metadata and inspect send vector for sizes
LbmMeta recv_meta;
while ((rc = server->RecvMetadata(recv_meta)) == EAGAIN) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

// Allocate buffers based on sender's bulk sizes and copy flags from send
for (size_t i = 0; i < recv_meta.send.size(); ++i) {
    std::vector<char> buffer(recv_meta.send[i].size);
    recv_meta.recv.push_back(server->Expose(buffer.data(), buffer.size(),
                                            recv_meta.send[i].flags.bits_));
}

// RecvBulks receives into recv vector only
server->RecvBulks(recv_meta);
```

### 6. Custom Metadata Serialization

```cpp
// Always serialize send and recv vectors first in custom metadata
namespace cereal {
    template<class Archive>
    void serialize(Archive& ar, CustomMeta& meta) {
        ar(meta.send, meta.recv);  // Serialize base class vectors first
        ar(meta.custom_field1, meta.custom_field2);  // Then custom fields
    }
}
```

### 7. Buffer Allocation Strategy

```cpp
// Receive metadata and inspect send vector for sizes
LbmMeta meta;
int rc = server->RecvMetadata(meta);
while (rc == EAGAIN) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    rc = server->RecvMetadata(meta);
}
if (rc != 0) {
    return;
}

// Allocate buffers based on sender's bulk sizes in send vector
std::vector<std::vector<char>> buffers;
for (const auto& bulk : meta.send) {
    buffers.emplace_back(bulk.size);  // Allocate exact size from sender
}

// Populate recv vector with exposed buffers, copying flags from send
for (size_t i = 0; i < buffers.size(); ++i) {
    meta.recv.push_back(server->Expose(buffers[i].data(), buffers[i].size(),
                                       meta.send[i].flags.bits_));
}
```

### 8. Multi-Threading

```cpp
// Use separate server thread for receiving
std::atomic<bool> running{true};
std::thread server_thread([&server, &running]() {
    while (running) {
        LbmMeta meta;
        int rc = server->RecvMetadata(meta);
        if (rc == 0) {
            // Process message
        } else if (rc != EAGAIN) {
            std::cerr << "Error: " << rc << "\n";
            break;
        }
    }
});
```

## Error Codes

### Return Values

All operations return an integer error code:

- **0**: Success
- **EAGAIN**: No data available (RecvMetadata only)
- **Positive values**: System error codes (from `errno.h` or `zmq_errno()`)
- **-1**: Generic error (e.g., deserialization failure, message part mismatch)

### Common ZMQ Error Codes

- **EAGAIN (11)**: Resource temporarily unavailable (non-blocking operation would block)
- **EINTR (4)**: Interrupted system call
- **ETERM (156384763)**: Context was terminated
- **ENOTSOCK (88)**: Invalid socket
- **EMSGSIZE (90)**: Message too large

### Checking Errors

```cpp
int rc = server->RecvMetadata(meta);
if (rc == EAGAIN) {
    // No data available, try again later
} else if (rc != 0) {
    // Error occurred
    std::cerr << "Error " << rc << ": " << strerror(rc) << "\n";
}
```

## Performance Considerations

1. **Metadata Overhead**: Keep custom metadata small - it's serialized/deserialized on every message

2. **Bulk Count**: Minimize the number of bulks per message when possible

3. **Buffer Reuse**: Reuse allocated buffers across multiple receives

4. **Connection Pooling**: Create clients once and reuse them

5. **Serialization Cost**: Use efficient serialization for custom metadata

6. **Polling Interval**: Balance between responsiveness and CPU usage when polling
   - Too frequent: Wastes CPU cycles
   - Too infrequent: Adds latency

7. **Blocking vs Polling**:
   - `Send()` and `RecvBulks()` are synchronous/blocking
   - `RecvMetadata()` can be polled with EAGAIN handling

## Limitations and Future Work

**Current Limitations:**
- Only ZeroMQ transport is implemented
- RecvMetadata polling required (returns EAGAIN)
- No built-in timeout mechanism
- Limited to TCP protocol

**Future Enhancements:**
- Thallium/Mercury transport for RPC-style communication
- Libfabric transport for RDMA operations
- Timeout support for operations
- Built-in retry mechanisms
- Protocol negotiation and versioning
- Connection multiplexing
- Async/await style API with callbacks
