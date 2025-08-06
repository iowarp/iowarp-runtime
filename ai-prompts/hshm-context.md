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

## CMake
```
find_package(HermesShm CONFIG REQUIRED)
message(STATUS "found hermes_shm at ${HermesShm_PREFIX}")
target_link_libraries(hshm::cxx)
```

## Timer
```cpp
TEST_CASE("TestTimer") {
  hshm::Timer timer;
  timer.Resume();
  sleep(3);
  timer.Pause();
  HILOG(kInfo, "Print timer: {}", timer.GetSec());
}
```


## Singleton
The header file:
```cpp
#define HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(T, NAME) extern __TU(T) * NAME;
#define HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(T, NAME) __TU(T) *NAME = nullptr;
#define HSHM_GET_GLOBAL_CROSS_PTR_VAR(T, NAME) \
  hshm::GetGlobalCrossPtrVar<__TU(T)>(NAME)
template <typename T>
HSHM_CROSS_FUN static inline T *GetGlobalCrossPtrVar(T *&instance) {
  if (instance == nullptr) {
    instance = new T();
  }
  return instance;
}

/** Singleton declaration */
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(hshm::ipc::MemoryManager, hshmMemoryManager);
#define HSHM_MEMORY_MANAGER                               \
  HSHM_GET_GLOBAL_CROSS_PTR_VAR(hshm::ipc::MemoryManager, \
                                hshm::ipc::hshmMemoryManager)
#define HSHM_MEMORY_MANAGER_T hshm::ipc::MemoryManager *
```

The source file:
```cpp
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(hshm::ipc::MemoryManager,
                                    hshmMemoryManager);
```

This will create a global variable named 

## Thread Model
HSHM_THREAD_MODEL points to a specific threading library, such as pthreads. Below is an example
```cpp
TEST_CASE("ThreadModel") {
  auto *thread_model = HSHM_THREAD_MODEL;
  hshm::thread::ThreadGroup group = thread_model->CreateThreadGroup({});
  hshm::thread::Thread thread = thread_model->Spawn(
      group,
      [](int tid) {
        std::cout << "Hello, world! (pthread) " << tid << std::endl;
      },
      1);
  thread_model->Join(thread);
}
```

## Dynamic Library Loading
```cpp
/** Dynamically load shared libraries */
struct SharedLibrary {
  void *handle_;

  SharedLibrary() = default;
  HSHM_DLL SharedLibrary(const std::string &name);
  HSHM_DLL ~SharedLibrary();

  // Delete copy operations
  SharedLibrary(const SharedLibrary &) = delete;
  SharedLibrary &operator=(const SharedLibrary &) = delete;

  // Move operations
  HSHM_DLL SharedLibrary(SharedLibrary &&other) noexcept;
  HSHM_DLL SharedLibrary &operator=(SharedLibrary &&other) noexcept;

  HSHM_DLL void Load(const std::string &name);
  HSHM_DLL void *GetSymbol(const std::string &name);
  HSHM_DLL std::string GetError() const;

  bool IsNull() { return handle_ == nullptr; }
};
```

## HSHM Data Structure Template
This will create various data structures using the chi:: namespace prefix instead of hipc:: and hshm::. The reason is to ensure that we don't need to keep specifying the allocator type in the data structures.
```cpp
HSHM_DATA_STRUCTURES_TEMPLATE(chi, CHI_MAIN_ALLOC_T);
```

For example, originally I had to do:
```cpp
hipc::vector<int, CHI_MAIN_ALLOC_T>
```

Now I can do below, which is a little cleaner: 
```cpp
chi::ipc::vector<int>
```

## Memory Allocation and Backends
```cpp
template <typename AllocT>
void PretestRank0() {
  std::string shm_url = "test_allocators2";
  AllocatorId alloc_id(1, 0);
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  mem_mngr->UnregisterAllocator(alloc_id);
  mem_mngr->DestroyBackend(hipc::MemoryBackendId::GetRoot());
  mem_mngr->CreateBackend<PosixShmMmap>(hipc::MemoryBackendId::Get(0),
                                        hshm::Unit<size_t>::Megabytes(100),
                                        shm_url);
  mem_mngr->CreateAllocator<AllocT>(hipc::MemoryBackendId::Get(0), alloc_id,
                                    sizeof(sub::ipc::mpsc_queue<int>));
}

void PretestRankN() {
  std::string shm_url = "test_allocators2";
  AllocatorId alloc_id(1, 0);
  auto mem_mngr = HSHM_MEMORY_MANAGER;
  mem_mngr->AttachBackend(MemoryBackendType::kPosixShmMmap, shm_url);
}

void MainPretest() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  HILOG(kInfo, "PRETEST RANK 0 beginning {}", rank);
  if (rank == RANK0) {
    PretestRank0<HSHM_DEFAULT_ALLOC_T>();
  }
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank != RANK0) {
    PretestRankN();
  }
  HILOG(kInfo, "PRETEST RANK 0 done {}", rank);
  MPI_Barrier(MPI_COMM_WORLD);
}

void MainPosttest() {}
```

## Base Configuration
```cpp 
namespace hshm {

class ConfigParse {
 public: 
  /**
   * parse a hostfile string
   * [] represents a range to generate
   * ; represents a new host name completely
   *
   * Example: hello[00-09,10]-40g;hello2[11-13]-40g
   * */
  static void ParseHostNameString(std::string hostname_set_str,
                                  std::vector<std::string> &list);

  /** parse the suffix of \a num_text NUMBER text */
  static std::string ParseNumberSuffix(const std::string &num_text);

  /** parse the number of \a num_text NUMBER text */
  template <typename T>
  static T ParseNumber(const std::string &num_text);

  /** 
  Converts \a size_text SIZE text into a size_t. E.g., 10g -> integer.
   */
  static hshm::u64 ParseSize(const std::string &size_text);

  /** Returns bandwidth (bytes / second). E.g., 10MBps -> integer */
  static hshm::u64 ParseBandwidth(const std::string &size_text);

  /** Returns latency (nanoseconds) */
  static hshm::u64 ParseLatency(const std::string &latency_text);

  /** Expands all environment variables in a path string */
  static std::string ExpandPath(std::string path);

  /** Parse hostfile */
  static std::vector<std::string> ParseHostfile(const std::string &path);
};

/**
 * Base class for configuration files
 * */
class BaseConfig {
 public:
  /** load configuration from a string */
  void LoadText(const std::string &config_string, bool with_default = true);

  /** load configuration from file */
  void LoadFromFile(const std::string &path, bool with_default = true);

  /** load the default configuration */
  virtual void LoadDefault() = 0;

 public:
  /** parse \a list_node vector from configuration file in YAML */
  template <typename T, typename VEC_TYPE = std::vector<T>>
  static void ParseVector(YAML::Node list_node, VEC_TYPE &list) {
    for (auto val_node : list_node) {
      list.emplace_back(val_node.as<T>());
    }
  }

  /** clear + parse \a list_node vector from configuration file in YAML */
  template <typename T, typename VEC_TYPE = std::vector<T>>
  static void ClearParseVector(YAML::Node list_node, VEC_TYPE &list) {
    list.clear();
    for (auto val_node : list_node) {
      list.emplace_back(val_node.as<T>());
    }
  }

 private:
  virtual void ParseYAML(YAML::Node &yaml_conf) = 0;
};

}  // namespace hshm
```

## MPSC Queue (multiple producer, single consumer lockfree queue)
An example of the MSPC queue.
```cpp
TEST_CASE("TestMpscQueueMpi") {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // The allocator was initialized in test_init.c
  // we are getting the "header" of the allocator
  auto *alloc = HSHM_MEMORY_MANAGER->GetAllocator<HSHM_DEFAULT_ALLOC_T>(
      AllocatorId(1, 0));
  auto *queue_ =
      alloc->GetCustomHeader<hipc::delay_ar<sub::ipc::mpsc_queue<int>>>();

  // Make the queue uptr
  if (rank == RANK0) {
    // Rank 0 create the pointer queue
    queue_->shm_init(alloc, 256);
    // Affine to CPU 0
    hshm::ProcessAffiner::SetCpuAffinity(HSHM_SYSTEM_INFO->pid_, 0);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank != RANK0) {
    // Affine to CPU 1
    hshm::ProcessAffiner::SetCpuAffinity(HSHM_SYSTEM_INFO->pid_, 1);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  sub::ipc::mpsc_queue<int> *queue = queue_->get();
  if (rank == RANK0) {
    // Emplace values into the queue
    for (int i = 0; i < 256; ++i) {
      queue->emplace(i);
    }
  } else {
    // Pop entries from the queue
    int x, count = 0;
    while (!queue->pop(x).IsNull() && count < 256) {
      REQUIRE(x == count);
      ++count;
    }
  }

  // The barrier is necessary so that
  // Rank 0 doesn't exit before Rank 1
  // The uptr frees data when rank 0 exits.
  MPI_Barrier(MPI_COMM_WORLD);
}
```

## Multi Queue
Creates a set of MPSC queues of size NUM_PRIORITIES * NUM_LANES.
```cpp
TEST_CASE("TestMultiRingBufferBasic") {
  auto *alloc = HSHM_DEFAULT_ALLOC;
  REQUIRE(alloc->GetCurrentlyAllocatedSize() == 0);

  PAGE_DIVIDE("TEST") {
    // Create a multi-ring buffer with 2 lanes, 3 priorities, depth 8
    hshm::multi_mpsc_queue<int> buffer(alloc, 2, 3, 8);

    // Test basic properties
    REQUIRE(buffer.GetNumLanes() == 2);
    REQUIRE(buffer.GetNumPriorities() == 3);

    // Push items to different lanes and priorities using GetLane
    auto &lane_0_pri_0 = buffer.GetLane(0, 0);  // Lane 0, Priority 0 (highest)
    auto &lane_0_pri_1 = buffer.GetLane(0, 1);  // Lane 0, Priority 1
    auto &lane_1_pri_0 = buffer.GetLane(1, 0);  // Lane 1, Priority 0 (highest)
    auto &lane_1_pri_2 = buffer.GetLane(1, 2);  // Lane 1, Priority 2 (lowest)

    auto tok1 = lane_0_pri_0.emplace(100);
    auto tok2 = lane_0_pri_1.emplace(200);
    auto tok3 = lane_1_pri_0.emplace(300);
    auto tok4 = lane_1_pri_2.emplace(400);

    REQUIRE(!tok1.IsNull());
    REQUIRE(!tok2.IsNull());
    REQUIRE(!tok3.IsNull());
    REQUIRE(!tok4.IsNull());

    // Check sizes
    REQUIRE(lane_0_pri_0.GetSize() == 1);
    REQUIRE(lane_0_pri_1.GetSize() == 1);
    REQUIRE(lane_1_pri_0.GetSize() == 1);
    REQUIRE(lane_1_pri_2.GetSize() == 1);

    // Pop from individual lanes
    int val;
    auto pop_tok = lane_0_pri_0.pop(val);
    REQUIRE(!pop_tok.IsNull());
    REQUIRE(val == 100);

    pop_tok = lane_0_pri_1.pop(val);
    REQUIRE(!pop_tok.IsNull());
    REQUIRE(val == 200);

    pop_tok = lane_1_pri_0.pop(val);
    REQUIRE(!pop_tok.IsNull());
    REQUIRE(val == 300);

    pop_tok = lane_1_pri_2.pop(val);
    REQUIRE(!pop_tok.IsNull());
    REQUIRE(val == 400);

    // All items popped

    // Try to pop from empty lanes
    pop_tok = lane_0_pri_0.pop(val);
    REQUIRE(pop_tok.IsNull());
  }

  REQUIRE(alloc->GetCurrentlyAllocatedSize() == 0);
}
```

## Lightbeam 
Below is an example of using lightbeam for start and ZMQ server and client.
```cpp 
using namespace hshm::lbm;

class LightbeamTransportTest {
 public:
  LightbeamTransportTest(Transport transport, const std::string& addr,
                         const std::string& protocol, int port)
      : transport_(transport),
        addr_(addr),
        protocol_(protocol),
        port_(port) {}

  void Run() {
    std::cout << "\n==== Testing backend: " << BackendName() << " ====\n";
    auto server_ptr =
        TransportFactory::GetServer(addr_, transport_, protocol_, port_);
    std::string server_addr = server_ptr->GetAddress();
    std::unique_ptr<Client> client_ptr;
    if (transport_ == Transport::kLibfabric) {
      client_ptr = TransportFactory::GetClient(server_addr, transport_,
                                               protocol_, port_);
    } else {
      client_ptr = TransportFactory::GetClient(server_addr, transport_,
                                               protocol_, port_);
    }

    const std::string magic = "unit_test_magic";
    // Client exposes and sends data
    Bulk send_bulk = client_ptr->Expose(magic.data(), magic.size(), 0);
    Event* send_event = client_ptr->Send(send_bulk);
    while (!send_event->is_done) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(send_event->error_code == 0);
    delete send_event;

    // Server exposes buffer and receives data
    std::vector<char> recv_buf(magic.size());
    Bulk recv_bulk = server_ptr->Expose(recv_buf.data(), recv_buf.size(), 0);
    Event* recv_event = nullptr;
    while (!recv_event || !recv_event->is_done) {
      if (recv_event) delete recv_event;
      recv_event = server_ptr->Recv(recv_bulk);
      if (!recv_event->is_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    assert(recv_event->error_code == 0);
    std::string received(recv_bulk.data, recv_bulk.size);
    std::cout << "Received: " << received << std::endl;
    assert(received == magic);
    delete recv_event;
    std::cout << "[" << BackendName() << "] Test passed!\n";
  }

 private:
  std::string BackendName() const {
    switch (transport_) {
      case Transport::kZeroMq:
        return "ZeroMQ";
      case Transport::kThallium:
        return "Thallium";
      case Transport::kLibfabric:
        return "Libfabric";
      default:
        return "Unknown";
    }
  }
  Transport transport_;
  std::string addr_;
  std::string protocol_;
  int port_;
};

int main() {
  // Test ZeroMQ
  std::string zmq_addr = "127.0.0.1";
  std::string zmq_protocol = "tcp";
  int zmq_port = 8192;
  LightbeamTransportTest test(Transport::kZeroMq, zmq_addr, zmq_protocol,
                            zmq_port);
  test.Run();
  std::cout << "All transport tests passed!" << std::endl;
  return 0;
} 
```

## Bitfields

```cpp
namespace hshm {

#define BIT_OPT(T, n) (((T)1) << n)
#define ALL_BITS(T) (~((T)0))

/**
 * A generic bitfield template
 * */
template <typename T = u32, bool ATOMIC = false>
struct bitfield {
  hipc::opt_atomic<T, ATOMIC> bits_;

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN bitfield() : bits_(0) {}

  /** Emplace constructor */
  HSHM_INLINE_CROSS_FUN explicit bitfield(T mask) : bits_(mask) {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN bitfield(const bitfield &other) : bits_(other.bits_) {}

  /** Copy assignment */
  HSHM_INLINE_CROSS_FUN bitfield &operator=(const bitfield &other) {
    bits_ = other.bits_;
    return *this;
  }

  /** Move constructor */
  HSHM_INLINE_CROSS_FUN bitfield(bitfield &&other) noexcept
      : bits_(other.bits_) {}

  /** Move assignment */
  HSHM_INLINE_CROSS_FUN bitfield &operator=(bitfield &&other) noexcept {
    bits_ = other.bits_;
    return *this;
  }

  /** Copy from any bitfield */
  template <bool ATOMIC2>
  HSHM_INLINE_CROSS_FUN bitfield(const bitfield<T, ATOMIC2> &other)
      : bits_(other.bits_) {}

  /** Copy assignment from any bitfield */
  template <bool ATOMIC2>
  HSHM_INLINE_CROSS_FUN bitfield &operator=(const bitfield<T, ATOMIC2> &other) {
    bits_ = other.bits_;
    return *this;
  }

  /** Set bits using mask */
  HSHM_INLINE_CROSS_FUN void SetBits(T mask) { bits_ |= mask; }

  /** Unset bits in mask */
  HSHM_INLINE_CROSS_FUN void UnsetBits(T mask) { bits_ &= ~mask; }

  /** Check if any bits are set */
  HSHM_INLINE_CROSS_FUN T Any(T mask) const { return (bits_ & mask).load(); }

  /** Check if all bits are set */
  HSHM_INLINE_CROSS_FUN T All(T mask) const { return Any(mask) == mask; }

  /** Copy bits from another bitfield */
  HSHM_INLINE_CROSS_FUN void CopyBits(bitfield field, T mask) {
    bits_ &= (field.bits_ & mask);
  }

  /** Clear all bits */
  HSHM_INLINE_CROSS_FUN void Clear() { bits_ = 0; }

  /** Make a mask */
  HSHM_INLINE_CROSS_FUN static T MakeMask(int start, int length) {
    return ((((T)1) << length) - 1) << start;
  }

  /** Serialization */
  template <typename Ar>
  void serialize(Ar &ar) {
    ar & bits_;
  }
};
typedef bitfield<u8> bitfield8_t;
typedef bitfield<u16> bitfield16_t;
typedef bitfield<u32> bitfield32_t;
typedef bitfield<u64> bitfield64_t;
typedef bitfield<int> ibitfield;
}
```

For example:
```cpp
#define TASK_PERIODIC BIT_OPT(int, 0)
#define TASK_FIRE_AND_FORGET BIT_OPT(int, 1)
```