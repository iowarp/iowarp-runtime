# IOWarp Distributed Unit Test

This directory contains the distributed unit test infrastructure for the IOWarp runtime, focusing on testing the bdev module with different PoolQuery strategies across multiple nodes.

## Overview

The distributed test verifies proper operation of distributed storage operations using three PoolQuery strategies:

1. **DirectHash**: Uses loop iterator as hash for deterministic node selection
2. **Range**: Uses range query to distribute operations across specified node range
3. **Broadcast**: Uses broadcast query to send operations to all nodes

## Directory Structure

```
test/distributed/
├── README.md                    # This file
├── docker-compose.yml           # Docker cluster definition (4 nodes)
├── generate_hostfile.sh         # Auto-generates hostfile for cluster
├── hostfile                     # Generated hostfile with node IPs
└── run_distributed_test.sh      # Main test execution script
```

## Quick Start

### Docker Mode (Recommended)

Run all tests in Docker cluster:

```bash
cd test/distributed
./run_distributed_test.sh all
```

Run specific test case:

```bash
./run_distributed_test.sh -t direct run
./run_distributed_test.sh -t range run
./run_distributed_test.sh -t broadcast run
```

### Local Mode

For local development testing:

```bash
./run_distributed_test.sh -m local all
```

## Docker Cluster

The `docker-compose.yml` defines a 4-node cluster:

- **Node 1** (172.25.0.10): Build and test execution node
- **Node 2** (172.25.0.11): Worker node
- **Node 3** (172.25.0.12): Worker node
- **Node 4** (172.25.0.13): Worker node

### Cluster Configuration

Each node has:
- **Memory**: 16GB RAM + 16GB shared memory
- **Image**: `iowawrp/iowarp-deps-spack:ai`
- **Network**: Bridge network (172.25.0.0/16)
- **Volumes**:
  - `~/.ppi-jarvis`: Jarvis configuration
  - Repository root: Mounted at `/iowarp-runtime`
  - Hostfile: Mounted at `/etc/iowarp/hostfile`

## Test Runner Script

The `run_distributed_test.sh` script provides complete test management:

### Commands

- `setup`: Set up the distributed environment
- `build`: Build the test executable
- `run`: Execute the test
- `clean`: Clean up resources
- `all`: Complete workflow (setup → build → run)

### Options

```bash
-n, --num-nodes NUM     Number of nodes (default: 4)
-t, --test-case CASE    Test case: all, direct, range, broadcast (default: all)
-m, --mode MODE         Runtime mode: docker or local (default: docker)
-h, --help              Show help message
```

### Examples

```bash
# Run all tests with 4 nodes (default)
./run_distributed_test.sh all

# Run DirectHash test with 8 nodes
./run_distributed_test.sh -n 8 -t direct all

# Build only
./run_distributed_test.sh build

# Clean up Docker cluster
./run_distributed_test.sh clean

# Local mode for development
./run_distributed_test.sh -m local all
```

## Test Cases

### 1. DirectHash Test (`direct`)

Tests deterministic node placement using hash-based routing:

```cpp
for (int i = 0; i < 100; i++) {
  auto pool_query = chi::PoolQuery::DirectHash(i);
  // Allocate blocks on specific node
  // Write data
  // Read and verify
  // Free blocks
}
```

**Verifies**: Hash-based distribution, node-specific operations, data integrity

### 2. Range Test (`range`)

Tests range-based distribution across multiple nodes:

```cpp
auto pool_query = chi::PoolQuery::Range(0, num_nodes - 1);
// Operations distributed across node range
```

**Verifies**: Range query routing, multi-node coordination, consistent state

### 3. Broadcast Test (`broadcast`)

Tests broadcast operations to all nodes:

```cpp
auto pool_query = chi::PoolQuery::Broadcast();
// Operations sent to all nodes
```

**Verifies**: Broadcast routing, all-node operations, global consistency

## Jarvis Integration

The test integrates with Jarvis for pipeline-based execution:

### Package

Location: `test/jarvis_wrp_runtime/jarvis_wrp_runtime/wrp_distributed/`

```python
from jarvis_cd.core.pkg import Application

class WrpDistributed(Application):
    # Manages distributed test execution
    # Configures test parameters
    # Executes test and captures results
```

### Pipelines

**Local Pipeline** (`test/jarvis_wrp_runtime/pipelines/distributed_test_local.py`):
- Single-node testing
- Development environment
- Quick iteration

**Container Pipeline** (`test/jarvis_wrp_runtime/pipelines/distributed_test_container.py`):
- Multi-node testing
- Uses Docker cluster
- Production-like environment

### Usage with Jarvis

```bash
# Using Jarvis pipelines
jarvis pipeline run distributed_test_local
jarvis pipeline run distributed_test_container
```

## Hostfile

The `hostfile` contains IP addresses of all nodes in the cluster:

```
172.25.0.10
172.25.0.11
172.25.0.12
172.25.0.13
```

Generated automatically by `generate_hostfile.sh`. Mounted at `/etc/iowarp/hostfile` in containers.

## Building the Test

### With Script

```bash
./run_distributed_test.sh build
```

### Manually (Docker)

```bash
docker exec iowarp-distributed-node1 bash -c "
  cd /iowarp-runtime/build &&
  cmake --preset debug .. &&
  cmake --build . --target chimaera_distributed_bdev_tests -j8
"
```

### Manually (Local)

```bash
cd ../..  # Repository root
cmake --preset debug
cmake --build build --target chimaera_distributed_bdev_tests -j8
```

## Running Tests

### With Script

```bash
# All tests
./run_distributed_test.sh run

# Specific test case
./run_distributed_test.sh -t direct run
```

### Manually (Docker)

```bash
docker exec iowarp-distributed-node1 \
  /iowarp-runtime/build/bin/chimaera_distributed_bdev_tests \
  --num-nodes 4 --test-case all
```

### Manually (Local)

```bash
cd ../../build/bin
./chimaera_distributed_bdev_tests --num-nodes 1 --test-case all
```

## Docker Commands

### Start Cluster

```bash
cd test/distributed
docker-compose up -d
```

### View Logs

```bash
# All nodes
docker-compose logs -f

# Specific node
docker-compose logs -f iowarp-node1
```

### Check Status

```bash
docker-compose ps
```

### Stop Cluster

```bash
docker-compose down
```

### Access Node Shell

```bash
docker exec -it iowarp-distributed-node1 bash
```

## Troubleshooting

### Container Build Fails

Check available memory and ensure sufficient resources:

```bash
docker system df
docker system prune  # If needed
```

### Test Fails to Connect to Nodes

1. Verify hostfile is generated:
   ```bash
   cat hostfile
   ```

2. Check network connectivity:
   ```bash
   docker exec iowarp-distributed-node1 ping -c 3 172.25.0.11
   ```

3. Verify all containers are running:
   ```bash
   docker-compose ps
   ```

### Shared Memory Issues

Ensure `shm_size` in `docker-compose.yml` is sufficient (currently 16GB per node).

### Build Errors

Check build logs:

```bash
docker-compose logs iowarp-node1
```

Rebuild manually if needed:

```bash
docker exec iowarp-distributed-node1 bash -c "
  cd /iowarp-runtime &&
  rm -rf build &&
  mkdir build &&
  cd build &&
  cmake --preset debug .. &&
  cmake --build . --target chimaera_distributed_bdev_tests -j8
"
```

## Environment Variables

- `NUM_NODES`: Number of nodes in cluster (default: 4)
- `TEST_CASE`: Test case to run (default: all)
- `RUNTIME_MODE`: Execution mode - docker or local (default: docker)
- `CONTAINER_HOSTFILE`: Path to hostfile in container (default: /etc/iowarp/hostfile)

## Test Output

Test results are logged to:
- **Docker**: Container stdout (view with `docker-compose logs`)
- **Local**: Terminal stdout
- **Jarvis**: `/tmp/wrp_distributed_*/distributed_test_*.txt`

## Integration with CI/CD

The distributed test can be integrated into CI/CD pipelines:

```yaml
# Example GitLab CI
distributed_test:
  stage: test
  script:
    - cd test/distributed
    - ./run_distributed_test.sh all
  tags:
    - docker
```

## Performance Considerations

- **Docker overhead**: Tests in Docker may be slower than bare metal
- **Network latency**: Container networking adds latency vs. physical cluster
- **Shared memory**: Ensure sufficient shm_size for large-scale tests
- **Parallelism**: Build with `-j8` for faster compilation

## Future Enhancements

- [ ] Support for larger clusters (8, 16, 32 nodes)
- [ ] Performance benchmarking mode
- [ ] Fault injection testing
- [ ] Network partition simulation
- [ ] Kubernetes deployment option
- [ ] Integration with cloud providers (AWS, GCP, Azure)

## Related Files

- Test source: `test/unit/distributed/test_distributed_bdev.cc`
- Test CMake: `test/unit/distributed/CMakeLists.txt`
- Jarvis package: `test/jarvis_wrp_runtime/jarvis_wrp_runtime/wrp_distributed/pkg.py`
- Pipeline scripts: `test/jarvis_wrp_runtime/pipelines/distributed_test_*.py`

## See Also

- Main test README: `test/unit/README_task_archive_tests.md`
- BDev tests: `test/unit/test_bdev_chimod.cc`
- Docker deployment: `docker/README.md`
- Jarvis documentation: `test/jarvis_wrp_runtime/README.md`
