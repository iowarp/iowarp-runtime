# IoWarp Runtime Deployment Guide

This guide describes how to deploy and configure the IoWarp runtime (Chimaera distributed task execution framework).

## Table of Contents

- [Quick Start](#quick-start)
- [Configuration Methods](#configuration-methods)
- [Environment Variables](#environment-variables)
- [Configuration File Format](#configuration-file-format)
- [Deployment Scenarios](#deployment-scenarios)
- [Troubleshooting](#troubleshooting)

## Quick Start

### Basic Deployment

```bash
# Set configuration file path
export CHI_SERVER_CONF=/path/to/chimaera_config.yaml

# Start the runtime
chimaera_start_runtime
```

### Docker Deployment

```bash
cd docker
docker-compose up -d
```

## Configuration Methods

The runtime supports multiple configuration methods with the following precedence:

1. **Environment Variable (Recommended)**: `CHI_SERVER_CONF` or `WRP_RUNTIME_CONF`
   - Points to a YAML configuration file
   - Most flexible and explicit method
   - `CHI_SERVER_CONF` is checked first, then `WRP_RUNTIME_CONF`

2. **Default Configuration**: Built-in defaults
   - Used when no configuration file is specified
   - Suitable for development and testing

### Configuration File Path Resolution

The runtime reads the configuration file path from environment variables with the following precedence:

1. **CHI_SERVER_CONF** (checked first)
2. **WRP_RUNTIME_CONF** (fallback if CHI_SERVER_CONF is not set)
3. Built-in defaults (if neither environment variable is set)

**Examples**:

```bash
# Method 1: Using CHI_SERVER_CONF (recommended)
export CHI_SERVER_CONF=/etc/chimaera/chimaera_config.yaml
chimaera_start_runtime

# Method 2: Using WRP_RUNTIME_CONF (alternative)
export WRP_RUNTIME_CONF=/etc/iowarp/runtime_config.yaml
chimaera_start_runtime

# Method 3: No configuration (uses defaults)
chimaera_start_runtime
```

## Environment Variables

### Configuration File Location

| Variable | Description | Default | Priority |
|----------|-------------|---------|----------|
| `CHI_SERVER_CONF` | Path to YAML configuration file | (empty - uses defaults) | Primary |
| `WRP_RUNTIME_CONF` | Alternative path to YAML configuration file | (empty - uses defaults) | Secondary |

**Note**: The runtime checks `CHI_SERVER_CONF` first. If not set, it falls back to `WRP_RUNTIME_CONF`. If neither is set, built-in defaults are used.

**Important**: The runtime does NOT read individual `CHI_*` environment variables (like `CHI_SCHED_WORKERS`, `CHI_ZMQ_PORT`, etc.). All configuration must be specified in a YAML file pointed to by `CHI_SERVER_CONF` or `WRP_RUNTIME_CONF`.

## Configuration File Format

The configuration file uses YAML format with the following sections:

### Complete Configuration Example

```yaml
# Chimaera Runtime Configuration
# Based on config/chimaera_default.yaml

# Worker thread configuration
workers:
  sched_threads: 8           # Unified scheduler worker threads
  process_reaper_threads: 1  # Process reaper threads

# Memory segment configuration
memory:
  main_segment_size: 1073741824      # 1GB (or use: 1G)
  client_data_segment_size: 536870912 # 512MB (or use: 512M)
  runtime_data_segment_size: 536870912 # 512MB (or use: 512M)

# Network configuration
networking:
  port: 5555
  neighborhood_size: 32  # Maximum number of queries when splitting range queries
  hostfile: "/etc/chimaera/hostfile"  # Optional: path to hostfile for distributed mode

# Logging configuration
logging:
  level: "info"  # Options: debug, info, warning, error
  file: "/tmp/chimaera.log"

# Runtime configuration
runtime:
  stack_size: 65536  # 64KB per task
  queue_depth: 10000
  lane_map_policy: "round_robin"  # Options: map_by_pid_tid, round_robin, random
  heartbeat_interval: 1000  # milliseconds
```

### Size Format

Memory sizes can be specified in multiple formats:
- **Bytes**: `1073741824`
- **Suffixed**: `1G`, `512M`, `64K`
- **Human-readable**: Automatically parsed by HSHM ConfigParse

### Hostfile Format

For distributed deployments, create a hostfile with one IP address per line:

```
172.20.0.10
172.20.0.11
172.20.0.12
```

Then reference it in the configuration:

```yaml
networking:
  hostfile: "/etc/chimaera/hostfile"
```

### Distributed Testing Configuration

For testing with smaller neighborhoods (stress testing):

```yaml
# Based on test/unit/distributed/chimaera_distributed.yaml
workers:
  sched_threads: 8
  process_reaper_threads: 1

memory:
  main_segment_size: 4294967296      # 4GB
  client_data_segment_size: 2147483648 # 2GB
  runtime_data_segment_size: 2147483648 # 2GB

networking:
  port: 5555
  neighborhood_size: 4  # Small size to stress test range/broadcast queries
  hostfile: "/etc/iowarp/hostfile"

logging:
  level: "info"
  file: "/tmp/chimaera.log"

runtime:
  stack_size: 65536
  queue_depth: 10000
  lane_map_policy: "round_robin"
  heartbeat_interval: 1000
```

## Deployment Scenarios

### Scenario 1: Single Node Development

**Configuration**: Use default values

```bash
# No configuration needed - uses built-in defaults
chimaera_start_runtime
```

**Default Values**:
- 8 scheduler workers, 1 process reaper
- 1GB main segment, 512MB client/runtime segments
- Port 5555, neighborhood size 32
- Round-robin lane mapping

### Scenario 2: Single Node with Custom Configuration

**Configuration**: YAML file with custom settings

```bash
# Create configuration file
cat > /etc/chimaera/chimaera_config.yaml <<EOF
workers:
  sched_threads: 16

memory:
  main_segment_size: 4G
  client_data_segment_size: 2G
  runtime_data_segment_size: 2G

networking:
  port: 5555

logging:
  level: "debug"
  file: "/var/log/chimaera.log"
EOF

# Set configuration path and start
export CHI_SERVER_CONF=/etc/chimaera/chimaera_config.yaml
chimaera_start_runtime
```

### Scenario 3: Multi-Node Cluster with Docker (Default Configuration)

**Configuration**: Docker Compose using default configuration

The Docker container automatically sets `WRP_RUNTIME_CONF=/etc/wrp_runtime/wrp_runtime_config.yaml`.

1. **Create hostfile** (`docker/hostfile`):
   ```
   172.20.0.10
   172.20.0.11
   172.20.0.12
   ```

2. **Deploy cluster** (uses default config):
   ```bash
   cd docker
   docker-compose up -d
   ```

The default configuration provides:
- 8 scheduler workers, 1 process reaper
- 1GB main segment, 512MB client/runtime segments
- Port 5555, neighborhood size 32
- Info logging to /tmp/chimaera.log

### Scenario 3b: Multi-Node Cluster with Custom Configuration

**Configuration**: Docker Compose with custom configuration file

1. **Create custom configuration** (`docker/chimaera_cluster.yaml`):
   ```yaml
   workers:
     sched_threads: 16
     process_reaper_threads: 1

   memory:
     main_segment_size: 4G
     client_data_segment_size: 2G
     runtime_data_segment_size: 2G

   networking:
     port: 5555
     neighborhood_size: 32
     hostfile: "/etc/chimaera/hostfile"

   logging:
     level: "debug"
     file: "/tmp/chimaera.log"

   runtime:
     stack_size: 65536
     queue_depth: 20000
     lane_map_policy: "round_robin"
     heartbeat_interval: 1000
   ```

2. **Create hostfile** (`docker/hostfile`):
   ```
   172.20.0.10
   172.20.0.11
   172.20.0.12
   ```

3. **Mount custom config in docker-compose.yml**:
   ```yaml
   version: '3.8'

   services:
     chimaera-node1:
       image: iowarp/iowarp-runtime:latest
       networks:
         chimaera-cluster:
           ipv4_address: 172.20.0.10
       volumes:
         # Override default config by mounting to WRP_RUNTIME_CONF path
         - ./chimaera_cluster.yaml:/etc/wrp_runtime/wrp_runtime_config.yaml:ro
         - ./hostfile:/etc/chimaera/hostfile:ro
       shm_size: 8gb  # Must be >= 4GB + 2GB + 2GB
   ```

4. **Deploy cluster**:
   ```bash
   cd docker
   docker-compose up -d
   ```

### Scenario 4: Docker with CHI_SERVER_CONF Override

**Configuration**: Override WRP_RUNTIME_CONF using CHI_SERVER_CONF

```bash
# Create custom configuration
cat > chimaera_custom.yaml <<EOF
workers:
  sched_threads: 16
  process_reaper_threads: 1

memory:
  main_segment_size: 8G
  client_data_segment_size: 4G
  runtime_data_segment_size: 4G

networking:
  port: 5555
  neighborhood_size: 32

logging:
  level: "info"
  file: "/tmp/chimaera.log"

runtime:
  stack_size: 65536
  queue_depth: 10000
  lane_map_policy: "round_robin"
  heartbeat_interval: 1000
EOF

# Run with custom config (CHI_SERVER_CONF takes precedence over WRP_RUNTIME_CONF)
docker run -d \
  -v $(pwd)/chimaera_custom.yaml:/etc/wrp_runtime/custom_config.yaml:ro \
  -e CHI_SERVER_CONF=/etc/wrp_runtime/custom_config.yaml \
  --shm-size=16gb \
  iowarp/iowarp-runtime:latest
```

### Scenario 5: Production Deployment

**Configuration**: Optimized for production workloads

```yaml
# production.yaml
workers:
  sched_threads: 32  # Match CPU core count
  process_reaper_threads: 2

memory:
  main_segment_size: 16G
  client_data_segment_size: 8G
  runtime_data_segment_size: 8G

networking:
  port: 5555
  neighborhood_size: 64  # Larger for better load distribution
  hostfile: "/etc/chimaera/hostfile"

logging:
  level: "warning"  # Reduce log verbosity
  file: "/var/log/chimaera/chimaera.log"

runtime:
  stack_size: 131072  # 128KB for larger tasks
  queue_depth: 50000  # Higher queue depth
  lane_map_policy: "round_robin"
  heartbeat_interval: 500   # More frequent heartbeats
```

**Deployment**:
```bash
export CHI_SERVER_CONF=/etc/chimaera/production.yaml
chimaera_start_runtime
```

## Troubleshooting

### Issue: Runtime fails to start

**Symptoms**: `Failed to initialize Chimaera runtime`

**Solutions**:
1. Check configuration file path:
   ```bash
   echo $CHI_SERVER_CONF
   cat $CHI_SERVER_CONF
   ```

2. Verify YAML syntax:
   ```bash
   python3 -c "import yaml; yaml.safe_load(open('$CHI_SERVER_CONF'))"
   ```

3. Check shared memory availability:
   ```bash
   # Linux: Check available shared memory
   df -h /dev/shm

   # Docker: Verify shm_size setting
   docker inspect <container> | grep -i shm
   ```

### Issue: Configuration not loaded

**Symptoms**: Runtime uses default values instead of configuration file

**Solutions**:
1. Ensure `CHI_SERVER_CONF` or `WRP_RUNTIME_CONF` is set before starting runtime:
   ```bash
   echo $CHI_SERVER_CONF
   echo $WRP_RUNTIME_CONF
   ```
2. Check file permissions (must be readable):
   ```bash
   ls -l $CHI_SERVER_CONF
   ```
3. Verify file path is absolute, not relative
4. Check runtime logs for configuration loading messages

### Issue: Docker container shared memory exhausted

**Symptoms**: `Failed to allocate shared memory segment`

**Solutions**:
1. Increase Docker `shm_size`:
   ```yaml
   shm_size: 4gb  # Must be >= sum(main + client_data + runtime_data)
   ```

2. Reduce segment sizes in configuration:
   ```yaml
   memory:
     main_segment_size: 512M
     client_data_segment_size: 256M
     runtime_data_segment_size: 256M
   ```

### Issue: Network connection failures in distributed mode

**Symptoms**: Tasks not routing to remote nodes

**Solutions**:
1. Verify hostfile contains correct IP addresses:
   ```bash
   cat /etc/chimaera/hostfile
   ```

2. Check network connectivity:
   ```bash
   # Test connectivity to each node
   nc -zv 172.20.0.10 5555
   nc -zv 172.20.0.11 5555
   ```

3. Verify port configuration matches across nodes:
   ```yaml
   networking:
     port: 5555  # Must be same on all nodes
   ```

### Issue: High memory usage

**Symptoms**: Runtime consuming more memory than expected

**Solutions**:
1. Reduce segment sizes:
   ```yaml
   memory:
     main_segment_size: 512M
     client_data_segment_size: 256M
     runtime_data_segment_size: 256M
   ```

2. Reduce queue depth:
   ```yaml
   performance:
     queue_depth: 5000  # Lower value
   ```

3. Monitor with logging:
   ```yaml
   logging:
     level: "debug"  # Enable detailed logging
   ```

### Issue: Performance bottlenecks

**Symptoms**: Tasks executing slowly or queuing up

**Solutions**:
1. Increase worker threads:
   ```yaml
   workers:
     sched_threads: 16  # Match CPU core count
   ```

2. Optimize lane mapping:
   ```yaml
   performance:
     lane_map_policy: "map_by_pid_tid"  # Better affinity
   ```

3. Increase queue depth:
   ```yaml
   performance:
     queue_depth: 20000
   ```

4. Adjust neighborhood size for distributed workloads:
   ```yaml
   networking:
     neighborhood_size: 64  # Larger for better distribution
   ```

## Configuration Best Practices

1. **Configuration File Management**:
   - Always use YAML configuration files instead of relying on defaults
   - Keep configuration files in version control
   - Use descriptive names for configuration files (e.g., `production.yaml`, `development.yaml`)
   - Document any deviations from default values with comments

2. **Memory Sizing**:
   - Set `main_segment_size` based on total task count and data size
   - Allocate at least 50% of main_segment_size for client/runtime segments
   - Ensure Docker `shm_size` is 20-30% larger than sum of segments
   - Example: If total segments = 2GB, set `shm_size: 2.5gb`

3. **Worker Threads**:
   - Set `sched_threads` equal to CPU core count for CPU-bound workloads
   - Use fewer threads for I/O-bound workloads
   - Keep `process_reaper_threads` at 1 unless debugging

4. **Network Tuning**:
   - Use smaller `neighborhood_size` (4-8) for stress testing
   - Use larger values (32-64) for production distributed deployments
   - Keep port consistent across all cluster nodes
   - Always specify hostfile path for distributed deployments

5. **Logging**:
   - Use `debug` level during development
   - Use `info` level for normal operation
   - Use `warning` or `error` for production
   - Ensure log directory is writable

6. **Runtime Configuration**:
   - Start with default `stack_size` (64KB) and increase if tasks need more
   - Increase `queue_depth` for bursty workloads
   - Use `round_robin` lane mapping for general workloads
   - Adjust `heartbeat_interval` based on monitoring requirements

## References

- Example configurations:
  - Development: `config/chimaera_default.yaml`
  - Distributed testing: `test/unit/distributed/chimaera_distributed.yaml`
- Docker setup: `docker/deploy.Dockerfile`, `docker/docker-compose.yml`
- Runtime source: `util/chimaera_start_runtime.cc`
- Configuration manager: `src/config_manager.cc`, `include/chimaera/config_manager.h`
