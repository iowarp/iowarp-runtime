# Jarvis-CD Package Development Guide

This guide explains how to develop custom packages for Jarvis-CD, including the repository structure, abstract methods, and implementation examples.

## Table of Contents

1. [Repository Structure](#repository-structure)
2. [Pipeline Indexes](#pipeline-indexes)
3. [Package Types](#package-types)
4. [Abstract Methods](#abstract-methods)
5. [Environment Variables](#environment-variables)
6. [Configuration](#configuration)
7. [Package Directory Structure](#package-directory-structure)
8. [Execution System](#execution-system)
9. [Utility Classes](#utility-classes)
10. [Interceptor Development](#interceptor-development)
11. [Implementation Examples](#implementation-examples)
12. [Best Practices](#best-practices)

## Repository Structure

Jarvis-CD packages are organized in repositories with a specific structure that supports both packages and pipeline indexes. **IMPORTANT**: All repositories must include a subdirectory with the same name as the repository to be properly recognized by Jarvis-CD.

### Required Repository Structure

```
my_repo/
├── my_repo/                  # REQUIRED: subdirectory with same name as repo
│   ├── package1/
│   │   ├── __init__.py
│   │   └── pkg.py            # Main package implementation
│   ├── package2/
│   │   ├── __init__.py
│   │   └── pkg.py
│   └── __init__.py
├── pipelines/                # REQUIRED: pipeline index directory
│   ├── basic_workflow.yaml
│   ├── performance_test.yaml
│   ├── examples/
│   │   ├── simple_demo.yaml
│   │   └── advanced_demo.yaml
│   └── io_benchmarks/
│       ├── ior_test.yaml
│       └── fio_test.yaml
└── README.md                 # Optional: repository documentation
```

### Key Requirements

1. **Repository Root**: Contains two main subdirectories: `{repo_name}/` and `pipelines/`
2. **Package Directory**: Must be `{repo_name}/{package_name}/` (e.g., `my_repo/ior/`, `my_repo/redis/`)
3. **Pipeline Index Directory**: Must be `pipelines/` for pipeline script discovery
4. **Main File**: Must be named `pkg.py` (not `package.py`)
5. **Class Name**: Must follow UpperCamelCase naming convention. For single words use capitalized form (e.g., `Ior`, `Redis`). For snake_case package names, convert to UpperCamelCase (e.g., `data_stagein` → `DataStagein`, `redis_benchmark` → `RedisBenchmark`)
6. **Init Files**: Include `__init__.py` files for proper Python module structure

### Package Class Naming Convention

Package class names must follow the UpperCamelCase (PascalCase) naming convention:

| Package Directory | Expected Class Name | Notes |
|-------------------|-------------------|-------|
| `ior` | `Ior` | Single word - capitalize first letter |
| `redis` | `Redis` | Single word - capitalize first letter |
| `data_stagein` | `DataStagein` | Snake_case - convert to UpperCamelCase |
| `redis_benchmark` | `RedisBenchmark` | Snake_case - convert to UpperCamelCase |
| `cosmic_tagger` | `CosmicTagger` | Snake_case - convert to UpperCamelCase |
| `adios2_gray_scott` | `Adios2GrayScott` | Mixed - convert to UpperCamelCase |

**Important**: Package loading will fail with a fatal error if the class name doesn't match this convention, preventing the package from being added to pipelines.

### Adding Repositories

```bash
# Add a repository to Jarvis
jarvis repo add /path/to/my_repo

# List registered repositories
jarvis repo list
```

## Pipeline Indexes

Pipeline indexes allow repositories to provide pre-configured pipeline scripts that users can discover, load, and copy. These scripts demonstrate common workflows, provide testing templates, and serve as examples for package usage.

### Pipeline Index Structure

The `pipelines/` directory in your repository serves as the pipeline index. It can contain:

- **YAML Files**: Pipeline scripts that can be loaded directly
- **Subdirectories**: Organized collections of related pipeline scripts
- **Nested Structure**: Multiple levels of organization

```
pipelines/
├── basic_workflow.yaml           # Simple pipeline script
├── performance_test.yaml         # Performance testing pipeline
├── examples/                     # Example pipelines subdirectory
│   ├── simple_demo.yaml
│   ├── advanced_demo.yaml
│   └── multi_node_example.yaml
├── benchmarks/                   # Benchmark pipelines subdirectory
│   ├── io_tests/
│   │   ├── ior_benchmark.yaml
│   │   └── fio_benchmark.yaml
│   └── compute_tests/
│       ├── hpl_benchmark.yaml
│       └── stream_benchmark.yaml
└── integration_tests/            # Integration test pipelines
    ├── full_stack_test.yaml
    └── component_test.yaml
```

### Pipeline Index Commands

Users can interact with pipeline indexes using the following commands:

#### List Available Pipeline Scripts

```bash
# List all pipeline scripts from all repositories
jarvis ppl index list

# List pipeline scripts from a specific repository
jarvis ppl index list my_repo
```

The output shows both files and directories with color coding:
- **Files**: Default color - these are loadable pipeline scripts
- **Directories**: Cyan color with "(directory)" label - these contain more scripts

#### Load Pipeline Script from Index

```bash
# Load a pipeline script directly into the current workspace
jarvis ppl index load my_repo.examples.simple_demo

# Load from nested directory structure
jarvis ppl index load my_repo.benchmarks.io_tests.ior_benchmark
```

#### Copy Pipeline Script from Index

```bash
# Copy pipeline script to current directory
jarvis ppl index copy my_repo.examples.simple_demo

# Copy to specific location
jarvis ppl index copy my_repo.examples.simple_demo /path/to/output/

# Copy to specific filename
jarvis ppl index copy my_repo.examples.simple_demo ./my_custom_pipeline.yaml
```

### Creating Pipeline Scripts for Your Repository

When developing packages, include example pipeline scripts that demonstrate:

1. **Basic Usage**: Simple pipeline showing package basics
2. **Advanced Configuration**: Pipeline with comprehensive configuration options
3. **Integration Examples**: Pipelines showing how your packages work with others
4. **Performance Testing**: Pipelines for benchmarking and validation
5. **Development/Testing**: Pipelines for package development and debugging

#### Example Pipeline Script

```yaml
# pipelines/examples/basic_usage.yaml
name: basic_usage_example
env:
  # Optional: define environment for this pipeline
  EXAMPLE_VAR: "value"

pkgs:
  - pkg_type: my_repo.my_package
    pkg_name: main_app
    # Package configuration
    input_file: "test_input.dat"
    output_dir: "/tmp/output"
    threads: 4

interceptors:
  # Optional: interceptors for monitoring/profiling
  - pkg_type: builtin.profiler
    pkg_name: perf_monitor
    sampling_rate: 1000
    output_file: "/tmp/profile.out"
```

### Pipeline Index Best Practices

#### 1. Organize by Purpose

```
pipelines/
├── examples/          # Basic usage examples
├── benchmarks/        # Performance testing
├── tutorials/         # Step-by-step learning
├── validation/        # Package validation tests
└── integration/       # Multi-package workflows
```

#### 2. Use Descriptive Names

```
# ✅ Good names
ior_single_node_test.yaml
multi_node_mpi_benchmark.yaml
storage_performance_analysis.yaml

# ❌ Poor names
test.yaml
example.yaml
config.yaml
```

#### 3. Include Documentation Comments

```yaml
# Pipeline: I/O Performance Benchmark
# Purpose: Measures I/O performance using IOR with different block sizes
# Requirements: MPI environment, shared filesystem
# Expected Runtime: 10-15 minutes
name: io_performance_benchmark

# Environment setup for consistent testing
env:
  IOR_HINT: "posix"
  TEST_DIR: "/shared/benchmark"

pkgs:
  - pkg_type: my_repo.ior
    pkg_name: ior_test
    # Test with 1GB files using 4 processes
    nprocs: 4
    block: "1G"
    transfer: "64K"
    test_file: "${TEST_DIR}/ior_test_file"
```

#### 4. Provide Multiple Complexity Levels

```
pipelines/
├── simple_demo.yaml           # Minimal configuration
├── intermediate_demo.yaml     # Common options configured
└── advanced_demo.yaml         # Full configuration showcase
```

#### 5. Include Validation Pipelines

```yaml
# pipelines/validation/package_test.yaml
# Validation pipeline to ensure package works correctly
name: package_validation
pkgs:
  - pkg_type: my_repo.my_package
    pkg_name: validation_test
    # Minimal configuration for basic functionality test
    mode: "validation"
    quick_test: true
    expected_output: "test_passed"
```

### Repository Integration

When users add your repository with `jarvis repo add`, both the packages and pipeline indexes become available:

```bash
# Add repository (exposes both packages and pipeline indexes)
jarvis repo add /path/to/my_repo

# Discover packages
jarvis ppl append my_repo.package_name

# Discover pipeline scripts
jarvis ppl index list my_repo
jarvis ppl index load my_repo.examples.basic_usage
```

This integration provides a complete development ecosystem where users can:
1. **Discover**: Find available packages and example pipelines
2. **Learn**: Use example pipelines to understand package capabilities
3. **Develop**: Copy and modify pipeline scripts for their own use
4. **Validate**: Use provided test pipelines to verify functionality

## Package Types

Jarvis-CD provides several base classes for different types of packages:

### 1. SimplePackage (jarvis_cd.basic.pkg.SimplePackage)

**Most common base class** - Use this for packages that need interceptor support. Most builtin packages inherit from this.

```python
from jarvis_cd.core.pkg import SimplePackage

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
from jarvis_cd.core.pkg import Application

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
from jarvis_cd.core.pkg import Service

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

### 4. Interceptor (jarvis_cd.core.pkg.Interceptor)

For packages that modify environment variables to intercept system calls (e.g., profiling tools, I/O interceptors). Interceptors work by modifying `LD_PRELOAD` and other environment variables to inject custom libraries into target applications.

**Key Method: `modify_env()`**
Interceptors must implement the `modify_env()` method, which is automatically called by Jarvis to modify the environment before other packages run. This method should use `setenv()` and `prepend_env()` to modify environment variables, particularly `LD_PRELOAD`.

```python
from jarvis_cd.core.pkg import Interceptor
import os

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
            },
            {
                'name': 'enable_tracing',
                'msg': 'Enable detailed tracing',
                'type': bool,
                'default': False
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # Find the interceptor library using the built-in find_library method
        lib_path = self.find_library('interceptor')
        if not lib_path:
            lib_path = self.config['library_path']
            
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Interceptor library not found: {lib_path}")
            
        self.interceptor_lib = lib_path
        self.log(f"Found interceptor library: {lib_path}")
    
    def modify_env(self):
        """
        Modify environment for interception - called automatically by Jarvis.
        
        This method is where interceptors set up LD_PRELOAD and other environment
        variables needed for interception to work.
        """
        # Add interceptor library to LD_PRELOAD
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            new_preload = f"{self.interceptor_lib}:{current_preload}"
        else:
            new_preload = self.interceptor_lib
            
        self.setenv('LD_PRELOAD', new_preload)
        
        # Set interceptor configuration environment variables
        if self.config['enable_tracing']:
            self.setenv('INTERCEPTOR_TRACE', '1')
            self.setenv('INTERCEPTOR_TRACE_FILE', f'{self.shared_dir}/trace.log')
        
        self.log(f"Interceptor environment configured with LD_PRELOAD: {new_preload}")
```

**Important Notes:**
- Interceptors use `modify_env()` method, not `start()` 
- `modify_env()` is called automatically during pipeline start (runtime), not configuration
- Use `setenv()` and `prepend_env()` methods to modify environment variables
- Interceptors share the same `mod_env` reference with the target package
- LD_PRELOAD modifications directly affect the package's execution environment

### Pipeline-Level Interceptor Architecture - NEW SYSTEM

#### Pipeline YAML Structure

Interceptors are now defined at the pipeline level in a separate `interceptors` section:

```yaml
name: my_pipeline
pkgs:
  - pkg_type: example_app
    pkg_name: my_app
    interceptors: ["profiler", "tracer"]  # References to pipeline interceptors
interceptors:
  - pkg_type: performance_profiler
    pkg_name: profiler
    sampling_rate: 1000
    output_file: /tmp/profile.out
  - pkg_type: io_tracer  
    pkg_name: tracer
    trace_reads: true
    trace_writes: true
```

#### Key Architecture Changes

1. **Pipeline-Level Definition**: Interceptors are defined once in the `interceptors` section
2. **Package References**: Packages reference interceptors by name in their `interceptors` list
3. **Runtime Application**: Interceptors are applied during `pipeline.start()`, not configuration
4. **Shared Environment**: Interceptors and packages share the exact same `mod_env` object
5. **Unique IDs**: Interceptor IDs must be unique from package IDs within the pipeline

#### Interceptor Lifecycle

```
Pipeline Start → For Each Package → Apply Referenced Interceptors → Run Package
                                  ↓
                           Load Interceptor Instance
                                  ↓
                           Share mod_env Reference
                                  ↓
                           Call interceptor.modify_env()
                                  ↓  
                           Package starts with modified environment
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

### Debugging with GdbServer

The `GdbServer` class enables remote debugging by launching applications under gdbserver. This is particularly useful for debugging MPI applications or applications running on remote nodes.

#### GdbServer Usage

```python
from jarvis_cd.shell.process import GdbServer

# Launch application under gdbserver
GdbServer(cmd='./my_app --args', port=2345, exec_info=LocalExecInfo()).run()

# With MPI
GdbServer(
    cmd='my_mpi_app',
    port=2345,
    exec_info=MpiExecInfo(
        env=self.mod_env,
        hostfile=self.jarvis.hostfile,
        nprocs=self.config['nprocs']
    )
).run()
```

#### Conditional Debugging Pattern

Most packages should support optional debugging using a `do_dbg` configuration flag:

```python
class MyApp(Application):
    def _configure_menu(self):
        return [
            {
                'name': 'do_dbg',
                'msg': 'Enable remote debugging with gdbserver',
                'type': bool,
                'default': False
            },
            {
                'name': 'dbg_port',
                'msg': 'GDB server port',
                'type': int,
                'default': 2345
            }
        ]

    def start(self):
        # Build command
        cmd = f'lmp -in {self.input_path}'

        # Execute with or without debugging
        if self.config['do_dbg']:
            GdbServer(cmd, self.config['dbg_port'], LocalExecInfo(env=self.mod_env)).run()
        else:
            Exec(cmd, LocalExecInfo(env=self.mod_env)).run()
```

#### Connecting to GdbServer

Once the application is running under gdbserver, connect from your local machine:

```bash
# Connect to gdbserver
gdb ./my_app
(gdb) target remote hostname:2345
(gdb) continue
```

#### Multi-Process Debugging

For MPI applications, each process can run on a different port:

```python
def start(self):
    cmd = 'my_mpi_app'

    if self.config['do_dbg']:
        # Calculate unique port per process
        base_port = self.config['dbg_port']

        # Launch with gdbserver (MPI will spawn multiple instances)
        GdbServer(
            cmd,
            base_port,  # MPI processes will use base_port, base_port+1, etc.
            MpiExecInfo(
                env=self.mod_env,
                hostfile=self.jarvis.hostfile,
                nprocs=self.config['nprocs']
            )
        ).run()
    else:
        Exec(cmd, MpiExecInfo(
            env=self.mod_env,
            hostfile=self.jarvis.hostfile,
            nprocs=self.config['nprocs']
        )).run()
```

## Utility Classes

Jarvis-CD provides several utility classes to help with common tasks in package development:

### SizeType - Size String Conversion

The `SizeType` class converts size strings (like "1k", "2M", "10G") to integer byte values using binary multipliers (powers of 2).

#### Supported Multipliers

- **k/K**: 1024 (1 << 10) - Kilobytes
- **m/M**: 1048576 (1 << 20) - Megabytes  
- **g/G**: 1073741824 (1 << 30) - Gigabytes
- **t/T**: 1099511627776 (1 << 40) - Terabytes

#### Basic Usage

SizeType works bidirectionally - it can convert from human-readable strings to bytes, or from bytes to human-readable strings:

```python
from jarvis_cd.util import SizeType

# String -> Bytes (parsing user input)
buffer_size = SizeType("1M")        # 1048576 bytes
cache_size = SizeType("512k")       # 524288 bytes
storage_limit = SizeType("10G")     # 10737418240 bytes

# Bytes -> Human Readable (formatting numeric values)
exact_size = SizeType(1048576)      # "1M" when displayed
partial_size = SizeType(1536)       # "1.5K" when displayed  
float_size = SizeType(2048.5)       # "2K" when displayed (rounded)

# Round-trip conversion works perfectly
original = SizeType("1.5M")
bytes_val = original.bytes          # 1572864
reconstructed = SizeType(bytes_val) # Back to "1.5M"
assert str(original) == str(reconstructed)

# Convert to integer bytes (multiple ways)
bytes_value = int(buffer_size)      # 1048576 - using int() conversion
bytes_value = buffer_size.bytes     # 1048576 - using .bytes property  
bytes_value = buffer_size.to_bytes() # 1048576 - using .to_bytes() method

# Use in configuration
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Parse buffer size from config
    buffer_size = SizeType(self.config['buffer_size'])
    self.setenv('BUFFER_SIZE', str(buffer_size.bytes))
    
    # Set memory limits  
    mem_limit = SizeType(self.config.get('memory_limit', '1G'))
    if mem_limit.gigabytes > 8:
        print(f"Warning: Large memory limit: {mem_limit.to_human_readable()}")
```

#### Configuration Integration

Use SizeType in `_configure_menu()` for size-based parameters:

```python
def _configure_menu(self):
    return [
        {
            'name': 'buffer_size',
            'msg': 'Buffer size (e.g., 1M, 512K, 2G)',
            'type': str,
            'default': '1M'
        },
        {
            'name': 'cache_size', 
            'msg': 'Cache size (e.g., 100M, 1G)',
            'type': str,
            'default': '100M'
        },
        {
            'name': 'max_file_size',
            'msg': 'Maximum file size (e.g., 10G, 1T)',
            'type': str,
            'default': '10G'
        }
    ]

def _configure(self, **kwargs):
    # Configuration automatically updated
    
    # Convert size strings to bytes for application use
    buffer_bytes = SizeType(self.config['buffer_size']).bytes
    cache_bytes = SizeType(self.config['cache_size']).bytes
    max_file_bytes = SizeType(self.config['max_file_size']).bytes
    
    # Set environment variables as bytes
    self.setenv('BUFFER_SIZE', str(buffer_bytes))
    self.setenv('CACHE_SIZE', str(cache_bytes))
    self.setenv('MAX_FILE_SIZE', str(max_file_bytes))
    
    # Generate configuration file with byte values
    config_content = f"""
    buffer_size={buffer_bytes}
    cache_size={cache_bytes}
    max_file_size={max_file_bytes}
    """
    
    with open(f'{self.shared_dir}/app_config.conf', 'w') as f:
        f.write(config_content)
```

#### Getting Integer Bytes

There are multiple ways to get the size as integer bytes:

```python
size = SizeType("1M")

# Method 1: int() conversion (most common)
bytes_int = int(size)                    # 1048576

# Method 2: .bytes property  
bytes_int = size.bytes                   # 1048576

# Method 3: .to_bytes() method (explicit)
bytes_int = size.to_bytes()              # 1048576

# All return the same integer value
assert int(size) == size.bytes == size.to_bytes()

# Use in environment variables (strings)
self.setenv('BUFFER_SIZE', str(size.bytes))
self.setenv('CACHE_SIZE', str(int(size)))
```

#### Properties and Conversion

```python
size = SizeType("2G")

# Access different units
print(f"Bytes: {size.bytes}")           # 2147483648
print(f"KB: {size.kilobytes}")          # 2097152.0
print(f"MB: {size.megabytes}")          # 2048.0  
print(f"GB: {size.gigabytes}")          # 2.0
print(f"TB: {size.terabytes}")          # 0.001953125

# Human-readable format
print(f"Human: {size.to_human_readable()}")  # "2G"
print(f"String: {str(size)}")                # "2G"
```

#### Arithmetic Operations

```python
# Arithmetic with other SizeType instances
total_memory = SizeType("1G") + SizeType("512M")  # 1.5G
remaining = SizeType("2G") - SizeType("500M")     # 1.5G

# Arithmetic with numbers
doubled = SizeType("1G") * 2                      # 2G
half = SizeType("1G") / 2                         # 512M

# Comparisons
if SizeType("1G") > SizeType("500M"):
    print("1G is larger than 500M")

# Use in sorting
sizes = [SizeType("1M"), SizeType("1G"), SizeType("100K")]
sorted_sizes = sorted(sizes)  # [100K, 1M, 1G]
```

#### Class Methods

```python
# Create from different units
size1 = SizeType.from_bytes(1048576)      # 1M
size2 = SizeType.from_kilobytes(1024)     # 1M
size3 = SizeType.from_megabytes(1)        # 1M
size4 = SizeType.from_gigabytes(1)        # 1G

# Parse method (same as constructor)
size = SizeType.parse("1G")               # Same as SizeType("1G")

# Create from integer bytes and display human-readable
memory_usage = SizeType.from_bytes(67108864)  # 64M
print(f"Memory usage: {memory_usage}")         # "64M"
```

#### Bidirectional Usage Example

```python
class MemoryMonitor(Application):
    def _configure_menu(self):
        return [
            {
                'name': 'max_memory',
                'msg': 'Maximum memory usage (e.g., 1G, 512M)',
                'type': str,
                'default': '1G'
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        # Parse user-provided limit (string -> bytes)
        self.max_memory = SizeType(self.config['max_memory'])
        self.setenv('MAX_MEMORY_BYTES', str(self.max_memory.bytes))
        
    def monitor_memory(self):
        # Get current memory usage in bytes from system
        current_bytes = self.get_memory_usage()  # Returns integer bytes
        
        # Convert bytes to human-readable for display (bytes -> string)
        current_readable = SizeType(current_bytes)
        
        print(f"Memory usage: {current_readable} / {self.max_memory}")
        
        # Compare with limit
        if current_bytes > self.max_memory.bytes:
            print(f"Warning: Exceeded memory limit!")
            
        return current_readable
```

#### Convenience Functions

```python
from jarvis_cd.util import size_to_bytes, human_readable_size

# Quick conversion to integer bytes (no SizeType object needed)
bytes_val = size_to_bytes("1M")           # 1048576 (integer)
bytes_val = size_to_bytes("512K")         # 524288 (integer)
bytes_val = size_to_bytes("2G")           # 2147483648 (integer)

# Quick human-readable formatting
readable = human_readable_size(1048576)   # "1M"
readable = human_readable_size(2147483648) # "2G"

# Use in configuration parsing
def parse_config_size(config_value):
    return size_to_bytes(config_value)  # Direct integer bytes
```

#### Input Validation and Error Handling

```python
def _configure(self, **kwargs):
    # Configuration automatically updated
    
    try:
        buffer_size = SizeType(self.config['buffer_size'])
        
        # Validate reasonable limits
        if buffer_size.bytes < 1024:  # Less than 1K
            raise ValueError("Buffer size too small (minimum 1K)")
        elif buffer_size.gigabytes > 100:  # More than 100G
            raise ValueError("Buffer size too large (maximum 100G)")
            
        self.buffer_bytes = buffer_size.bytes
        
    except ValueError as e:
        raise ValueError(f"Invalid buffer_size '{self.config['buffer_size']}': {e}")
```

#### Real-World Usage Examples

##### Memory-Intensive Application

```python
class BigDataProcessor(Application):
    def _configure_menu(self):
        return [
            {
                'name': 'chunk_size',
                'msg': 'Data chunk size for processing (e.g., 64M, 1G)',
                'type': str,
                'default': '64M'
            },
            {
                'name': 'memory_limit',
                'msg': 'Maximum memory usage (e.g., 4G, 16G)',
                'type': str,
                'default': '4G'
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        chunk_size = SizeType(self.config['chunk_size'])
        memory_limit = SizeType(self.config['memory_limit'])
        
        # Calculate number of chunks that fit in memory
        max_chunks = int(memory_limit.bytes / chunk_size.bytes)
        
        # Set application parameters
        self.setenv('CHUNK_SIZE_BYTES', str(chunk_size.bytes))
        self.setenv('MAX_CHUNKS', str(max_chunks))
        self.setenv('MEMORY_LIMIT_BYTES', str(memory_limit.bytes))
        
        print(f"Processing with {chunk_size.to_human_readable()} chunks")
        print(f"Memory limit: {memory_limit.to_human_readable()}")
        print(f"Max concurrent chunks: {max_chunks}")
```

##### Storage Configuration

```python
class DatabaseApp(Service):
    def _configure_menu(self):
        return [
            {
                'name': 'cache_size',
                'msg': 'Database cache size (e.g., 512M, 2G)',
                'type': str,
                'default': '512M'
            },
            {
                'name': 'log_file_size',
                'msg': 'Maximum log file size (e.g., 100M, 1G)',
                'type': str,
                'default': '100M'
            },
            {
                'name': 'data_threshold',
                'msg': 'Archive threshold (e.g., 10G, 100G)',
                'type': str,
                'default': '10G'
            }
        ]
    
    def _configure(self, **kwargs):
        # Configuration automatically updated
        
        cache_size = SizeType(self.config['cache_size'])
        log_file_size = SizeType(self.config['log_file_size'])
        data_threshold = SizeType(self.config['data_threshold'])
        
        # Generate database configuration
        db_config = f"""
        [memory]
        cache_size = {cache_size.bytes}
        
        [logging]
        max_log_file_size = {log_file_size.bytes}
        
        [storage]
        archive_threshold = {data_threshold.bytes}
        """
        
        with open(f'{self.shared_dir}/database.conf', 'w') as f:
            f.write(db_config)
```

#### Best Practices

1. **Always validate sizes** in configuration methods
2. **Use human-readable defaults** in `_configure_menu()` 
3. **Convert to bytes early** in the configuration process
4. **Provide reasonable limits** and error messages
5. **Use properties** for different unit access
6. **Document expected formats** in parameter descriptions

The SizeType class makes it easy to handle size specifications in a user-friendly way while ensuring consistent binary calculations throughout your packages.

### Package Utility Methods

All package classes inherit several utility methods from the base `Pkg` class that provide common functionality for logging, timing, and file processing.

#### log() - Colored Logging

The `log()` method provides colored console output with package context for debugging and status messages.

```python
def log(self, message, color=None):
    """
    Log a message with package context and optional color.
    
    :param message: Message to log
    :param color: Color to use (from jarvis_cd.util.logger.Color enum), defaults to package color
    """
```

##### Usage Examples

```python
from jarvis_cd.util.logger import Color

class MyPackage(Application):
    def start(self):
        # Default package color (light green)
        self.log("Starting application")
        
        # Custom colors for different message types
        self.log("Configuration loaded successfully", Color.GREEN)
        self.log("Warning: Using default settings", Color.YELLOW)
        self.log("Error: Failed to connect", Color.RED)
        self.log("Debug information", Color.LIGHT_BLACK)
        
        # Available colors include:
        # Color.RED, Color.GREEN, Color.YELLOW, Color.BLUE
        # Color.MAGENTA, Color.CYAN, Color.WHITE
        # Color.LIGHT_RED, Color.LIGHT_GREEN, etc.
```

##### Output Format

Messages are automatically formatted with the package class name:
```
[MyPackage] Starting application
[MyPackage] Configuration loaded successfully
```

#### sleep() - Configurable Delays

The `sleep()` method provides configurable delays with logging, useful for testing, synchronization, or rate limiting.

```python
def sleep(self, time_sec=None):
    """
    Sleep for a specified amount of time.
    
    :param time_sec: Time to sleep in seconds. If not provided, uses self.config['sleep']
    """
```

##### Usage Examples

```python
class MyPackage(Application):
    def _configure_menu(self):
        return [
            {
                'name': 'startup_delay',
                'msg': 'Delay before starting (seconds)',
                'type': int,
                'default': 5
            }
        ]
    
    def start(self):
        # Use explicit delay
        self.log("Waiting 3 seconds before startup")
        self.sleep(3)
        
        # Use configured delay (from self.config['sleep'])
        self.sleep()  # Uses default 'sleep' parameter from common menu
        
        # Use custom configuration parameter
        delay = self.config.get('startup_delay', 0)
        if delay > 0:
            self.log(f"Startup delay: {delay} seconds")
            self.sleep(delay)
```

##### Configuration Integration

The `sleep` parameter is automatically available in all package configuration menus:

```bash
# Configure sleep time
jarvis pkg conf mypackage sleep=10

# The package can then use self.sleep() to sleep for 10 seconds
```

#### copy_template_file() - Template Processing

The `copy_template_file()` method copies files while replacing template constants, useful for generating configuration files from templates.

```python
def copy_template_file(self, source_path, dest_path, replacements=None):
    """
    Copy a template file from source to destination, replacing template constants.
    
    Template constants have the format ##CONSTANT_NAME## and are replaced with
    values from the replacements dictionary.
    
    :param source_path: Path to the source template file
    :param dest_path: Path where the processed file should be saved
    :param replacements: Dictionary of replacements {CONSTANT_NAME: value}
    """
```

##### Template Format

Template constants use the format `##CONSTANT_NAME##`:

```xml
<!-- Template file: config/server.xml -->
<server>
    <hostname>##HOSTNAME##</hostname>
    <port>##PORT##</port>
    <threads>##THREAD_COUNT##</threads>
    <memory>##MEMORY_LIMIT##</memory>
</server>
```

##### Usage Examples

```python
class MyPackage(Service):
    def _configure_menu(self):
        return [
            {
                'name': 'hostname',
                'msg': 'Server hostname',
                'type': str,
                'default': 'localhost'
            },
            {
                'name': 'port',
                'msg': 'Server port',
                'type': int,
                'default': 8080
            },
            {
                'name': 'threads',
                'msg': 'Number of worker threads',
                'type': int,
                'default': 4
            }
        ]
    
    def _configure(self, **kwargs):
        # Generate configuration file from template
        config_file = f"{self.shared_dir}/server.xml"
        
        self.copy_template_file(
            source_path=f"{self.pkg_dir}/config/server.xml.template",
            dest_path=config_file,
            replacements={
                'HOSTNAME': self.config['hostname'],
                'PORT': self.config['port'],
                'THREAD_COUNT': self.config['threads'],
                'MEMORY_LIMIT': '2G'
            }
        )
        
        self.log(f"Generated configuration: {config_file}")
```

##### Result

After processing, the template becomes:

```xml
<!-- Generated file: shared_dir/server.xml -->
<server>
    <hostname>localhost</hostname>
    <port>8080</port>
    <threads>4</threads>
    <memory>2G</memory>
</server>
```

##### Advanced Usage

```python
def _configure(self, **kwargs):
    # Use pkg_dir for template source directory
    template_dir = f"{self.pkg_dir}/templates"
    output_dir = self.shared_dir
    
    # Common replacements for multiple files
    common_vars = {
        'USER': os.environ.get('USER', 'unknown'),
        'HOSTNAME': socket.gethostname(),
        'TIMESTAMP': datetime.now().isoformat(),
        'PID': os.getpid()
    }
    
    # Process multiple template files
    templates = [
        ('config.xml.template', 'config.xml'),
        ('startup.sh.template', 'startup.sh'),
        ('logging.conf.template', 'logging.conf')
    ]
    
    for template_name, output_name in templates:
        self.copy_template_file(
            source_path=f"{template_dir}/{template_name}",
            dest_path=f"{output_dir}/{output_name}",
            replacements={
                **common_vars,  # Include common variables
                'SERVICE_NAME': self.config['service_name'],
                'LOG_LEVEL': self.config.get('log_level', 'INFO')
            }
        )
```

##### Error Handling

The method automatically:
- Creates destination directories if they don't exist
- Provides clear error messages for missing template files
- Logs successful operations with replacement counts
- Raises exceptions for template or I/O errors

## Interceptor Development

Interceptors are specialized packages that modify the execution environment to intercept system calls, library calls, or I/O operations. They are commonly used for profiling, monitoring, debugging, and performance analysis.

### Interceptor Architecture

Interceptors work by:
1. **Library Injection**: Adding shared libraries to `LD_PRELOAD`
2. **Environment Modification**: Setting environment variables for interceptor configuration
3. **Call Interception**: Using library preloading to override system/library functions

### The modify_env() Method - Core Interceptor Interface

**All interceptors must implement the `modify_env()` method.** This is the primary interface that Jarvis uses to apply interceptor functionality to other packages in the pipeline.

#### How modify_env() Works - NEW ARCHITECTURE

1. **Called at Runtime**: Jarvis automatically calls `modify_env()` during `pipeline.start()`, just before each package's `start()` method
2. **Shared Environment**: Interceptors and packages share the same `mod_env` reference (same pointer)
3. **LD_PRELOAD Management**: Most interceptors add libraries to `LD_PRELOAD` to inject interception code
4. **Configuration Setup**: The method can set environment variables that configure the interceptor's behavior
5. **Per-Package Application**: Each interceptor is applied only to packages that reference it in their `interceptors` list

#### modify_env() vs start()

- **`modify_env()`**: Used by interceptors to modify the environment. Called during pipeline start, per package.
- **`start()`**: Used by applications and services to start running. Not typically used by interceptors.

```python
class MyInterceptor(Interceptor):
    def modify_env(self):
        """
        Core interceptor method - modifies shared environment for interception.
        Called automatically during pipeline start, just before package starts.
        
        IMPORTANT: self.mod_env is the SAME OBJECT as the target package's mod_env.
        Any changes made here directly affect the package's execution environment.
        """
        # Add interceptor library to LD_PRELOAD (shared with package)
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            self.setenv('LD_PRELOAD', f"{self.interceptor_lib}:{current_preload}")
        else:
            self.setenv('LD_PRELOAD', self.interceptor_lib)
        
        # Set interceptor configuration (shared with package)
        self.setenv('INTERCEPTOR_CONFIG_FILE', f'{self.shared_dir}/interceptor.conf')
        
        # Changes are immediately visible to the package since mod_env is shared
```

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
from jarvis_cd.core.pkg import Interceptor
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
        """Modify environment for profiling interception"""
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
from jarvis_cd.core.pkg import Interceptor
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
        """Modify environment for I/O tracing interception"""
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
from jarvis_cd.core.pkg import Interceptor

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
        """Modify environment for memory debugging interception"""
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

#### 1. Always Implement modify_env() Method

```python
class MyInterceptor(Interceptor):
    def modify_env(self):
        """
        Required method for all interceptors - called during pipeline start.
        Environment modifications are applied to shared mod_env with target package.
        """
        # Add libraries to LD_PRELOAD (shared environment)
        current_preload = self.mod_env.get('LD_PRELOAD', '')
        if current_preload:
            self.setenv('LD_PRELOAD', f"{self.interceptor_lib}:{current_preload}")
        else:
            self.setenv('LD_PRELOAD', self.interceptor_lib)
        
        # Set interceptor configuration environment variables (shared)
        self.setenv('INTERCEPTOR_CONFIG', self.config['config_file'])
        
        # Log what was configured
        self.log(f"Interceptor applied to package with shared mod_env")
```

#### 2. Always Check Library Availability

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
from jarvis_cd.core.pkg import Application
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
from jarvis_cd.core.pkg import Application
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
from jarvis_cd.core.pkg import Service
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
from jarvis_cd.core.pkg import Interceptor
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
        """Modify environment for profiling interception"""
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

### 1. CRITICAL: Always Call .run() on Exec Objects

**MOST IMPORTANT**: All Exec objects and process utilities must call `.run()` to actually execute commands. Simply creating an Exec object does not execute anything.

```python
# ✅ Correct - Execute the command
Exec('my_command', LocalExecInfo()).run()

# ❌ Wrong - Command is never executed
Exec('my_command', LocalExecInfo())  # Does nothing!

# ✅ Correct - Store executor and run
executor = Exec('my_command', LocalExecInfo())
executor.run()

# ✅ Correct - Process utilities also need .run()
from jarvis_cd.shell.process import Mkdir, Rm, Which
Mkdir('/output/dir', LocalExecInfo()).run()
Rm('/tmp/files*', LocalExecInfo()).run()
Which('required_tool', LocalExecInfo()).run()

# ❌ Wrong - These commands are never executed
Mkdir('/output/dir', LocalExecInfo())  # Directory not created!
Rm('/tmp/files*', LocalExecInfo())     # Files not removed!
```

This is the most common mistake in package development and will cause your package to appear to work but actually do nothing.

### 2. Use Environment Variables Correctly

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

### 4. Use Proper Execution Commands

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

### 5. Implement Proper Cleanup

```python
def clean(self):
    """Clean all package data"""
    # Remove output files
    if hasattr(self, 'output_dir') and os.path.exists(self.output_dir):
        Rm(self.output_dir, LocalExecInfo()).run()
    
    # Remove temporary files
    Rm('/tmp/myapp_*', LocalExecInfo()).run()
```

### 6. Error Handling

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

### 7. Documentation

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

## Working with the New Interceptor Architecture

### Pipeline Commands for Interceptors

```bash
# Load a pipeline with interceptors
jarvis ppl load yaml my_pipeline.yaml

# View pipeline configuration (shows both packages and interceptors)
jarvis ppl print

# Start pipeline (interceptors are applied at runtime)
jarvis ppl start

# Check pipeline status
jarvis ppl status
```

### Example Pipeline with Interceptors

```yaml
# my_pipeline.yaml
name: performance_testing
pkgs:
  - pkg_type: builtin.ior
    pkg_name: benchmark
    interceptors: ["profiler", "tracer"]  # Apply both interceptors
    nprocs: 4
    block: "1G"
interceptors:
  - pkg_type: builtin.perf_profiler
    pkg_name: profiler
    sampling_rate: 1000
    output_file: /tmp/perf.out
  - pkg_type: builtin.io_tracer
    pkg_name: tracer
    trace_reads: true
    trace_writes: true
    min_size: 1024
```

### Pipeline Output Example

When you run `jarvis ppl print`, you'll see:

```
Pipeline: performance_testing
Directory: /home/user/.ppi-jarvis/config/pipelines/performance_testing
Packages:
  benchmark:
    Type: builtin.ior
    Global ID: performance_testing.benchmark
    Configuration:
      interceptors: ['profiler', 'tracer']
      nprocs: 4
      block: 1G
Interceptors:
  profiler:
    Type: builtin.perf_profiler
    Global ID: performance_testing.profiler
    Configuration:
      sampling_rate: 1000
      output_file: /tmp/perf.out
  tracer:
    Type: builtin.io_tracer
    Global ID: performance_testing.tracer
    Configuration:
      trace_reads: true
      trace_writes: true
      min_size: 1024
```

### Runtime Execution Flow

1. **Pipeline Start**: `jarvis ppl start` is called
2. **Package Processing**: For each package in the pipeline:
   - Load package instance
   - Check `interceptors` list in package configuration
   - For each referenced interceptor:
     - Load interceptor instance from pipeline interceptors
     - Share the same `mod_env` reference between interceptor and package
     - Call interceptor's `modify_env()` method
   - Start the package with the modified environment

This architecture ensures that interceptors can modify the exact environment that packages will use, providing seamless interception capabilities.

## Pipeline Management Commands

### `jarvis ppl destroy`

Destroys a pipeline by removing its directory and configuration files. Automatically cleans package data before destruction.

**Usage:**
```bash
# Destroy the current pipeline
jarvis ppl destroy

# Destroy a specific pipeline by name
jarvis ppl destroy pipeline_name
```

**Behavior:**
- If no pipeline name is provided, destroys the current pipeline
- Attempts to clean package data before destruction using each package's `clean()` method
- Removes the entire pipeline directory and configuration files
- Clears the current pipeline if the destroyed pipeline was active
- Shows remaining pipelines after successful destruction

**Example:**
```bash
# Create and destroy a test pipeline
jarvis ppl create test_pipeline
jarvis ppl append echo
jarvis ppl destroy  # Destroys current pipeline (test_pipeline)

# Destroy a specific pipeline while working on another
jarvis cd other_pipeline
jarvis ppl destroy test_pipeline  # Destroys test_pipeline, keeps other_pipeline active
```