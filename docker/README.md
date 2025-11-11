# IOWarp Runtime Docker Deployment

This directory contains Docker configurations for building and deploying the IOWarp Runtime (Chimaera distributed task execution framework).

## Container Images

### Build Container (iowarp/iowarp-runtime-build:latest)
- **Dockerfile**: `build.Dockerfile`
- **Purpose**: Builds Chimaera runtime in release mode
- **Base Image**: `iowarp/cte-hermes-shm:latest`
- **Build Process**: Compiles source using CMake release preset
- **Contents**: Complete runtime installation in `/usr/local`

### Deployment Container (iowarp/iowarp-runtime:latest)
- **Dockerfile**: `deploy.Dockerfile`
- **Purpose**: Production runtime container
- **Base Image**: `iowarp/iowarp-runtime-build:latest`
- **Entry Point**: Automatically runs `chimaera_start_runtime`
- **Configuration**: Supports environment variables and config files

## Quick Start

### 1. Build Containers Locally

```bash
# Build build container
docker build -t iowarp/iowarp-runtime-build:latest -f docker/build.Dockerfile .

# Build deployment container
docker build -t iowarp/iowarp-runtime:latest -f docker/deploy.Dockerfile .
```

### 2. Deploy Using Docker Compose

```bash
cd docker
docker-compose up -d
```

This starts a 3-node Chimaera cluster with:
- Node 1: `172.20.0.10:5555` (exposed on host port 5555)
- Node 2: `172.20.0.11:5555` (exposed on host port 5556)
- Node 3: `172.20.0.12:5555` (exposed on host port 5557)

### 3. Check Cluster Status

```bash
# View running containers
docker-compose ps

# View logs from specific node
docker-compose logs -f chimaera-node1

# View logs from all nodes
docker-compose logs -f
```

### 4. Stop Cluster

```bash
docker-compose down
```

## Configuration

### Method 1: Environment Variables (Recommended)

Configure runtime via environment variables in `docker-compose.yml`:

```yaml
environment:
  # Worker configuration
  - CHI_SCHED_WORKERS=8
  - CHI_PROCESS_REAPER_THREADS=1

  # Memory configuration (in bytes)
  - CHI_MAIN_SEGMENT_SIZE=1073741824      # 1GB
  - CHI_CLIENT_DATA_SEGMENT_SIZE=536870912 # 512MB
  - CHI_RUNTIME_DATA_SEGMENT_SIZE=536870912 # 512MB

  # Network configuration
  - CHI_ZMQ_PORT=5555
  - CHI_NEIGHBORHOOD_SIZE=32

  # Logging configuration
  - CHI_LOG_LEVEL=info
  - CHI_LOG_FILE=/tmp/chimaera.log

  # Performance tuning
  - CHI_STACK_SIZE=65536
  - CHI_QUEUE_DEPTH=10000
  - CHI_LANE_MAP_POLICY=round_robin

  # Runtime configuration
  - CHI_HEARTBEAT_INTERVAL=1000
  - CHI_TASK_TIMEOUT=30000

  # Cluster configuration
  - CHI_HOSTFILE=/etc/chimaera/hostfile
```

### Method 2: Custom Configuration File

Mount a custom YAML configuration file:

```yaml
volumes:
  - ./chimaera_config.yaml:/etc/chimaera/chimaera_config.yaml:ro
```

See `chimaera_config.yaml` for an example configuration file.

## Critical Requirements

### Shared Memory Size

**CRITICAL**: The `shm_size` parameter must be greater than or equal to the sum of all memory segments:

```yaml
shm_size: 2gb  # Must be >= main_segment_size + client_data_segment_size + runtime_data_segment_size
```

Default configuration requires:
- Main segment: 1GB
- Client data: 512MB
- Runtime data: 512MB
- **Total: 2GB minimum**

### Hostfile Configuration

The hostfile contains IP addresses of all cluster nodes (one per line):

```
172.20.0.10
172.20.0.11
172.20.0.12
```

Mount the hostfile in each container:

```yaml
volumes:
  - ./hostfile:/etc/chimaera/hostfile:ro
environment:
  - CHI_HOSTFILE=/etc/chimaera/hostfile
```

## Network Configuration

The docker-compose setup creates a bridge network with static IPs:

```yaml
networks:
  chimaera-cluster:
    driver: bridge
    ipam:
      driver: default
      config:
        - subnet: 172.20.0.0/16
```

Each node gets a static IP for predictable routing:
- Node 1: `172.20.0.10`
- Node 2: `172.20.0.11`
- Node 3: `172.20.0.12`

## Advanced Usage

### Running Single Node

```bash
docker run -d \
  --name chimaera-runtime \
  -p 5555:5555 \
  --shm-size=2gb \
  -e CHI_SCHED_WORKERS=8 \
  -e CHI_MAIN_SEGMENT_SIZE=1073741824 \
  -e CHI_CLIENT_DATA_SEGMENT_SIZE=536870912 \
  -e CHI_RUNTIME_DATA_SEGMENT_SIZE=536870912 \
  iowarp/iowarp-runtime:latest
```

### Custom Configuration

```bash
docker run -d \
  --name chimaera-runtime \
  -p 5555:5555 \
  --shm-size=2gb \
  -v $(pwd)/chimaera_config.yaml:/etc/chimaera/chimaera_config.yaml:ro \
  iowarp/iowarp-runtime:latest
```

### Debugging

```bash
# Interactive shell in running container
docker exec -it chimaera-node1 /bin/bash

# View configuration being used
docker exec chimaera-node1 cat /etc/chimaera/chimaera_config.yaml

# View logs
docker logs chimaera-node1
```

## GitHub Actions CI/CD

The repository includes automated container builds via GitHub Actions (`.github/workflows/build-containers.yml`):

1. **build-build**: Builds `iowarp/iowarp-runtime-build:latest`
2. **build-deploy**: Builds `iowarp/iowarp-runtime:latest` (depends on build)

### Triggers
- Manual workflow dispatch
- Push to main branch (when relevant files change)

### Required Secrets
Configure in GitHub repository settings:
- `DOCKER_HUB_USERNAME`: Docker Hub username
- `DOCKER_HUB_ACCESS_TOKEN`: Docker Hub access token

## Files

- `build.Dockerfile` - Build container build
- `deploy.Dockerfile` - Deployment container build
- `docker-compose.yml` - Multi-node cluster orchestration
- `hostfile` - Cluster node IP addresses
- `chimaera_config.yaml` - Example runtime configuration
- `README.md` - This file

## Configuration Parameters

### Workers
- `CHI_SCHED_WORKERS`: Number of scheduler worker threads (default: 8)
- `CHI_PROCESS_REAPER_THREADS`: Number of process reaper threads (default: 1)

### Memory
- `CHI_MAIN_SEGMENT_SIZE`: Main shared memory segment size in bytes (default: 1GB)
- `CHI_CLIENT_DATA_SEGMENT_SIZE`: Client data segment size in bytes (default: 512MB)
- `CHI_RUNTIME_DATA_SEGMENT_SIZE`: Runtime data segment size in bytes (default: 512MB)

### Network
- `CHI_ZMQ_PORT`: ZeroMQ communication port (default: 5555)
- `CHI_NEIGHBORHOOD_SIZE`: Max queries for range query splitting (default: 32)
- `CHI_HOSTFILE`: Path to hostfile with cluster node IPs

### Logging
- `CHI_LOG_LEVEL`: Logging level - debug, info, warn, error (default: info)
- `CHI_LOG_FILE`: Log file path (default: /tmp/chimaera.log)

### Performance
- `CHI_STACK_SIZE`: Stack size per task in bytes (default: 64KB)
- `CHI_QUEUE_DEPTH`: Maximum queue depth (default: 10000)
- `CHI_LANE_MAP_POLICY`: Lane mapping policy - map_by_pid_tid, round_robin, random (default: round_robin)

### Runtime
- `CHI_HEARTBEAT_INTERVAL`: Heartbeat interval in milliseconds (default: 1000)
- `CHI_TASK_TIMEOUT`: Task timeout in milliseconds (default: 30000)

## Troubleshooting

### Container fails to start
- Check shared memory size: `shm_size` must be >= sum of all segments
- Verify hostfile is properly mounted and readable
- Check logs: `docker logs <container_name>`

### Network connectivity issues
- Ensure all nodes have unique static IPs
- Verify hostfile contains correct IP addresses
- Check firewall rules allow ZeroMQ port communication

### Performance issues
- Increase worker threads: `CHI_SCHED_WORKERS`
- Adjust memory segment sizes based on workload
- Tune queue depth: `CHI_QUEUE_DEPTH`
- Modify lane mapping policy: `CHI_LANE_MAP_POLICY`

## Support

For issues and questions:
- GitHub Issues: https://github.com/iowarp/chimaera/issues
- Documentation: See main project README and docs
