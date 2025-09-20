# Jarvis Package Development Guide

## Table of Contents

1. [Package Development Fundamentals](#package-development-fundamentals)
2. [Package Types](#package-types)
3. [Development Workflow and Best Practices](#development-workflow-and-best-practices)
4. [Code Examples and Templates](#code-examples-and-templates)
5. [Testing Packages](#testing-packages)
6. [Contributing Packages to Repositories](#contributing-packages-to-repositories)
7. [Advanced Development Topics](#advanced-development-topics)

## Package Development Fundamentals

### What is a Jarvis Package?

A Jarvis package (pkg) is a self-contained unit that encapsulates the deployment logic for an application, service, or system component. Packages define how to configure, start, stop, and manage applications in a distributed environment.

### Package Architecture

Each package consists of:

1. **Package Class**: Python class inheriting from base types (Service, Application, or Interceptor)
2. **Configuration Schema**: Defines configurable parameters
3. **Lifecycle Methods**: Handle deployment stages (configure, start, stop, clean)
4. **Resource Management**: Handles directories, environment variables, and dependencies

### Package Directory Structure

```
my_package/
├── pkg.py              # Main package implementation
├── my_package.yaml     # Default configuration
├── README.md          # Documentation
├── config/            # Configuration templates
│   └── template.conf
└── test/              # Package tests
    └── test_pkg.py
```

### Base Package Class

All packages inherit from the `Pkg` base class, which provides:

```python
class Pkg:
    def __init__(self):
        self.pkg_dir = '...'        # Package directory location
        self.shared_dir = '...'     # Shared directory across nodes
        self.private_dir = '...'    # Node-local private directory
        self.env = {}               # Environment variables
        self.mod_env = {}           # Modified environment (with LD_PRELOAD)
        self.config = {}            # Package configuration
        self.global_id = '...'      # Globally unique ID
        self.pkg_id = '...'         # Package ID within pipeline
        self.jarvis = None          # Jarvis manager instance
```

### Key Concepts

#### Global ID vs Package ID

- **Package ID (pkg_id)**: Unique identifier within a pipeline (e.g., `hermes`)
- **Global ID (global_id)**: Fully qualified identifier (e.g., `test_pipeline.hermes`)

#### Directory Types

1. **pkg_dir**: Package source directory containing the Python class
2. **shared_dir**: Network-shared directory for common data
3. **private_dir**: Node-local directory for machine-specific data

#### Environment Management

- **env**: Standard environment variables for the pipeline
- **mod_env**: Modified environment including interceptor libraries (LD_PRELOAD)

## Package Types

### 1. Services

Long-running daemons that typically require manual stopping.

**Characteristics:**
- Persistent processes
- Network listeners
- State management
- Health monitoring

**Base class:**
```python
from jarvis_cd.basic.pkg import Service

class MyService(Service):
    def _init(self):
        # Initialize service-specific variables
        pass
```

**Example Services:**
- Storage systems (OrangeFS, Hermes)
- Databases (Redis)
- Message queues
- Monitoring daemons

### 2. Applications

Programs that run to completion automatically.

**Characteristics:**
- Finite execution time
- Input/output processing
- Result generation
- Automatic termination

**Base class:**
```python
from jarvis_cd.basic.pkg import Application

class MyApp(Application):
    def _init(self):
        # Initialize application-specific variables
        pass
```

**Example Applications:**
- Benchmarks (IOR, YCSB)
- Simulations (LAMMPS, WRF)
- Data processing tools
- Analysis programs

### 3. Interceptors

Libraries that intercept and modify system or library calls.

**Characteristics:**
- No standalone execution
- Environment modification
- Library preloading
- Transparent interception

**Base class:**
```python
from jarvis_cd.basic.pkg import Interceptor

class MyInterceptor(Interceptor):
    def _init(self):
        # Initialize interceptor-specific variables
        pass
```

**Example Interceptors:**
- I/O interceptors (POSIX, MPI-IO)
- Network interceptors
- Memory allocators
- Profiling tools

## Development Workflow and Best Practices

### Step 1: Create Package Structure

#### Using Jarvis CLI

```bash
# Create a new package in your repository
jarvis repo create my_package service

# This creates:
# my_repo/
#   └── my_repo/
#       └── my_package/
#           └── pkg.py
```

#### Manual Creation

```bash
mkdir -p my_repo/my_repo/my_package
touch my_repo/my_repo/my_package/pkg.py
touch my_repo/my_repo/my_package/__init__.py
```

### Step 2: Implement Package Class

#### Essential Methods

1. **_init**: Initialize package variables
2. **_configure_menu**: Define configuration parameters
3. **configure**: Process configuration and generate files
4. **start**: Launch the application/service
5. **stop**: Gracefully stop (services only)
6. **clean**: Remove generated data
7. **status**: Check running status (services only)

### Step 3: Development Best Practices

#### 1. Configuration Management

```python
def _configure_menu(self):
    """Define user-configurable parameters"""
    return [
        {
            'name': 'param_name',
            'msg': 'Parameter description',
            'type': str,            # str, int, float, bool
            'default': 'value',
            'required': False,
            'choices': ['opt1', 'opt2'],  # Optional: limit choices
            'aliases': ['alt_name']        # Alternative names
        }
    ]
```

#### 2. Error Handling

```python
def configure(self, **kwargs):
    try:
        self.update_config(kwargs)
        # Validate configuration
        if not self._validate_config():
            raise ValueError("Invalid configuration")
    except Exception as e:
        self.log_error(f"Configuration failed: {e}")
        raise
```

#### 3. Resource Management

```python
def start(self):
    # Create necessary directories
    Mkdir([self.config['data_dir'], 
           self.config['log_dir']])
    
    # Set up environment
    self.setenv('MY_VAR', 'value')
    
    # Launch with proper cleanup
    try:
        self.process = Exec(cmd, LocalExecInfo())
    except Exception as e:
        self.clean()  # Clean up on failure
        raise
```

#### 4. Logging

```python
def start(self):
    self.log_info("Starting my_package")
    self.log_debug(f"Configuration: {self.config}")
    
    # Redirect output to logs
    log_file = f"{self.shared_dir}/output.log"
    Exec(cmd, LocalExecInfo(pipe_stdout=log_file))
```

### Step 4: Testing Strategy

1. **Unit tests**: Test individual methods
2. **Integration tests**: Test with other packages
3. **Performance tests**: Benchmark resource usage
4. **Failure tests**: Test error handling

### Step 5: Documentation

Create comprehensive documentation:

```markdown
# My Package

## Overview
Brief description of what the package does

## Installation
Prerequisites and installation steps

## Configuration
Available parameters and their meanings

## Usage
Example configurations and use cases

## Troubleshooting
Common issues and solutions
```

## Code Examples and Templates

### Complete Service Example

```python
"""
Redis Cache Service Package
Provides a Redis in-memory cache service for distributed applications
"""

from jarvis_cd.basic.pkg import Service
from jarvis_util import *
import os
import time

class RedisCache(Service):
    """
    Redis cache service with clustering support
    """
    
    def _init(self):
        """Initialize Redis service variables"""
        self.daemon_proc = None
        self.redis_path = None
        self.config_file = f"{self.shared_dir}/redis.conf"
        self.data_dir = f"{self.private_dir}/redis_data"
        self.log_file = f"{self.private_dir}/redis.log"
        
    def _configure_menu(self):
        """Define Redis configuration parameters"""
        return [
            {
                'name': 'port',
                'msg': 'Redis server port',
                'type': int,
                'default': 6379,
                'class': 'network'
            },
            {
                'name': 'bind_addr',
                'msg': 'Bind address for Redis',
                'type': str,
                'default': '0.0.0.0',
                'class': 'network'
            },
            {
                'name': 'maxmemory',
                'msg': 'Maximum memory for Redis (e.g., 1gb)',
                'type': str,
                'default': '4gb',
                'class': 'resource'
            },
            {
                'name': 'maxmemory_policy',
                'msg': 'Eviction policy when memory limit reached',
                'type': str,
                'default': 'allkeys-lru',
                'choices': ['noeviction', 'allkeys-lru', 'volatile-lru', 
                          'allkeys-random', 'volatile-random', 'volatile-ttl'],
                'class': 'behavior'
            },
            {
                'name': 'persistence',
                'msg': 'Enable persistence to disk',
                'type': bool,
                'default': True,
                'class': 'storage'
            },
            {
                'name': 'cluster_mode',
                'msg': 'Enable Redis cluster mode',
                'type': bool,
                'default': False,
                'class': 'cluster'
            },
            {
                'name': 'cluster_nodes',
                'msg': 'Number of cluster nodes',
                'type': int,
                'default': 3,
                'class': 'cluster'
            }
        ]
    
    def configure(self, **kwargs):
        """Generate Redis configuration file"""
        self.update_config(kwargs, rebuild=False)
        
        # Create data directory
        Mkdir(self.data_dir)
        
        # Find Redis executable
        self.redis_path = self.find_executable('redis-server')
        if not self.redis_path:
            raise Exception('Redis not found. Please install Redis.')
        
        # Generate configuration file
        self._generate_config()
        
        # Set up cluster if enabled
        if self.config['cluster_mode']:
            self._setup_cluster()
    
    def _generate_config(self):
        """Generate Redis configuration file"""
        config_content = f"""
# Redis Configuration
bind {self.config['bind_addr']}
port {self.config['port']}
dir {self.data_dir}
logfile {self.log_file}
maxmemory {self.config['maxmemory']}
maxmemory-policy {self.config['maxmemory_policy']}
"""
        
        if self.config['persistence']:
            config_content += """
save 900 1
save 300 10
save 60 10000
dbfilename dump.rdb
"""
        
        if self.config['cluster_mode']:
            config_content += """
cluster-enabled yes
cluster-config-file nodes.conf
cluster-node-timeout 5000
"""
        
        # Write configuration file
        with open(self.config_file, 'w') as f:
            f.write(config_content)
    
    def _setup_cluster(self):
        """Set up Redis cluster configuration"""
        # Generate cluster node configurations
        for i in range(self.config['cluster_nodes']):
            node_port = self.config['port'] + i
            node_config = f"{self.shared_dir}/redis_{i}.conf"
            # Generate per-node configuration
            # ... (implementation details)
    
    def start(self):
        """Start Redis service"""
        self.log_info("Starting Redis service")
        
        # Start Redis server
        cmd = f"{self.redis_path} {self.config_file}"
        
        self.daemon_proc = Exec(
            cmd,
            PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env,
                exec_async=True,
                pipe_stdout=f"{self.log_file}.out",
                pipe_stderr=f"{self.log_file}.err"
            )
        )
        
        # Wait for Redis to start
        time.sleep(self.config['sleep'])
        
        # Verify Redis is running
        if not self._check_health():
            raise Exception("Redis failed to start")
        
        self.log_info("Redis service started successfully")
    
    def stop(self):
        """Stop Redis service gracefully"""
        self.log_info("Stopping Redis service")
        
        # Send shutdown command
        Exec(
            f"redis-cli -p {self.config['port']} shutdown",
            PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env
            )
        )
        
        # Wait for process to terminate
        if self.daemon_proc:
            self.daemon_proc.wait()
        
        self.log_info("Redis service stopped")
    
    def kill(self):
        """Force kill Redis service"""
        Kill('redis-server',
             PsshExecInfo(
                 hostfile=self.jarvis.hostfile,
                 env=self.env
             ))
    
    def clean(self):
        """Remove all Redis data and logs"""
        self.log_info("Cleaning Redis data")
        
        Rm([self.data_dir, self.log_file, self.config_file],
           PsshExecInfo(
               hostfile=self.jarvis.hostfile,
               env=self.env
           ))
    
    def status(self):
        """Check if Redis is running"""
        try:
            result = Exec(
                f"redis-cli -p {self.config['port']} ping",
                LocalExecInfo(collect_output=True, hide_output=True)
            )
            return result.stdout.strip() == "PONG"
        except:
            return False
    
    def _check_health(self):
        """Internal health check"""
        max_retries = 10
        for i in range(max_retries):
            if self.status():
                return True
            time.sleep(1)
        return False
    
    def _get_stat(self, stat_dict):
        """Collect Redis statistics"""
        if self.status():
            # Get Redis info
            info = Exec(
                f"redis-cli -p {self.config['port']} info",
                LocalExecInfo(collect_output=True, hide_output=True)
            )
            
            # Parse statistics
            for line in info.stdout.split('\n'):
                if 'used_memory_human' in line:
                    stat_dict[f'{self.pkg_id}.memory'] = line.split(':')[1]
                elif 'connected_clients' in line:
                    stat_dict[f'{self.pkg_id}.clients'] = line.split(':')[1]
```

### Complete Application Example

```python
"""
Scientific Simulation Application Package
Runs Gray-Scott reaction-diffusion simulation
"""

from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import os
import pathlib

class GrayScottSimulation(Application):
    """
    Gray-Scott reaction-diffusion simulation with ADIOS2 I/O
    """
    
    def _init(self):
        """Initialize simulation variables"""
        self.gray_scott_path = None
        self.adios_config = f"{self.shared_dir}/adios2.xml"
        
    def _configure_menu(self):
        """Define simulation parameters"""
        return [
            {
                'name': 'L',
                'msg': 'Grid size per dimension',
                'type': int,
                'default': 128,
                'class': 'simulation'
            },
            {
                'name': 'steps',
                'msg': 'Number of simulation steps',
                'type': int,
                'default': 1000,
                'class': 'simulation'
            },
            {
                'name': 'checkpoint_interval',
                'msg': 'Steps between checkpoints',
                'type': int,
                'default': 100,
                'class': 'io'
            },
            {
                'name': 'Du',
                'msg': 'Diffusion rate of U',
                'type': float,
                'default': 0.2,
                'class': 'physics'
            },
            {
                'name': 'Dv',
                'msg': 'Diffusion rate of V',
                'type': float,
                'default': 0.1,
                'class': 'physics'
            },
            {
                'name': 'F',
                'msg': 'Feed rate',
                'type': float,
                'default': 0.01,
                'class': 'physics'
            },
            {
                'name': 'k',
                'msg': 'Kill rate',
                'type': float,
                'default': 0.05,
                'class': 'physics'
            },
            {
                'name': 'dt',
                'msg': 'Time step',
                'type': float,
                'default': 2.0,
                'class': 'physics'
            },
            {
                'name': 'nprocs',
                'msg': 'Number of MPI processes',
                'type': int,
                'default': 4,
                'class': 'parallel'
            },
            {
                'name': 'ppn',
                'msg': 'Processes per node',
                'type': int,
                'default': 1,
                'class': 'parallel'
            },
            {
                'name': 'output_dir',
                'msg': 'Output directory for results',
                'type': str,
                'default': '/tmp/gray_scott_output',
                'class': 'io'
            },
            {
                'name': 'viz',
                'msg': 'Enable visualization output',
                'type': bool,
                'default': True,
                'class': 'io'
            },
            {
                'name': 'adios_engine',
                'msg': 'ADIOS2 engine type',
                'type': str,
                'default': 'BP4',
                'choices': ['BP4', 'BP5', 'HDF5', 'SST'],
                'class': 'io'
            }
        ]
    
    def configure(self, **kwargs):
        """Configure simulation parameters"""
        self.update_config(kwargs, rebuild=False)
        
        # Find Gray-Scott executable
        self.gray_scott_path = self.find_executable('gray-scott')
        if not self.gray_scott_path:
            raise Exception('Gray-Scott simulation not found')
        
        # Create output directory
        Mkdir(self.config['output_dir'])
        
        # Generate ADIOS2 configuration
        self._generate_adios_config()
        
        # Generate simulation settings file
        self._generate_settings()
    
    def _generate_adios_config(self):
        """Generate ADIOS2 XML configuration"""
        adios_xml = f"""<?xml version="1.0"?>
<adios-config>
    <io name="SimulationOutput">
        <engine type="{self.config['adios_engine']}">
            <parameter key="Threads" value="4"/>
            <parameter key="ProfileUnits" value="Microseconds"/>
            <parameter key="InitialBufferSize" value="1Mb"/>
        </engine>
        
        <transport type="File">
            <parameter key="Library" value="posix"/>
        </transport>
    </io>
    
    <io name="VisualizationOutput">
        <engine type="BP4">
            <parameter key="RendezvousReaderCount" value="0"/>
            <parameter key="QueueLimit" value="5"/>
            <parameter key="QueueFullPolicy" value="Block"/>
        </engine>
    </io>
    
    <io name="CheckpointOutput">
        <engine type="BP4">
            <parameter key="Threads" value="2"/>
            <parameter key="CollectiveMetadata" value="Off"/>
        </engine>
    </io>
</adios-config>
"""
        with open(self.adios_config, 'w') as f:
            f.write(adios_xml)
    
    def _generate_settings(self):
        """Generate simulation settings file"""
        settings = {
            'L': self.config['L'],
            'steps': self.config['steps'],
            'plotgap': self.config['checkpoint_interval'],
            'Du': self.config['Du'],
            'Dv': self.config['Dv'],
            'F': self.config['F'],
            'k': self.config['k'],
            'dt': self.config['dt'],
            'noise': 0.01,
            'output': self.config['output_dir'],
            'adios_config': self.adios_config
        }
        
        settings_file = f"{self.shared_dir}/settings.json"
        import json
        with open(settings_file, 'w') as f:
            json.dump(settings, f, indent=2)
    
    def start(self):
        """Run the simulation"""
        self.log_info(f"Starting Gray-Scott simulation")
        self.log_info(f"Grid size: {self.config['L']}^3")
        self.log_info(f"Steps: {self.config['steps']}")
        self.log_info(f"Processes: {self.config['nprocs']}")
        
        # Build command
        cmd = [
            self.gray_scott_path,
            f"{self.shared_dir}/settings.json",
            f"{self.config['output_dir']}/gs.bp"
        ]
        
        # Set up environment
        self.setenv('ADIOS2_CONFIG', self.adios_config)
        
        # Run simulation
        self.start_time = time.time()
        
        Exec(
            ' '.join(cmd),
            MpiExecInfo(
                env=self.mod_env,
                hostfile=self.jarvis.hostfile,
                nprocs=self.config['nprocs'],
                ppn=self.config['ppn']
            )
        )
        
        self.end_time = time.time()
        self.runtime = self.end_time - self.start_time
        
        self.log_info(f"Simulation completed in {self.runtime:.2f} seconds")
        
        # Generate visualization if enabled
        if self.config['viz']:
            self._generate_visualization()
    
    def _generate_visualization(self):
        """Generate visualization from output data"""
        self.log_info("Generating visualization")
        
        viz_cmd = f"gs-plot {self.config['output_dir']}/gs.bp"
        
        try:
            Exec(viz_cmd, LocalExecInfo(env=self.env))
            self.log_info("Visualization generated successfully")
        except:
            self.log_warning("Visualization generation failed")
    
    def stop(self):
        """Stop is not typically needed for applications"""
        pass
    
    def clean(self):
        """Remove simulation output and temporary files"""
        self.log_info("Cleaning simulation data")
        
        Rm(
            [self.config['output_dir'], 
             f"{self.shared_dir}/settings.json",
             self.adios_config],
            PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env
            )
        )
    
    def _get_stat(self, stat_dict):
        """Collect simulation statistics"""
        stat_dict[f'{self.pkg_id}.runtime'] = self.runtime
        stat_dict[f'{self.pkg_id}.grid_size'] = self.config['L']
        stat_dict[f'{self.pkg_id}.steps'] = self.config['steps']
        stat_dict[f'{self.pkg_id}.processes'] = self.config['nprocs']
        
        # Calculate performance metrics
        grid_points = self.config['L'] ** 3
        total_updates = grid_points * self.config['steps']
        stat_dict[f'{self.pkg_id}.updates_per_sec'] = total_updates / self.runtime
```

### Complete Interceptor Example

```python
"""
POSIX I/O Interceptor Package
Intercepts POSIX I/O calls for optimization or monitoring
"""

from jarvis_cd.basic.pkg import Interceptor
from jarvis_util import *
import os

class PosixInterceptor(Interceptor):
    """
    POSIX I/O interceptor for transparent I/O optimization
    """
    
    def _init(self):
        """Initialize interceptor variables"""
        self.interceptor_lib = None
        self.config_file = f"{self.shared_dir}/posix_interceptor.conf"
        
    def _configure_menu(self):
        """Define interceptor configuration"""
        return [
            {
                'name': 'mode',
                'msg': 'Interception mode',
                'type': str,
                'default': 'optimize',
                'choices': ['optimize', 'monitor', 'debug'],
                'class': 'behavior'
            },
            {
                'name': 'buffer_size',
                'msg': 'I/O buffer size',
                'type': str,
                'default': '4m',
                'class': 'performance'
            },
            {
                'name': 'prefetch',
                'msg': 'Enable prefetching',
                'type': bool,
                'default': True,
                'class': 'performance'
            },
            {
                'name': 'async_io',
                'msg': 'Enable asynchronous I/O',
                'type': bool,
                'default': False,
                'class': 'performance'
            },
            {
                'name': 'compression',
                'msg': 'Enable compression',
                'type': bool,
                'default': False,
                'class': 'feature'
            },
            {
                'name': 'compression_level',
                'msg': 'Compression level (1-9)',
                'type': int,
                'default': 6,
                'class': 'feature'
            },
            {
                'name': 'exclude_paths',
                'msg': 'Paths to exclude from interception',
                'type': list,
                'default': ['/dev', '/proc', '/sys'],
                'class': 'filter'
            },
            {
                'name': 'include_paths',
                'msg': 'Paths to include for interception',
                'type': list,
                'default': [],
                'class': 'filter'
            },
            {
                'name': 'log_calls',
                'msg': 'Log all intercepted calls',
                'type': bool,
                'default': False,
                'class': 'debug'
            },
            {
                'name': 'stats_file',
                'msg': 'File to write statistics',
                'type': str,
                'default': None,
                'class': 'monitoring'
            }
        ]
    
    def configure(self, **kwargs):
        """Configure the interceptor"""
        self.update_config(kwargs, rebuild=False)
        
        # Find interceptor library
        self.interceptor_lib = self.find_library('posix_interceptor')
        if not self.interceptor_lib:
            raise Exception('POSIX interceptor library not found')
        
        self.log_info(f"Found interceptor library at {self.interceptor_lib}")
        
        # Generate configuration file
        self._generate_config()
        
        # Set up environment variables
        self._setup_environment()
    
    def _generate_config(self):
        """Generate interceptor configuration file"""
        config = {
            'mode': self.config['mode'],
            'buffer_size': self.config['buffer_size'],
            'prefetch': self.config['prefetch'],
            'async_io': self.config['async_io'],
            'compression': {
                'enabled': self.config['compression'],
                'level': self.config['compression_level']
            },
            'paths': {
                'exclude': self.config['exclude_paths'],
                'include': self.config['include_paths']
            },
            'debug': {
                'log_calls': self.config['log_calls'],
                'stats_file': self.config['stats_file']
            }
        }
        
        import json
        with open(self.config_file, 'w') as f:
            json.dump(config, f, indent=2)
    
    def _setup_environment(self):
        """Set up environment variables for interception"""
        # Set configuration file path
        self.setenv('POSIX_INTERCEPTOR_CONFIG', self.config_file)
        
        # Set mode-specific variables
        if self.config['mode'] == 'debug':
            self.setenv('POSIX_INTERCEPTOR_DEBUG', '1')
            self.setenv('POSIX_INTERCEPTOR_VERBOSE', '1')
        
        if self.config['stats_file']:
            self.setenv('POSIX_INTERCEPTOR_STATS', 
                       os.path.expandvars(self.config['stats_file']))
        
        # Set performance parameters
        self.setenv('POSIX_INTERCEPTOR_BUFFER_SIZE', 
                   self.config['buffer_size'])
        
        if self.config['async_io']:
            self.setenv('POSIX_INTERCEPTOR_ASYNC', '1')
    
    def modify_env(self):
        """Modify environment to enable interception"""
        self.log_info("Enabling POSIX I/O interception")
        
        # Add interceptor library to LD_PRELOAD
        self.prepend_env('LD_PRELOAD', self.interceptor_lib, sep=':')
        
        # Ensure configuration is loaded
        self.setenv('POSIX_INTERCEPTOR_CONFIG', self.config_file)
        
        self.log_info(f"Interception mode: {self.config['mode']}")
        
    def clean(self):
        """Clean up interceptor files"""
        files_to_remove = [self.config_file]
        
        if self.config['stats_file']:
            files_to_remove.append(self.config['stats_file'])
        
        Rm(files_to_remove,
           PsshExecInfo(
               hostfile=self.jarvis.hostfile,
               env=self.env
           ))
    
    def _get_stat(self, stat_dict):
        """Collect interceptor statistics"""
        if self.config['stats_file'] and os.path.exists(self.config['stats_file']):
            # Parse statistics file
            import json
            with open(self.config['stats_file'], 'r') as f:
                stats = json.load(f)
                
            stat_dict[f'{self.pkg_id}.total_calls'] = stats.get('total_calls', 0)
            stat_dict[f'{self.pkg_id}.bytes_read'] = stats.get('bytes_read', 0)
            stat_dict[f'{self.pkg_id}.bytes_written'] = stats.get('bytes_written', 0)
            stat_dict[f'{self.pkg_id}.cache_hits'] = stats.get('cache_hits', 0)
            stat_dict[f'{self.pkg_id}.cache_misses'] = stats.get('cache_misses', 0)
```

### Helper Utilities Template

```python
"""
Package Helper Utilities
Common functions for package development
"""

from jarvis_util import *
import os
import socket
import subprocess

class PackageHelpers:
    """
    Reusable helper functions for packages
    """
    
    @staticmethod
    def find_free_port(start_port=8000, end_port=9000, host='localhost'):
        """Find an available network port"""
        for port in range(start_port, end_port):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                try:
                    s.bind((host, port))
                    return port
                except OSError:
                    continue
        raise Exception(f"No free port found in range {start_port}-{end_port}")
    
    @staticmethod
    def wait_for_service(host, port, timeout=30):
        """Wait for a service to become available"""
        import time
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.settimeout(1)
                    s.connect((host, port))
                    return True
            except (socket.timeout, ConnectionRefusedError):
                time.sleep(1)
        return False
    
    @staticmethod
    def parse_size(size_str):
        """Parse size strings like '4gb', '512mb' to bytes"""
        units = {
            'b': 1,
            'kb': 1024,
            'mb': 1024**2,
            'gb': 1024**3,
            'tb': 1024**4
        }
        
        size_str = size_str.lower().strip()
        for unit, multiplier in units.items():
            if size_str.endswith(unit):
                number = float(size_str[:-len(unit)])
                return int(number * multiplier)
        
        # If no unit, assume bytes
        return int(size_str)
    
    @staticmethod
    def format_size(size_bytes):
        """Format bytes to human-readable string"""
        for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
            if size_bytes < 1024.0:
                return f"{size_bytes:.2f} {unit}"
            size_bytes /= 1024.0
        return f"{size_bytes:.2f} PB"
    
    @staticmethod
    def generate_hostfile(nodes, ppn=1):
        """Generate MPI hostfile from node list"""
        hostfile_content = ""
        for node in nodes:
            if ppn > 1:
                hostfile_content += f"{node}:{ppn}\n"
            else:
                hostfile_content += f"{node}\n"
        return hostfile_content
    
    @staticmethod
    def check_executable(name):
        """Check if an executable exists in PATH"""
        try:
            subprocess.run(['which', name], 
                         check=True, 
                         capture_output=True)
            return True
        except subprocess.CalledProcessError:
            return False
    
    @staticmethod
    def get_node_resources():
        """Get local node resource information"""
        import psutil
        
        return {
            'cpu_count': psutil.cpu_count(),
            'memory_total': psutil.virtual_memory().total,
            'memory_available': psutil.virtual_memory().available,
            'disk_usage': {
                mount.mountpoint: psutil.disk_usage(mount.mountpoint)._asdict()
                for mount in psutil.disk_partitions()
                if mount.mountpoint.startswith('/')
            }
        }
```

## Testing Packages

### Unit Testing Framework

```python
"""
Test suite for MyPackage
"""

import unittest
import tempfile
import shutil
from unittest.mock import Mock, patch
from my_package.pkg import MyPackage

class TestMyPackage(unittest.TestCase):
    """Test cases for MyPackage"""
    
    def setUp(self):
        """Set up test environment"""
        # Create temporary directories
        self.temp_dir = tempfile.mkdtemp()
        self.pkg = MyPackage()
        
        # Mock Jarvis manager
        self.pkg.jarvis = Mock()
        self.pkg.jarvis.hostfile = Mock()
        
        # Set up package directories
        self.pkg.shared_dir = f"{self.temp_dir}/shared"
        self.pkg.private_dir = f"{self.temp_dir}/private"
        self.pkg.pkg_dir = f"{self.temp_dir}/pkg"
        
        # Create directories
        os.makedirs(self.pkg.shared_dir)
        os.makedirs(self.pkg.private_dir)
        
    def tearDown(self):
        """Clean up test environment"""
        shutil.rmtree(self.temp_dir)
    
    def test_configuration(self):
        """Test package configuration"""
        # Test default configuration
        menu = self.pkg._configure_menu()
        self.assertIsInstance(menu, list)
        self.assertTrue(len(menu) > 0)
        
        # Test configuration update
        config = {
            'port': 8080,
            'threads': 16
        }
        self.pkg.configure(**config)
        
        self.assertEqual(self.pkg.config['port'], 8080)
        self.assertEqual(self.pkg.config['threads'], 16)
    
    def test_invalid_configuration(self):
        """Test invalid configuration handling"""
        with self.assertRaises(ValueError):
            self.pkg.configure(port='invalid')
    
    @patch('jarvis_util.shell.exec.Exec')
    def test_start(self, mock_exec):
        """Test service start"""
        # Configure package
        self.pkg.configure(port=8080)
        
        # Start service
        self.pkg.start()
        
        # Verify Exec was called
        mock_exec.assert_called()
        
    @patch('jarvis_util.shell.exec.Exec')
    def test_stop(self, mock_exec):
        """Test service stop"""
        # Configure and start
        self.pkg.configure(port=8080)
        self.pkg.start()
        
        # Stop service
        self.pkg.stop()
        
        # Verify stop command was executed
        mock_exec.assert_called()
    
    def test_file_generation(self):
        """Test configuration file generation"""
        self.pkg.configure(port=8080)
        
        # Check if configuration file was created
        config_file = f"{self.pkg.shared_dir}/config.conf"
        self.assertTrue(os.path.exists(config_file))
        
        # Verify file contents
        with open(config_file, 'r') as f:
            content = f.read()
            self.assertIn('8080', content)
    
    def test_environment_setup(self):
        """Test environment variable setup"""
        self.pkg.configure(mode='debug')
        
        # Check environment variables
        self.assertIn('DEBUG', self.pkg.env)
        self.assertEqual(self.pkg.env['DEBUG'], '1')
    
    def test_resource_discovery(self):
        """Test resource discovery functionality"""
        # Mock resource graph
        self.pkg.jarvis.resource_graph = Mock()
        self.pkg.jarvis.resource_graph.find_storage.return_value = '/mnt/storage'
        
        # Test storage discovery
        storage = self.pkg._find_storage()
        self.assertEqual(storage, '/mnt/storage')
    
    def test_error_recovery(self):
        """Test error handling and recovery"""
        # Simulate start failure
        with patch('jarvis_util.shell.exec.Exec', side_effect=Exception("Start failed")):
            with self.assertRaises(Exception):
                self.pkg.start()
        
        # Verify cleanup was called
        # Check that temporary files were removed
        self.assertFalse(os.path.exists(f"{self.pkg.shared_dir}/temp.lock"))

class TestIntegration(unittest.TestCase):
    """Integration tests for package interactions"""
    
    def test_pipeline_integration(self):
        """Test package in pipeline context"""
        # Create mock pipeline
        pipeline = Mock()
        pipeline.sub_pkgs = []
        
        # Add package to pipeline
        pkg = MyPackage()
        pipeline.sub_pkgs.append(pkg)
        
        # Test pipeline operations
        for pkg in pipeline.sub_pkgs:
            pkg.configure(test_mode=True)
            # Verify configuration was applied
            self.assertTrue(pkg.config['test_mode'])

if __name__ == '__main__':
    unittest.main()
```

### Testing Best Practices

#### 1. Test Coverage

Ensure comprehensive test coverage:

```bash
# Run tests with coverage
python -m pytest --cov=my_package tests/

# Generate coverage report
coverage html
```

#### 2. Integration Testing

```python
def test_full_workflow():
    """Test complete package workflow"""
    pkg = MyPackage()
    
    # Full lifecycle test
    pkg.configure(test_param='value')
    pkg.start()
    assert pkg.status() == True
    pkg.stop()
    assert pkg.status() == False
    pkg.clean()
```

#### 3. Performance Testing

```python
def test_performance():
    """Test package performance"""
    import time
    
    pkg = MyPackage()
    pkg.configure(threads=16)
    
    start_time = time.time()
    pkg.start()
    startup_time = time.time() - start_time
    
    # Assert performance requirements
    assert startup_time < 10  # Should start within 10 seconds
```

## Contributing Packages to Repositories

### Repository Structure

#### Creating a Custom Repository

```bash
# Create repository structure
mkdir -p my_org_repo/my_org_repo
cd my_org_repo

# Initialize repository
cat > my_org_repo/__init__.py << EOF
"""My Organization's Jarvis Package Repository"""
__version__ = "1.0.0"
EOF

# Register with Jarvis
jarvis repo add /path/to/my_org_repo
```

#### Repository Layout

```
my_org_repo/
├── README.md
├── LICENSE
├── setup.py
├── requirements.txt
├── my_org_repo/
│   ├── __init__.py
│   ├── package1/
│   │   ├── __init__.py
│   │   ├── pkg.py
│   │   └── README.md
│   ├── package2/
│   │   ├── __init__.py
│   │   ├── pkg.py
│   │   └── README.md
│   └── common/
│       └── utils.py
└── tests/
    ├── test_package1.py
    └── test_package2.py
```

### Contribution Guidelines

#### 1. Code Quality Standards

```python
# Follow PEP 8 style guide
# Use type hints where appropriate
def configure(self, **kwargs: dict) -> None:
    """
    Configure the package.
    
    Args:
        **kwargs: Configuration parameters
        
    Raises:
        ValueError: If configuration is invalid
    """
    pass
```

#### 2. Documentation Requirements

Every package must include:

1. **Docstrings**: All classes and methods
2. **README.md**: Package overview and usage
3. **Examples**: Configuration examples
4. **API Reference**: Complete parameter documentation

#### 3. Testing Requirements

```bash
# Run all tests before submission
python -m pytest tests/

# Check code style
flake8 my_package/

# Check type hints
mypy my_package/
```

### Submission Process

#### 1. Fork and Clone

```bash
# Fork the repository on GitHub
git clone https://github.com/your-username/ppi-jarvis-cd.git
cd ppi-jarvis-cd
```

#### 2. Create Feature Branch

```bash
git checkout -b add-my-package
```

#### 3. Add Your Package

```bash
# Copy package to builtin directory
cp -r my_package builtin/builtin/

# Add tests
cp -r my_package_tests test/unit/
```

#### 4. Update Documentation

Add your package to the documentation:

```markdown
## Available Packages

### My Package
- **Type**: Service/Application/Interceptor
- **Description**: Brief description
- **Configuration**: Link to detailed docs
```

#### 5. Submit Pull Request

```bash
# Commit changes
git add .
git commit -m "Add MyPackage for feature X"

# Push to your fork
git push origin add-my-package

# Create pull request on GitHub
```

### Package Metadata

Include metadata in your package:

```python
class MyPackage(Service):
    """
    Package metadata
    """
    
    __version__ = "1.0.0"
    __author__ = "Your Name"
    __email__ = "your.email@example.com"
    __license__ = "BSD-3-Clause"
    __description__ = "Brief package description"
    __dependencies__ = ["dep1>=1.0", "dep2"]
    __supported_platforms__ = ["linux"]
```

## Advanced Development Topics

### 1. Dynamic Configuration

Implement adaptive configuration based on resources:

```python
def configure(self, **kwargs):
    """Configure with resource awareness"""
    self.update_config(kwargs)
    
    # Detect available resources
    resources = self.jarvis.resource_graph.get_node_resources()
    
    # Adapt configuration
    if not self.config.get('threads'):
        # Use 75% of available cores
        self.config['threads'] = int(resources['cpu_count'] * 0.75)
    
    if not self.config.get('memory'):
        # Use 50% of available memory
        available_mb = resources['memory_available'] / (1024**2)
        self.config['memory'] = f"{int(available_mb * 0.5)}m"
```

### 2. Multi-Stage Packages

Implement packages with multiple execution stages:

```python
class MultiStagePackage(Application):
    def start(self):
        """Execute multi-stage workflow"""
        self.stage1_preprocess()
        self.stage2_compute()
        self.stage3_postprocess()
    
    def stage1_preprocess(self):
        """Data preprocessing stage"""
        self.log_info("Starting preprocessing")
        # Implementation
    
    def stage2_compute(self):
        """Main computation stage"""
        self.log_info("Starting computation")
        # Implementation
    
    def stage3_postprocess(self):
        """Post-processing stage"""
        self.log_info("Starting post-processing")
        # Implementation
```

### 3. Package Dependencies

Handle inter-package dependencies:

```python
class DependentPackage(Service):
    def _init(self):
        self.dependencies = ['hermes_run', 'orangefs']
    
    def configure(self, **kwargs):
        """Configure with dependency checking"""
        # Check dependencies
        for dep in self.dependencies:
            if not self._check_dependency(dep):
                raise Exception(f"Required package {dep} not found")
        
        self.update_config(kwargs)
    
    def _check_dependency(self, pkg_name):
        """Check if dependency is available"""
        return pkg_name in self.pipeline.sub_pkgs_dict
```

### 4. Custom Execution Protocols

Implement custom execution mechanisms:

```python
class CustomExecPackage(Application):
    def start(self):
        """Custom execution protocol"""
        # Use container execution
        if self.config['use_container']:
            self._run_in_container()
        # Use SLURM
        elif self.config['use_slurm']:
            self._run_with_slurm()
        # Standard execution
        else:
            self._run_standard()
    
    def _run_in_container(self):
        """Execute in container"""
        cmd = f"docker run -v {self.shared_dir}:/data my_image {self.cmd}"
        Exec(cmd, LocalExecInfo())
    
    def _run_with_slurm(self):
        """Submit as SLURM job"""
        from jarvis_util.shell.slurm_exec import SlurmExec, SlurmExecInfo
        
        SlurmExec(
            self.cmd,
            SlurmExecInfo(
                nodes=self.config['nodes'],
                time='01:00:00',
                partition='compute'
            )
        )
```

### 5. Event-Driven Packages

Implement event handlers and callbacks:

```python
class EventDrivenPackage(Service):
    def _init(self):
        self.event_handlers = {}
        self.register_events()
    
    def register_events(self):
        """Register event handlers"""
        self.event_handlers['data_ready'] = self.on_data_ready
        self.event_handlers['error'] = self.on_error
        self.event_handlers['complete'] = self.on_complete
    
    def on_data_ready(self, data):
        """Handle data ready event"""
        self.log_info(f"Data ready: {data}")
        self.process_data(data)
    
    def on_error(self, error):
        """Handle error event"""
        self.log_error(f"Error occurred: {error}")
        self.cleanup()
    
    def on_complete(self):
        """Handle completion event"""
        self.log_info("Processing complete")
        self.finalize()
```

### 6. Package Plugins

Create extensible packages with plugin support:

```python
class PluggablePackage(Service):
    def _init(self):
        self.plugins = []
        self.plugin_dir = f"{self.pkg_dir}/plugins"
    
    def load_plugins(self):
        """Load all available plugins"""
        import importlib
        
        for plugin_file in os.listdir(self.plugin_dir):
            if plugin_file.endswith('.py'):
                module_name = plugin_file[:-3]
                module = importlib.import_module(f"plugins.{module_name}")
                
                # Find plugin class
                for name, obj in inspect.getmembers(module):
                    if inspect.isclass(obj) and issubclass(obj, Plugin):
                        plugin = obj()
                        self.plugins.append(plugin)
                        self.log_info(f"Loaded plugin: {name}")
    
    def execute_plugins(self, hook_name, *args, **kwargs):
        """Execute all plugins for a given hook"""
        for plugin in self.plugins:
            if hasattr(plugin, hook_name):
                getattr(plugin, hook_name)(*args, **kwargs)
```

### 7. Monitoring and Metrics

Implement comprehensive monitoring:

```python
class MonitoredPackage(Service):
    def _init(self):
        self.metrics = {}
        self.metric_collectors = []
    
    def start(self):
        """Start with monitoring"""
        # Start metric collection
        self.start_monitoring()
        
        # Start service
        super().start()
    
    def start_monitoring(self):
        """Start metric collection"""
        from threading import Thread
        import psutil
        
        def collect_metrics():
            while self.running:
                # Collect system metrics
                self.metrics['cpu'] = psutil.cpu_percent()
                self.metrics['memory'] = psutil.virtual_memory().percent
                self.metrics['disk_io'] = psutil.disk_io_counters()
                
                # Custom metrics
                self.collect_custom_metrics()
                
                time.sleep(self.config['metric_interval'])
        
        monitor_thread = Thread(target=collect_metrics)
        monitor_thread.daemon = True
        monitor_thread.start()
    
    def collect_custom_metrics(self):
        """Collect package-specific metrics"""
        # Implementation specific to package
        pass
    
    def export_metrics(self):
        """Export metrics to file or monitoring system"""
        import json
        
        metrics_file = f"{self.shared_dir}/metrics.json"
        with open(metrics_file, 'w') as f:
            json.dump(self.metrics, f, indent=2)
```

### 8. Fault Tolerance

Implement robust error handling and recovery:

```python
class FaultTolerantPackage(Service):
    def start(self):
        """Start with fault tolerance"""
        max_retries = self.config.get('max_retries', 3)
        retry_delay = self.config.get('retry_delay', 5)
        
        for attempt in range(max_retries):
            try:
                self._start_service()
                break
            except Exception as e:
                self.log_error(f"Start attempt {attempt + 1} failed: {e}")
                
                if attempt < max_retries - 1:
                    self.log_info(f"Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    self._cleanup_failed_start()
                else:
                    self.log_error("Max retries exceeded")
                    raise
    
    def _cleanup_failed_start(self):
        """Clean up after failed start"""
        # Kill any zombie processes
        try:
            Kill(self.service_name, 
                 PsshExecInfo(hostfile=self.jarvis.hostfile))
        except:
            pass
        
        # Clean up temporary files
        self._clean_temp_files()
    
    def health_check(self):
        """Periodic health check"""
        if not self.status():
            self.log_warning("Service unhealthy, attempting recovery")
            self.recover()
    
    def recover(self):
        """Attempt to recover failed service"""
        self.stop()
        time.sleep(2)
        self.start()
```

## Conclusion

This guide provides comprehensive coverage of Jarvis package development, from basic concepts to advanced techniques. By following these patterns and best practices, you can create robust, reusable packages that integrate seamlessly with the Jarvis CD ecosystem.

### Key Takeaways

1. **Understand Package Types**: Choose the appropriate base class for your use case
2. **Follow Best Practices**: Use consistent patterns and thorough error handling
3. **Test Thoroughly**: Implement comprehensive unit and integration tests
4. **Document Well**: Provide clear documentation and examples
5. **Consider Performance**: Optimize for resource usage and scalability
6. **Plan for Production**: Implement monitoring, logging, and fault tolerance

### Resources

- **Source Code**: https://github.com/iowarp/ppi-jarvis-cd
- **Documentation**: https://grc.iit.edu/docs/jarvis/jarvis-cd/index
- **Examples**: See `builtin/builtin/` directory for reference implementations
- **Support**: File issues on GitHub for questions and bug reports

Happy package development!