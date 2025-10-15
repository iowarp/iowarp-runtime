# Chimaera Runtime Docker Container

Docker container for running the Chimaera distributed task execution runtime as a daemon service.

## Overview

This Docker container packages the Chimaera runtime for easy deployment across distributed nodes. It handles:
- Runtime configuration generation from environment variables or config files
- Shared memory management for task execution
- Network configuration for inter-node communication
- Logging and monitoring

## Quick Start

### Build the Container

```bash
# From project root directory
cd docker
docker-compose build
```

### Start the Runtime Cluster

```bash
# Start all nodes defined in docker-compose.yml
docker-compose up -d

# Check status
docker-compose ps

# View logs from node 1
docker-compose logs -f chimaera-node1
```

### Stop the Runtime Cluster

```bash
# Graceful shutdown
docker-compose down

# Force kill and cleanup
docker-compose down --volumes
```

## Configuration

### Method 1: Environment Variables (Recommended)

Configure runtime parameters via environment variables in `docker-compose.yml`:

```yaml
environment:
  - CHI_SCHED_WORKERS=8                    # Scheduler worker threads
  - CHI_PROCESS_REAPER_WORKERS=1           # Process reaper threads
  - CHI_MAIN_SEGMENT_SIZE=1G               # Main segment size (supports G/M/K suffixes)
  - CHI_CLIENT_DATA_SEGMENT_SIZE=512M      # Client data segment size
  - CHI_RUNTIME_DATA_SEGMENT_SIZE=512M     # Runtime data segment size
  - CHI_ZMQ_PORT=5555                      # ZeroMQ port
  - CHI_LOG_LEVEL=info                     # Logging level (debug/info/warning/error)
  - CHI_STACK_SIZE=65536                   # Stack size per task (bytes)
  - CHI_QUEUE_DEPTH=10000                  # Task queue depth
  - CHI_HEARTBEAT_INTERVAL=1000            # Heartbeat interval (milliseconds)
  - CHI_TASK_TIMEOUT=30000                 # Task timeout (milliseconds)
  - CHI_SHM_SIZE=2147483648                # Total shared memory size (must match shm_size)
  - CHI_HOSTFILE=/etc/chimaera/hostfile    # Path to hostfile inside container
```

### Method 2: Custom Configuration File

Mount a custom YAML configuration file:

```yaml
volumes:
  - ./chimaera_config.yaml:/etc/chimaera/chimaera_config.yaml:ro
```

Configuration file format matches `config/chimaera_default.yaml`:

```yaml
workers:
  sched_threads: 8
  process_reaper_threads: 1

memory:
  main_segment_size: 1073741824      # 1GB
  client_data_segment_size: 536870912 # 512MB
  runtime_data_segment_size: 536870912 # 512MB

network:
  zmq_port: 5555

logging:
  level: "info"
  file: "/var/log/chimaera/chimaera.log"

performance:
  stack_size: 65536
  queue_depth: 10000
  lane_map_policy: "round_robin"

shared_memory:
  main_segment_name: "chi_main_segment_${USER}"
  client_data_segment_name: "chi_client_data_segment_${USER}"
  runtime_data_segment_name: "chi_runtime_data_segment_${USER}"

runtime:
  heartbeat_interval: 1000
  task_timeout: 30000
```

## Hostfile Configuration

Create a `hostfile` with IP addresses of cluster nodes (one per line):

```bash
# docker/hostfile
172.20.0.10
172.20.0.11
172.20.0.12
```

Mount the hostfile in `docker-compose.yml`:

```yaml
volumes:
  - ./hostfile:/etc/chimaera/hostfile:ro

environment:
  - CHI_HOSTFILE=/etc/chimaera/hostfile
```

## Shared Memory Requirements

**CRITICAL**: The container requires sufficient shared memory for task execution.

### Setting Shared Memory Size

Configure shared memory using the `shm_size` parameter in `docker-compose.yml`:

```yaml
services:
  chimaera-node1:
    shm_size: 2gb  # 2 gigabytes shared memory
```

Or using docker run:

```bash
docker run --shm-size=2g chimaera-runtime
```

### Memory Size Calculation

Shared memory size must be **greater than or equal to** the sum of all segments:

```
shm_size >= main_segment_size + client_data_segment_size + runtime_data_segment_size
```

Example:
- Main segment: 1GB
- Client data: 512MB
- Runtime data: 512MB
- **Minimum shm_size: 2GB**

**Note**: The entrypoint script validates shared memory size and will exit with an error if insufficient.

## Network Configuration

### ZeroMQ Port

Chimaera uses ZeroMQ for inter-node communication. The default port is 5555.

In `docker-compose.yml`, map container port to host port:

```yaml
ports:
  - "5555:5555"  # host_port:container_port
```

For multiple nodes on the same host, use different host ports:

```yaml
# Node 1
ports:
  - "5555:5555"

# Node 2
ports:
  - "5556:5555"

# Node 3
ports:
  - "5557:5555"
```

### Cluster Network

The compose file creates a dedicated bridge network for the cluster:

```yaml
networks:
  chimaera-net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/16
```

Each node gets a static IP:
- Node 1: 172.20.0.10
- Node 2: 172.20.0.11
- Node 3: 172.20.0.12

## Volume Mounts

### Log Persistence

Container logs are stored in named volumes:

```yaml
volumes:
  - chimaera-node1-logs:/var/log/chimaera
```

Access logs:

```bash
# View logs from volume
docker-compose logs -f chimaera-node1

# Or exec into container
docker exec -it chimaera-node1 tail -f /var/log/chimaera/chimaera.log
```

### Configuration Files

Mount custom configuration files as read-only:

```yaml
volumes:
  - ./chimaera_config.yaml:/etc/chimaera/chimaera_config.yaml:ro
  - ./hostfile:/etc/chimaera/hostfile:ro
```

## Running Single Node

To run a single Chimaera node:

```bash
docker run -d \
  --name chimaera-single \
  --shm-size=2g \
  -p 5555:5555 \
  -e CHI_SCHED_WORKERS=8 \
  -e CHI_MAIN_SEGMENT_SIZE=1G \
  -e CHI_CLIENT_DATA_SEGMENT_SIZE=512M \
  -e CHI_RUNTIME_DATA_SEGMENT_SIZE=512M \
  -e CHI_ZMQ_PORT=5555 \
  -e CHI_LOG_LEVEL=info \
  -v $(pwd)/hostfile:/etc/chimaera/hostfile:ro \
  chimaera-runtime:latest
```

## Scaling the Cluster

To add more nodes, copy an existing service definition in `docker-compose.yml` and update:
1. Service name (e.g., `chimaera-node4`)
2. Container name
3. Hostname
4. IP address (e.g., `172.20.0.13`)
5. Host port mapping (e.g., `5558:5555`)

Update the hostfile with the new node's IP:

```bash
echo "172.20.0.13" >> docker/hostfile
```

Restart the cluster:

```bash
docker-compose up -d
```

## Troubleshooting

### Shared Memory Issues

**Error**: "Total segment size exceeds shared memory size"

**Solution**: Increase `shm_size` in docker-compose.yml or reduce segment sizes

```yaml
shm_size: 4gb  # Increase from 2gb to 4gb
```

### Container Exits Immediately

Check logs for errors:

```bash
docker-compose logs chimaera-node1
```

Common issues:
- Insufficient shared memory (`shm_size` too small)
- Invalid configuration file format
- Missing library dependencies

### Network Communication Failures

Verify network connectivity between containers:

```bash
# From node 1, ping node 2
docker exec chimaera-node1 ping -c 3 172.20.0.11

# Check ZeroMQ port is listening
docker exec chimaera-node1 netstat -tlnp | grep 5555
```

### Port Conflicts

If host ports are already in use, change the port mapping:

```yaml
ports:
  - "5565:5555"  # Use different host port
```

## Building from Source

To build the container with custom Chimaera binaries:

1. Build Chimaera runtime:
   ```bash
   cmake --preset debug
   cmake --build build
   ```

2. Copy binaries to docker/bin/:
   ```bash
   mkdir -p docker/bin
   cp build/bin/chimaera_start_runtime docker/bin/
   cp build/bin/chimaera_stop_runtime docker/bin/
   cp build/bin/libcxx.so* docker/bin/
   cp build/bin/libchimaera_admin_*.so* docker/bin/
   ```

3. Build Docker image:
   ```bash
   docker build -f docker/Dockerfile -t chimaera-runtime:latest .
   ```

## Environment Variables Reference

| Variable | Description | Default | Example |
|----------|-------------|---------|---------|
| `CHI_SCHED_WORKERS` | Scheduler worker threads | 8 | 16 |
| `CHI_PROCESS_REAPER_WORKERS` | Process reaper threads | 1 | 2 |
| `CHI_MAIN_SEGMENT_SIZE` | Main segment size | 1G | 2G, 1073741824 |
| `CHI_CLIENT_DATA_SEGMENT_SIZE` | Client data segment size | 512M | 1G, 536870912 |
| `CHI_RUNTIME_DATA_SEGMENT_SIZE` | Runtime data segment size | 512M | 1G, 536870912 |
| `CHI_ZMQ_PORT` | ZeroMQ port | 5555 | 6000 |
| `CHI_LOG_LEVEL` | Logging level | info | debug, warning |
| `CHI_STACK_SIZE` | Stack size per task (bytes) | 65536 | 131072 |
| `CHI_QUEUE_DEPTH` | Task queue depth | 10000 | 20000 |
| `CHI_HEARTBEAT_INTERVAL` | Heartbeat interval (ms) | 1000 | 5000 |
| `CHI_TASK_TIMEOUT` | Task timeout (ms) | 30000 | 60000 |
| `CHI_SHM_SIZE` | Total shared memory (bytes) | 2147483648 | 4294967296 |
| `CHI_HOSTFILE` | Path to hostfile | (none) | /etc/chimaera/hostfile |
| `CHI_SERVER_CONF` | Path to config file | /etc/chimaera/chimaera_config.yaml | (custom path) |

## Best Practices

1. **Shared Memory**: Always set `shm_size` ≥ sum of segment sizes
2. **Logging**: Use persistent volumes for log directories
3. **Configuration**: Use environment variables for simple deployments, config files for complex setups
4. **Network**: Use static IPs for predictable cluster networking
5. **Security**: Run containers with minimal privileges (not as root when possible)
6. **Resource Limits**: Set CPU and memory limits in production:
   ```yaml
   deploy:
     resources:
       limits:
         cpus: '4'
         memory: 8G
   ```

## Production Deployment

For production deployments, consider:

1. **Orchestration**: Use Kubernetes for large-scale deployments
2. **Monitoring**: Add health checks and metrics collection
3. **Backup**: Persist important data to external volumes
4. **Security**: Use secrets management for sensitive configuration
5. **High Availability**: Deploy multiple replicas across availability zones

## Support

For issues and questions:
- GitHub Issues: https://github.com/anthropics/iowarp/issues
- Documentation: `docs/MODULE_DEVELOPMENT_GUIDE.md`
