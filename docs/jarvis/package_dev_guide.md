# Jarvis-CD Package Development Guide

This guide explains how to develop custom packages for Jarvis-CD, including the repository structure, abstract methods, and implementation examples.

## Table of Contents

1. [Repository Structure](#repository-structure)
2. [Package Types](#package-types)
3. [Abstract Methods](#abstract-methods)
4. [Environment Variables](#environment-variables)
5. [Configuration](#configuration)
6. [Package Directory Structure](#package-directory-structure)
7. [Execution System](#execution-system)
8. [Interceptor Development](#interceptor-development)
9. [Implementation Examples](#implementation-examples)
10. [Best Practices](#best-practices)

## Repository Structure

Jarvis-CD packages are organized in repositories with the following structure:

```
my_repo/
├── package1/
│   ├── __init__.py
│   └── pkg.py          # Main package implementation
├── package2/
│   ├── __init__.py
│   └── pkg.py
└── __init__.py
```

### Key Requirements

1. **Repository Root**: Contains subdirectories for each package
2. **Package Directory**: Named after the package (e.g., `ior`, `redis`)
3. **Main File**: Must be named `pkg.py` (not `package.py`)
4. **Class Name**: Must be the capitalized package name (e.g., `Ior`, `Redis`)
5. **Init Files**: Include `__init__.py` files for proper Python module structure

### Adding Repositories

```bash
# Add a repository to Jarvis
jarvis repo add /path/to/my_repo

# List registered repositories
jarvis repo list
```

## Package Types

Jarvis-CD provides several base classes for different types of packages:

### 1. SimplePackage (jarvis_cd.basic.pkg.SimplePackage)

**Most common base class** - Use this for packages that need interceptor support. Most builtin packages inherit from this.

```python
from jarvis_cd.basic.pkg import SimplePackage

class MyPackage(SimplePackage):
    def _init(self):
        # Initialize variables
        self.my_var = None
    
    def _configure_menu(self):
        # Get base menu from SimplePackage (includes interceptors)
        base_menu = super()._configure_menu()
        
        # Add package-specific menu items
        package_menu = [
            {
                'name': 'input_file',
                'msg': 'Input file path',
                'type': str,
                'default': 'input.dat'
            }
        ]
        
        return base_menu + package_menu
    
    def _configure(self, **kwargs):
        # Configure the package - update_config() called automatically
        
    def start(self):
        # Process interceptors automatically
        self._process_interceptors()
        # Run the package
        pass
```

### 2. Application (jarvis_cd.basic.pkg.Application)

For applications that run and complete automatically (e.g., benchmarks, data processing tools).

```python
from jarvis_cd.basic.pkg import Application

class MyApp(Application):
    def _init(self):
        # Initialize variables
        self.output_file = None
        
    def _configure_menu(self):
        return [
            {
                'name': 'output_file',
                'msg': 'Output file path',
                'type': str,
                'default': 'output.dat'
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
    
    def start(self):
        # Run the application
        pass
    
    def stop(self):
        # Usually not needed for applications
        pass
```

### 3. Service (jarvis_cd.basic.pkg.Service)

For long-running services that need manual stopping (e.g., databases, web servers).

```python
from jarvis_cd.basic.pkg import Service

class MyService(Service):
    def _init(self):
        # Initialize variables
        self.daemon_process = None
        
    def _configure_menu(self):
        return [
            {
                'name': 'port',
                'msg': 'Service port',
                'type': int,
                'default': 8080
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
    
    def start(self):
        # Start the service
        pass
    
    def stop(self):
        # Stop the service
        pass
    
    def status(self) -> str:
        # Return service status
        return "running"
```

### 4. Interceptor (jarvis_cd.basic.pkg.Interceptor)

For packages that modify environment variables to intercept system calls (e.g., profiling tools, I/O interceptors). Interceptors work by modifying `LD_PRELOAD` and other environment variables to inject custom libraries into target applications.

```python
from jarvis_cd.basic.pkg import Interceptor

class MyInterceptor(Interceptor):
    def _init(self):
        # Initialize variables
        self.interceptor_lib = None
        
    def _configure_menu(self):
        return [
            {
                'name': 'library_path',
                'msg': 'Path to interceptor library',
                'type': str,
                'default': '/usr/lib/libinterceptor.so'
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # Find the interceptor library
        lib_path = self.find_library('interceptor')
        if not lib_path:
            lib_path = self.config['library_path']
            
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Interceptor library not found: {lib_path}")
            
        self.interceptor_lib = lib_path
    
    def modify_env(self):
        # Add interceptor library to LD_PRELOAD
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            new_preload = f"{self.interceptor_lib}:{current_preload}"
        else:
            new_preload = self.interceptor_lib
            
        self.setenv('LD_PRELOAD', new_preload)
    
    def start(self):
        # Automatically calls modify_env()
        super().start()
```

## Abstract Methods

All packages inherit from the base `Pkg` class and can override these methods:

### Required Override Methods

#### `_init(self)`
**Purpose**: Initialize package-specific variables  
**Called**: During package instantiation  
**Notes**: Don't assume `self.config` is initialized. Set default values to None.

```python
def _init(self):
    """Initialize package-specific variables"""
    self.my_variable = None
    self.start_time = None
    self.output_file = None
```

#### `_configure_menu(self) -> List[Dict[str, Any]]`
**Purpose**: Define configuration options for the package  
**Called**: When generating CLI help or configuration forms  
**Returns**: List of configuration parameter dictionaries

```python
def _configure_menu(self):
    """Define configuration options"""
    return [
        {
            'name': 'param_name',
            'msg': 'Description of parameter',
            'type': str,
            'default': 'default_value',
            'choices': ['option1', 'option2'],  # Optional
            'args': [],                         # For nested parameters
        }
    ]
```

**Important**: If inheriting from `SimplePackage`, call the parent method:
```python
def _configure_menu(self):
    """Define configuration options"""
    # Get base menu from SimplePackage (includes interceptors)
    base_menu = super()._configure_menu()
    
    # Add package-specific menu items
    package_menu = [
        {
            'name': 'my_param',
            'msg': 'My parameter description',
            'type': str,
            'default': 'default_value'
        }
    ]
    
    return base_menu + package_menu
```

### Lifecycle Methods

#### `_configure(self, **kwargs)`
**Purpose**: Handle package configuration  
**Called**: When package is configured via CLI or programmatically  
**Use**: Set up environment variables and generate application-specific configuration files
**Note**: Override `_configure()`, not `configure()`. The public `configure()` method automatically calls `self.update_config()` before calling `_configure()`.

```python
def _configure(self, **kwargs):
    """Configure the package"""
    # No need to call self.update_config() - it's done automatically
    
    # Set environment variables
    if self.config['custom_path']:
        self.setenv('MY_APP_PATH', self.config['custom_path'])
        self.prepend_env('PATH', self.config['custom_path'] + '/bin')
    
    # Generate application-specific configuration files
    # This is where you create config files, validate parameters, etc.
```

#### `start(self)`
**Purpose**: Start the package (application, service, or interceptor)  
**Called**: During `jarvis ppl run` and `jarvis ppl start`

```python
def start(self):
    """Start the package"""
    # Use self.mod_env for environment variables
    # Use self.jarvis.hostfile for MPI execution
    cmd = ['my_application', '--config', self.config['config_file']]
    Exec(' '.join(cmd), LocalExecInfo(env=self.mod_env)).run()
```

#### `stop(self)`
**Purpose**: Stop the package  
**Called**: During `jarvis ppl run` and `jarvis ppl stop`

```python
def stop(self):
    """Stop the package"""
    # Gracefully shutdown the application/service
    pass
```

#### `kill(self)`
**Purpose**: Forcibly terminate the package  
**Called**: During `jarvis ppl kill`

```python
def kill(self):
    """Forcibly kill the package"""
    from jarvis_cd.shell.process import Kill
    Kill('my_application', PsshExecInfo(hostfile=self.jarvis.hostfile)).run()
```

#### `clean(self)`
**Purpose**: Clean up package data and temporary files  
**Called**: During `jarvis ppl clean`

```python
def clean(self):
    """Clean package data"""
    from jarvis_cd.shell.process import Rm
    Rm(self.config['output_dir'], 
       PsshExecInfo(hostfile=self.jarvis.hostfile)).run()
```

#### `status(self) -> str`
**Purpose**: Return current package status  
**Called**: During `jarvis ppl status`

```python
def status(self) -> str:
    """Return package status"""
    # Check if process is running, files exist, etc.
    return "running" | "stopped" | "error" | "unknown"
```

## Environment Variables

Jarvis-CD manages environment variables through a pipeline-wide system where environment modifications are propagated between packages.

### Environment Loading

When a pipeline is first loaded, the environment is constructed from:
1. **Pipeline Configuration** (`pipeline.yaml` - `env` section)
2. **Environment File** (`env.yaml` in pipeline directory)  
3. **System Environment** (current shell environment)

### Package Environment Dictionaries

Each package has two environment dictionaries:

### `self.env`
- **Purpose**: Shared environment across the pipeline
- **Source**: Loaded from pipeline environment + modifications from previous packages
- **Propagation**: Changes are propagated to subsequent packages in the pipeline
- **Usage**: Use this to set environment variables that should affect later packages

### `self.mod_env`
- **Purpose**: Package-specific environment copy
- **Source**: Deep copy of `self.env` at package load time
- **Scope**: Private to the package, not propagated
- **Usage**: Used in execution commands, modified by interceptors

### Environment Methods

```python
# Set an environment variable
self.setenv('MY_VAR', 'value')

# Prepend to PATH-like variables
self.prepend_env('PATH', '/new/path')
self.prepend_env('LD_LIBRARY_PATH', '/new/lib')

# Track existing environment variables
self.track_env({'EXISTING_VAR': os.environ.get('EXISTING_VAR', '')})
```

### Environment Propagation

Environment changes are automatically propagated between packages:

```python
# Package 1 (e.g., compiler setup)
def configure(self, **kwargs):
    self.setenv('CC', '/usr/bin/gcc-9')
    self.prepend_env('PATH', '/opt/compiler/bin')

# Package 2 (automatically receives Package 1's environment)
def start(self):
    # self.env now contains CC and PATH from Package 1
    # self.mod_env is a deep copy for this package's use
    Exec('make', LocalExecInfo(env=self.mod_env)).run()
```

### Usage in _configure()

**Always use environment methods in the `_configure()` method:**

```python
def _configure(self, **kwargs):
    """Configure package and set environment"""
    # No need to call self.update_config() - it's done automatically
    
    # Set application-specific environment (will be propagated to later packages)
    if self.config['install_path']:
        self.setenv('MY_APP_HOME', self.config['install_path'])
        self.prepend_env('PATH', f"{self.config['install_path']}/bin")
        self.prepend_env('LD_LIBRARY_PATH', f"{self.config['install_path']}/lib")
    
    # Track system environment if needed
    if 'CUDA_HOME' in os.environ:
        self.track_env({'CUDA_HOME': os.environ['CUDA_HOME']})
```

## Configuration

### Configuration Parameters

Each parameter in `_configure_menu()` supports these fields:

- **`name`** (required): Parameter name
- **`msg`** (required): Description for help text
- **`type`** (required): `str`, `int`, `float`, `bool`
- **`default`**: Default value
- **`choices`**: List of valid options
- **`aliases`**: Alternative parameter names
- **`required`**: Whether parameter is mandatory

### Configuration Access

```python
# Access configuration values
def start(self):
    input_file = self.config['input_file']
    num_procs = self.config['nprocs']
    debug_mode = self.config['debug']
```

## Package Directory Structure

Jarvis-CD provides three key directories that packages can use for organizing files, templates, and configuration:

### `self.pkg_dir` - Package Source Directory

The **package directory** contains the package's source code, templates, and static configuration files.

- **Location**: Points to the package's source directory (e.g., `builtin/builtin/my_package/`)
- **Purpose**: Access template files, default configurations, and package resources
- **Usage**: Read-only access to package-specific resources
- **Common subdirectories**: 
  - `config/` - Template configuration files
  - `templates/` - File templates
  - `scripts/` - Helper scripts

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Copy template configuration from package source
    template_path = f'{self.pkg_dir}/config/app_config.xml'
    output_path = f'{self.shared_dir}/app_config.xml'
    
    # Copy and customize template file
    self.copy_template_file(template_path, output_path, 
                           replacements={'PORT': self.config['port']})
```

#### Example Package Structure
```
my_package/
├── pkg.py                    # Main package implementation
├── config/                   # Template configurations
│   ├── app.xml              # Application config template
│   ├── logging.conf         # Logging configuration
│   └── defaults.yaml        # Default settings
├── templates/               # File templates
│   ├── Dockerfile.j2        # Container template
│   └── systemd.service      # Service template
└── scripts/                 # Helper scripts
    ├── setup.sh             # Installation script
    └── health_check.py      # Health monitoring
```

### `self.shared_dir` - Runtime Configuration Directory

The **shared directory** is where packages store generated configuration files that are accessible across the pipeline.

- **Location**: Pipeline-specific directory (e.g., `/tmp/jarvis_pipeline_123/shared/`)
- **Purpose**: Store generated configurations, runtime files, and inter-package communication
- **Usage**: Read-write access for generated files
- **Accessibility**: Available to all packages in the pipeline
- **Persistence**: Exists for the duration of the pipeline

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Generate runtime configuration files in shared directory
    self.config_file = f'{self.shared_dir}/database.conf'
    self.log_file = f'{self.shared_dir}/app.log'
    
    # Create configuration with runtime values
    config_content = f"""
    database_port={self.config['port']}
    data_directory={self.config['data_dir']}
    log_file={self.log_file}
    """
    
    with open(self.config_file, 'w') as f:
        f.write(config_content)

def start(self):
    # Use configuration file from shared directory
    cmd = ['my_app', '--config', self.config_file]
    Exec(' '.join(cmd), LocalExecInfo(env=self.mod_env)).run()
```

#### Typical Shared Directory Contents
```
shared/
├── adios2.xml              # Generated ADIOS2 configuration
├── database.conf           # Database configuration
├── hostfile                # MPI hostfile
├── pipeline_env.yaml       # Environment variables
└── app_logs/               # Application logs
    ├── app1.log
    └── app2.log
```

### `self.config_dir` - Package Instance Configuration

The **config directory** is a package-specific directory for storing instance-specific configuration files.

- **Location**: Package-specific directory within the pipeline (e.g., `/tmp/jarvis_pipeline_123/packages/my_package/`)
- **Purpose**: Store package-specific runtime configurations and temporary files
- **Usage**: Read-write access for package-specific files
- **Isolation**: Private to each package instance
- **Cleanup**: Can be cleaned when package is stopped or reset

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Create package-specific configuration
    param_file = f'{self.config_dir}/simulation.param'
    
    # Generate instance-specific parameter file
    with open(param_file, 'w') as f:
        f.write(f"""
        simulation_steps={self.config['steps']}
        output_frequency={self.config['output_freq']}
        mesh_size={self.config['mesh_size']}
        """)
    
    self.param_file = param_file

def start(self):
    # Use package-specific configuration
    cmd = ['simulator', '--params', self.param_file]
    Exec(' '.join(cmd), MpiExecInfo(
        env=self.mod_env,
        hostfile=self.jarvis.hostfile,
        nprocs=self.config['nprocs']
    )).run()
```

### Best Practices for Directory Usage

#### 1. Template Files in pkg_dir
```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Use pkg_dir for accessing template files
    template_xml = f'{self.pkg_dir}/config/adios2_template.xml'
    runtime_xml = f'{self.shared_dir}/adios2.xml'
    
    # Copy and customize template
    self.copy_template_file(template_xml, runtime_xml, 
                           replacements={
                               'ENGINE_TYPE': self.config['engine'],
                               'BUFFER_SIZE': str(self.config['buffer_size'])
                           })
```

#### 2. Runtime Files in shared_dir
```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Store generated files that other packages might need
    self.hostfile_path = f'{self.shared_dir}/mpi_hostfile'
    self.env_file = f'{self.shared_dir}/app_environment.sh'
    
    # Generate hostfile for MPI applications
    with open(self.hostfile_path, 'w') as f:
        for host in self.jarvis.hostfile:
            f.write(f"{host}\n")
```

#### 3. Instance-specific Files in config_dir
```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Create package-specific working directory
    self.work_dir = f'{self.config_dir}/workfiles'
    os.makedirs(self.work_dir, exist_ok=True)
    
    # Package-specific temporary files
    self.temp_input = f'{self.config_dir}/input.tmp'
    self.temp_output = f'{self.config_dir}/output.tmp'
```

#### 4. File Organization Example
```python
class MySimulation(Application):
    """Scientific simulation package"""
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # 1. Access template from package source
        input_template = f'{self.pkg_dir}/config/simulation_input.template'
        
        # 2. Generate shared configuration (accessible to other packages)
        self.shared_config = f'{self.shared_dir}/simulation.xml'
        self.copy_template_file(input_template, self.shared_config, 
                               replacements={'TIME_STEPS': str(self.config['steps'])})
        
        # 3. Create package-specific files
        self.work_dir = f'{self.config_dir}/simulation_work'
        os.makedirs(self.work_dir, exist_ok=True)
        
        # 4. Set environment pointing to configurations
        self.setenv('SIMULATION_CONFIG', self.shared_config)
        self.setenv('SIMULATION_WORK_DIR', self.work_dir)
```

#### 5. Cleanup Considerations
```python
def clean(self):
    """Clean package data"""
    # Clean package-specific files
    if os.path.exists(self.config_dir):
        Rm(self.config_dir, LocalExecInfo()).run()
    
    # Clean shared files this package created
    shared_files = [
        f'{self.shared_dir}/my_app_config.xml',
        f'{self.shared_dir}/my_app.log'
    ]
    for file_path in shared_files:
        if os.path.exists(file_path):
            os.remove(file_path)
```

### Directory Lifecycle

1. **Package Load**: Jarvis sets `pkg_dir`, `shared_dir`, and `config_dir`
2. **Configuration**: Package uses these directories in `_configure()`
3. **Execution**: Applications read from generated configuration files
4. **Cleanup**: Package cleans up generated files in `clean()`

This directory structure enables packages to:
- **Separate concerns**: Templates vs. runtime vs. instance-specific files
- **Share configurations**: Between packages through shared_dir
- **Maintain isolation**: Package-specific files in config_dir
- **Enable reusability**: Template files in pkg_dir can be reused

## Execution System

### Available Execution Classes

```python
from jarvis_cd.shell import Exec, LocalExecInfo, MpiExecInfo, PsshExecInfo, SshExecInfo
from jarvis_cd.shell.process import Kill, Rm, Mkdir, Chmod, Which

# Local execution
Exec('command', LocalExecInfo(env=self.mod_env)).run()

# MPI execution
Exec('mpi_command', MpiExecInfo(
    env=self.mod_env,
    hostfile=self.jarvis.hostfile,
    nprocs=self.config['nprocs'],
    ppn=self.config['ppn']
)).run()

# Parallel SSH execution
Exec('command', PsshExecInfo(
    env=self.mod_env,
    hostfile=self.jarvis.hostfile
)).run()

# Process utilities
Kill('process_name', PsshExecInfo(hostfile=self.jarvis.hostfile)).run()
Rm('/path/to/clean', LocalExecInfo()).run()
```

### Hostfile Access

```python
# Access the hostfile for distributed execution
hostfile = self.jarvis.hostfile

# Use in MPI commands
exec_info = MpiExecInfo(
    hostfile=hostfile,
    nprocs=len(hostfile),  # Number of hosts
    ppn=4  # Processes per node
)
```

## Interceptor Development

Interceptors are specialized packages that modify the execution environment to intercept system calls, library calls, or I/O operations. They are commonly used for profiling, monitoring, debugging, and performance analysis.

### Interceptor Architecture

Interceptors work by:
1. **Library Injection**: Adding shared libraries to `LD_PRELOAD`
2. **Environment Modification**: Setting environment variables for interceptor configuration
3. **Call Interception**: Using library preloading to override system/library functions

### The find_library() Method

The `find_library()` method helps locate shared libraries in the system for interceptor use:

```python
def find_library(self, library_name: str) -> Optional[str]:
    """
    Find a shared library by searching LD_LIBRARY_PATH and system paths.
    
    :param library_name: Name of the library to find
    :return: Path to library if found, None otherwise
    """
```

#### Library Search Order

The method searches for libraries in this order:

1. **Package-specific environment** (`self.mod_env` then `self.env`)
2. **System LD_LIBRARY_PATH** 
3. **Standard system paths**:
   - `/usr/lib`
   - `/usr/local/lib`
   - `/usr/lib64`
   - `/usr/local/lib64`
   - `/lib`
   - `/lib64`

#### Library Name Variations

For a library name like `"profiler"`, it searches for:
- `libprofiler.so` (standard shared library)
- `profiler.so` (as-is with .so extension)
- `libprofiler.a` (static library)
- `profiler` (exact name)

#### Usage Examples

```python
# Find a profiling library
profiler_lib = self.find_library('profiler')
if profiler_lib:
    self.setenv('LD_PRELOAD', profiler_lib)
else:
    raise RuntimeError("Profiler library not found")

# Find MPI profiling library
mpi_profiler = self.find_library('mpiP')
if mpi_profiler:
    current_preload = self.mod_env.get('LD_PRELOAD', '')
    if current_preload:
        self.setenv('LD_PRELOAD', f"{mpi_profiler}:{current_preload}")
    else:
        self.setenv('LD_PRELOAD', mpi_profiler)

# Find multiple interceptor libraries
interceptor_libs = []
for lib_name in ['vtune', 'pin', 'callgrind']:
    lib_path = self.find_library(lib_name)
    if lib_path:
        interceptor_libs.append(lib_path)
        
if interceptor_libs:
    self.setenv('LD_PRELOAD', ':'.join(interceptor_libs))
```

### LD_PRELOAD Management

Interceptors commonly need to manage `LD_PRELOAD` to inject multiple libraries:

```python
def add_to_preload(self, library_path: str):
    """Add a library to LD_PRELOAD safely"""
    current_preload = self.mod_env.get('LD_PRELOAD', '')
    
    # Check if library is already in preload
    if library_path in current_preload.split(':'):
        return
        
    if current_preload:
        new_preload = f"{library_path}:{current_preload}"
    else:
        new_preload = library_path
        
    self.setenv('LD_PRELOAD', new_preload)

def remove_from_preload(self, library_path: str):
    """Remove a library from LD_PRELOAD"""
    current_preload = self.mod_env.get('LD_PRELOAD', '')
    if not current_preload:
        return
        
    libs = current_preload.split(':')
    libs = [lib for lib in libs if lib != library_path]
    
    if libs:
        self.setenv('LD_PRELOAD', ':'.join(libs))
    else:
        # Remove LD_PRELOAD entirely if empty
        if 'LD_PRELOAD' in self.mod_env:
            del self.mod_env['LD_PRELOAD']
```

### Complete Interceptor Examples

#### Performance Profiler Interceptor

```python
from jarvis_cd.basic.pkg import Interceptor
import os

class PerfProfiler(Interceptor):
    """Performance profiling interceptor using custom profiling library"""
    
    def _configure_menu(self):
        return [
            {
                'name': 'profiler_lib',
                'msg': 'Profiler library name or path',
                'type': str,
                'default': 'libprofiler'
            },
            {
                'name': 'output_file',
                'msg': 'Profiler output file',
                'type': str,
                'default': 'profile.out'
            },
            {
                'name': 'sample_rate',
                'msg': 'Profiling sample rate (Hz)',
                'type': int,
                'default': 1000
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # Try to find the profiler library
        profiler_lib = self.find_library(self.config['profiler_lib'])
        if not profiler_lib:
            # Try using the config value as a direct path
            profiler_lib = self.config['profiler_lib']
            if not os.path.exists(profiler_lib):
                raise FileNotFoundError(f"Profiler library not found: {self.config['profiler_lib']}")
        
        self.profiler_path = profiler_lib
        self.log(f"Using profiler library: {self.profiler_path}")
        
        # Set profiler configuration environment
        self.setenv('PROFILER_OUTPUT', self.config['output_file'])
        self.setenv('PROFILER_SAMPLE_RATE', str(self.config['sample_rate']))
    
    def modify_env(self):
        # Add profiler to LD_PRELOAD
        self.add_to_preload(self.profiler_path)
        self.log(f"Added profiler to LD_PRELOAD: {self.profiler_path}")
    
    def clean(self):
        # Remove profiler output files
        if os.path.exists(self.config['output_file']):
            os.remove(self.config['output_file'])
            
    def add_to_preload(self, library_path: str):
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            self.setenv('LD_PRELOAD', f"{library_path}:{current_preload}")
        else:
            self.setenv('LD_PRELOAD', library_path)
```

#### I/O Tracing Interceptor

```python
from jarvis_cd.basic.pkg import Interceptor
import os

class IOTracer(Interceptor):
    """I/O operation tracing interceptor"""
    
    def _configure_menu(self):
        return [
            {
                'name': 'trace_reads',
                'msg': 'Trace read operations',
                'type': bool,
                'default': True
            },
            {
                'name': 'trace_writes', 
                'msg': 'Trace write operations',
                'type': bool,
                'default': True
            },
            {
                'name': 'trace_file',
                'msg': 'I/O trace output file',
                'type': str,
                'default': 'io_trace.log'
            },
            {
                'name': 'min_size',
                'msg': 'Minimum I/O size to trace (bytes)',
                'type': int,
                'default': 1024
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # Find the I/O tracing library
        io_lib = self.find_library('iotrace')
        if not io_lib:
            raise RuntimeError("I/O tracing library (libiotrace.so) not found")
            
        self.iotrace_lib = io_lib
        
        # Set I/O tracer configuration
        trace_ops = []
        if self.config['trace_reads']:
            trace_ops.append('read')
        if self.config['trace_writes']:
            trace_ops.append('write')
            
        self.setenv('IOTRACE_OPERATIONS', ','.join(trace_ops))
        self.setenv('IOTRACE_OUTPUT', self.config['trace_file'])
        self.setenv('IOTRACE_MIN_SIZE', str(self.config['min_size']))
        
    def modify_env(self):
        # Add I/O tracer to LD_PRELOAD
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            self.setenv('LD_PRELOAD', f"{self.iotrace_lib}:{current_preload}")
        else:
            self.setenv('LD_PRELOAD', self.iotrace_lib)
            
        self.log(f"I/O tracing enabled: {self.config['trace_file']}")
    
    def status(self) -> str:
        if 'LD_PRELOAD' in self.mod_env and self.iotrace_lib in self.mod_env['LD_PRELOAD']:
            return "tracing"
        return "inactive"
        
    def clean(self):
        # Remove trace files
        if os.path.exists(self.config['trace_file']):
            os.remove(self.config['trace_file'])
```

#### Memory Debugging Interceptor

```python
from jarvis_cd.basic.pkg import Interceptor

class MemoryDebugger(Interceptor):
    """Memory debugging interceptor using AddressSanitizer or Valgrind"""
    
    def _configure_menu(self):
        return [
            {
                'name': 'tool',
                'msg': 'Memory debugging tool',
                'type': str,
                'choices': ['asan', 'valgrind', 'tcmalloc'],
                'default': 'asan'
            },
            {
                'name': 'output_dir',
                'msg': 'Output directory for debug reports',
                'type': str,
                'default': '/tmp/memdebug'
            },
            {
                'name': 'detect_leaks',
                'msg': 'Enable leak detection',
                'type': bool,
                'default': True
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        tool = self.config['tool']
        
        if tool == 'asan':
            # Find AddressSanitizer library
            asan_lib = self.find_library('asan')
            if not asan_lib:
                raise RuntimeError("AddressSanitizer library not found")
            self.debug_lib = asan_lib
            
        elif tool == 'valgrind':
            # Valgrind doesn't use LD_PRELOAD, just set options
            self.debug_lib = None
            
        elif tool == 'tcmalloc':
            # Find TCMalloc debug library
            tcmalloc_lib = self.find_library('tcmalloc_debug')
            if not tcmalloc_lib:
                raise RuntimeError("TCMalloc debug library not found")
            self.debug_lib = tcmalloc_lib
            
        # Create output directory
        os.makedirs(self.config['output_dir'], exist_ok=True)
        
    def modify_env(self):
        tool = self.config['tool']
        output_dir = self.config['output_dir']
        
        if tool == 'asan':
            # Configure AddressSanitizer
            asan_options = [
                'abort_on_error=1',
                f'log_path={output_dir}/asan',
                'print_stats=1'
            ]
            
            if self.config['detect_leaks']:
                asan_options.append('detect_leaks=1')
                
            self.setenv('ASAN_OPTIONS', ':'.join(asan_options))
            
            # Add ASAN library to LD_PRELOAD
            current_preload = self.mod_env.get('LD_PRELOAD', '')
            if current_preload:
                self.setenv('LD_PRELOAD', f"{self.debug_lib}:{current_preload}")
            else:
                self.setenv('LD_PRELOAD', self.debug_lib)
                
        elif tool == 'valgrind':
            # Valgrind is handled at execution time, not through LD_PRELOAD
            # Set valgrind options for applications that check for them
            valgrind_options = [
                '--tool=memcheck',
                '--leak-check=full',
                f'--log-file={output_dir}/valgrind.log'
            ]
            self.setenv('VALGRIND_OPTS', ' '.join(valgrind_options))
            
        elif tool == 'tcmalloc':
            # Configure TCMalloc
            self.setenv('TCMALLOC_DEBUG', '1')
            self.setenv('TCMALLOC_DEBUG_LOG', f'{output_dir}/tcmalloc.log')
            
            # Add TCMalloc to LD_PRELOAD
            current_preload = self.mod_env.get('LD_PRELOAD', '')
            if current_preload:
                self.setenv('LD_PRELOAD', f"{self.debug_lib}:{current_preload}")
            else:
                self.setenv('LD_PRELOAD', self.debug_lib)
                
        self.log(f"Memory debugging enabled with {tool}")
```

### Interceptor Best Practices

#### 1. Always Check Library Availability

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Always verify library exists before using
    lib_path = self.find_library('myinterceptor')
    if not lib_path:
        raise RuntimeError(f"Required library 'myinterceptor' not found")
    
    self.interceptor_lib = lib_path
```

#### 2. Provide Fallback Options

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Try multiple library names/versions
    for lib_name in ['libprofiler_v2', 'libprofiler', 'profiler']:
        lib_path = self.find_library(lib_name)
        if lib_path:
            self.profiler_lib = lib_path
            break
    else:
        # Fallback to configuration path
        lib_path = self.config.get('library_path')
        if lib_path and os.path.exists(lib_path):
            self.profiler_lib = lib_path
        else:
            raise RuntimeError("No suitable profiler library found")
```

#### 3. Handle Multiple Interceptors

```python
def modify_env(self):
    # Check if other interceptors are already in LD_PRELOAD
    current_preload = self.mod_env.get('LD_PRELOAD', '')
    
    # Don't add if already present
    if self.interceptor_lib not in current_preload.split(':'):
        if current_preload:
            self.setenv('LD_PRELOAD', f"{self.interceptor_lib}:{current_preload}")
        else:
            self.setenv('LD_PRELOAD', self.interceptor_lib)
```

#### 4. Provide Configuration Validation

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Validate configuration
    if self.config['sample_rate'] <= 0:
        raise ValueError("Sample rate must be positive")
        
    if not os.path.exists(os.path.dirname(self.config['output_file'])):
        os.makedirs(os.path.dirname(self.config['output_file']), exist_ok=True)
    
    # Find and validate library
    lib_path = self.find_library(self.config['library_name'])
    if not lib_path:
        raise FileNotFoundError(f"Library not found: {self.config['library_name']}")
    
    self.interceptor_lib = lib_path
```

#### 5. Clean Up Properly

```python
def clean(self):
    # Remove output files
    for pattern in ['*.log', '*.trace', '*.prof']:
        for file_path in glob.glob(os.path.join(self.config['output_dir'], pattern)):
            os.remove(file_path)
    
    # Remove output directory if empty
    try:
        os.rmdir(self.config['output_dir'])
    except OSError:
        pass  # Directory not empty
```

## Implementation Examples

### Simple Application Example

```python
"""
Simple benchmark application package.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_cd.shell import Exec, LocalExecInfo
import os

class SimpleBench(Application):
    """Simple benchmark application"""
    
    def _init(self):
        """Initialize variables"""
        self.output_file = None
    
    def _configure_menu(self):
        """Configuration options"""
        return [
            {
                'name': 'duration',
                'msg': 'Benchmark duration in seconds',
                'type': int,
                'default': 60
            },
            {
                'name': 'output_dir',
                'msg': 'Output directory for results',
                'type': str,
                'default': '/tmp/benchmark'
            }
        ]
    
    def _configure(self, **kwargs):
        """Configure the benchmark"""
        # Configuration automatically updated - no need for self.update_config()
        
        # Set up output directory
        os.makedirs(self.config['output_dir'], exist_ok=True)
        self.output_file = os.path.join(self.config['output_dir'], 'results.txt')
        
        # Set environment variables
        self.setenv('BENCH_OUTPUT_DIR', self.config['output_dir'])
    
    def start(self):
        """Run the benchmark"""
        cmd = [
            'benchmark_tool',
            '--duration', str(self.config['duration']),
            '--output', self.output_file
        ]
        
        Exec(' '.join(cmd), LocalExecInfo(env=self.mod_env)).run()
    
    def clean(self):
        """Clean benchmark output"""
        if self.output_file and os.path.exists(self.output_file):
            os.remove(self.output_file)
```

### MPI Application Example

```python
"""
MPI-based parallel application package.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_cd.shell import Exec, LocalExecInfo, MpiExecInfo
from jarvis_cd.shell.process import Rm
import os

class ParallelApp(Application):
    """Parallel MPI application"""
    
    def _configure_menu(self):
        """Configuration options"""
        return [
            {
                'name': 'nprocs',
                'msg': 'Number of MPI processes',
                'type': int,
                'default': 4
            },
            {
                'name': 'ppn',
                'msg': 'Processes per node',
                'type': int,
                'default': 2
            },
            {
                'name': 'input_file',
                'msg': 'Input data file',
                'type': str,
                'default': 'input.dat'
            }
        ]
    
    def _configure(self, **kwargs):
        """Configure the application"""
        # Configuration automatically updated
        
        # Set MPI environment
        self.setenv('PARALLEL_APP_INPUT', self.config['input_file'])
    
    def start(self):
        """Run parallel application"""
        # Check for MPI executable
        Exec('which mpiexec', LocalExecInfo(env=self.mod_env)).run()
        
        # Run MPI application
        cmd = ['parallel_app', '--input', self.config['input_file']]
        
        Exec(' '.join(cmd), MpiExecInfo(
            env=self.mod_env,
            hostfile=self.jarvis.hostfile,
            nprocs=self.config['nprocs'],
            ppn=self.config['ppn']
        )).run()
    
    def clean(self):
        """Clean output files"""
        Rm('output_*', LocalExecInfo()).run()
```

### Service Example

```python
"""
Database service package.
"""
from jarvis_cd.basic.pkg import Service
from jarvis_cd.shell import Exec, LocalExecInfo
from jarvis_cd.shell.process import Kill, Which
import os
import time

class Database(Service):
    """Database service"""
    
    def _init(self):
        """Initialize service variables"""
        self.pid_file = None
        self.data_dir = None
    
    def _configure_menu(self):
        """Configuration options"""
        return [
            {
                'name': 'port',
                'msg': 'Database port',
                'type': int,
                'default': 5432
            },
            {
                'name': 'data_dir',
                'msg': 'Database data directory',
                'type': str,
                'default': '/var/lib/mydb'
            }
        ]
    
    def _configure(self, **kwargs):
        """Configure database"""
        # Configuration automatically updated
        
        self.data_dir = self.config['data_dir']
        self.pid_file = os.path.join(self.data_dir, 'mydb.pid')
        
        # Set database environment
        self.setenv('MYDB_PORT', str(self.config['port']))
        self.setenv('MYDB_DATA_DIR', self.data_dir)
        
        # Create data directory
        os.makedirs(self.data_dir, exist_ok=True)
    
    def start(self):
        """Start database service"""
        # Check if database is available
        Which('mydb_server', LocalExecInfo()).run()
        
        # Start database
        cmd = [
            'mydb_server',
            '--port', str(self.config['port']),
            '--data-dir', self.data_dir,
            '--pid-file', self.pid_file,
            '--daemonize'
        ]
        
        Exec(' '.join(cmd), LocalExecInfo(env=self.mod_env)).run()
        
        # Wait for service to start
        time.sleep(2)
    
    def stop(self):
        """Stop database service"""
        if os.path.exists(self.pid_file):
            with open(self.pid_file, 'r') as f:
                pid = f.read().strip()
            
            Exec(f'kill {pid}', LocalExecInfo()).run()
    
    def kill(self):
        """Force kill database"""
        Kill('mydb_server', LocalExecInfo()).run()
    
    def status(self) -> str:
        """Check database status"""
        if os.path.exists(self.pid_file):
            return "running"
        return "stopped"
    
    def clean(self):
        """Clean database data"""
        Rm(self.data_dir, LocalExecInfo()).run()
```

### Interceptor Example

```python
"""
Performance profiling interceptor package.
"""
from jarvis_cd.basic.pkg import Interceptor
import os

class Profiler(Interceptor):
    """Performance profiling interceptor"""
    
    def _init(self):
        """Initialize profiler variables"""
        self.profiler_lib = None
    
    def _configure_menu(self):
        """Configuration options"""
        return [
            {
                'name': 'profiler_path',
                'msg': 'Path to profiler library',
                'type': str,
                'default': '/usr/lib/libprofiler.so'
            },
            {
                'name': 'output_file',
                'msg': 'Profiler output file',
                'type': str,
                'default': 'profile_output.txt'
            }
        ]
    
    def _configure(self, **kwargs):
        """Configure profiler"""
        # Configuration automatically updated
        
        self.profiler_lib = self.config['profiler_path']
        
        # Set profiler environment
        self.setenv('PROFILER_OUTPUT', self.config['output_file'])
    
    def modify_env(self):
        """Set up profiling environment"""
        # Add profiler library to LD_PRELOAD
        if os.path.exists(self.profiler_lib):
            current_preload = self.mod_env.get('LD_PRELOAD', '')
            if current_preload:
                self.setenv('LD_PRELOAD', f"{self.profiler_lib}:{current_preload}")
            else:
                self.setenv('LD_PRELOAD', self.profiler_lib)
    
    def clean(self):
        """Clean profiler output"""
        output_file = self.config['output_file']
        if os.path.exists(output_file):
            os.remove(output_file)
```

## Best Practices

### 1. Use Environment Variables Correctly

```python
# ✅ Good - Use in _configure()
def _configure(self, **kwargs):
    # Configuration automatically updated
    self.setenv('MY_APP_HOME', self.config['install_path'])

# ✅ Good - Use mod_env in execution
def start(self):
    Exec('command', LocalExecInfo(env=self.mod_env)).run()

# ❌ Bad - Don't set environment in start()
def start(self):
    self.setenv('LATE_VAR', 'value')  # Too late!
```

### 2. Handle File Paths Properly

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Expand environment variables in paths
    output_dir = os.path.expandvars(self.config['output_dir'])
    
    # Create directories as needed
    os.makedirs(output_dir, exist_ok=True)
    
    # Store absolute paths
    self.output_dir = os.path.abspath(output_dir)
```

### 3. Use Proper Execution Commands

```python
def start(self):
    # ✅ Good - Use .run() method
    Exec('command', LocalExecInfo(env=self.mod_env)).run()
    
    # ✅ Good - Use process utilities
    from jarvis_cd.shell.process import Mkdir
    Mkdir('/path/to/dir', LocalExecInfo()).run()
    
    # ❌ Bad - Don't use subprocess directly
    import subprocess
    subprocess.run(['command'])  # Don't do this
```

### 4. Implement Proper Cleanup

```python
def clean(self):
    """Clean all package data"""
    # Remove output files
    if hasattr(self, 'output_dir') and os.path.exists(self.output_dir):
        Rm(self.output_dir, LocalExecInfo()).run()
    
    # Remove temporary files
    Rm('/tmp/myapp_*', LocalExecInfo()).run()
```

### 5. Error Handling

```python
def start(self):
    """Start with error handling"""
    try:
        # Check prerequisites
        Which('required_command', LocalExecInfo()).run()
        
        # Run main command
        result = Exec('main_command', LocalExecInfo(env=self.mod_env)).run()
        
        # Check result if needed
        if hasattr(result, 'exit_code') and result.exit_code != 0:
            raise RuntimeError("Command failed")
            
    except Exception as e:
        print(f"Error starting {self.__class__.__name__}: {e}")
        raise
```

### 6. Documentation

```python
class MyPackage(Application):
    """
    Brief description of what this package does.
    
    This package provides functionality for...
    """
    
    def _configure_menu(self):
        """
        Define configuration parameters.
        
        For more details on parameter format, see:
        https://docs.jarvis-cd.io/configuration
        """
        return [
            {
                'name': 'param',
                'msg': 'Clear description of parameter purpose',
                'type': str,
                'default': 'sensible_default'
            }
        ]
```

This guide provides the foundation for developing robust Jarvis-CD packages. For more advanced topics, refer to the existing builtin packages in the `builtin/` directory for real-world examples.