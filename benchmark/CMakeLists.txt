# Benchmark directory

# BDev I/O throughput benchmark
add_executable(task_throughput_benchmark
  task_throughput_benchmark.cc
)

target_link_libraries(task_throughput_benchmark
  cxx                             # Core Chimaera runtime
  chimaera::bdev_client           # BDev client for I/O operations
  chimaera::MOD_NAME_client       # MOD_NAME client for latency testing
  chimaera::admin_client          # Admin client for container creation
  hshm::cxx                       # HermesShm shared memory framework
)

# Install benchmark executable
install(TARGETS task_throughput_benchmark
  DESTINATION bin
)
