Below is a guide for building packages and pipeline scripts in Jarvis.

# Custom Repos

There are cases where your organization may have packages used internally and
do not want to have to wait to be integrated into the builtin repo.

## Repo structure

Custom repos have the following structure:

```
my_org_name
└── my_org_name
    └── orangefs
        └── package.py
```

## Register a custom repo

You can then register the repo as follows:

```bash
jarvis repo add /path/to/my_org_name
```

Whenever a new repo is added, it will be the first place
jarvis searches for pkgs.

## Creating pkgs from a template

You can then add pkgs to the repo as follows:

```bash
jarvis repo create [name] [pkg_class]
```

pkg_class can be one of:

- service
- app
- interceptor

For example:

```bash
jarvis repo create hermes service
```

The repo will then look as follows:

```
my_org_name
└── my_org_name
    ├── hermes
    │   └── package.py
    └── orangefs
        └── package.py
```

## Promoting a repo

Jarvis searches repos in a certain order. To make a repo the first place
that jarvis searches, run:

```bash
jarvis repo promote [repo_name]
```

## Remove a repo from consideration

Sometimes a repo needs to be removed entirely from consideration.
To do this, run:

```bash
jarvis repo remove [repo_name]
```

This will not destroy the contents of the repo, it will simply unregister
the repo from Jarvis.


# Building a Package

This guide documents how to extend the set of applications that Jarvis is
able to deploy. We refer to these as packages (pkgs for short).

## Bootstrap a `Pkg`

You can bootstrap a pkg to the primary repo as follows:

```bash
jarvis repo create [name] [pkg_class]
```

`pkg_class` can be one of:

- service
- app
- interceptor

For example:

```bash
jarvis repo create hermes service
jarvis repo create hermes_mpiio interceptor
jarvis repo create gray_scott app
```

We can then create an example pipeline as follows:

```bash
jarvis ppl create test
jarvis ppl append hermes
jarvis ppl append hermes_mpiio
jarvis ppl append gray_scott
```

This is an example of a pipeline which will deploy Hermes, the Hermes MPI-IO
interceptor, and Gray Scott, which is an application which performs I/O using
MPI.

## The `Pkg` Base Class

This section will go over the variables and methods common across all Pkg types.
These variables will be initialized automatically.

```python
class Pkg:
  def __init__(self):
    self.pkg_dir = '...'
    self.shared_dir = '...'
    self.private_dir = '...'
    self.env = {}
    self.mod_env = {}
    self.config = {}
    self.global_id = '...'
    self.pkg_id = '...'
```

### `pkg_id` and `global_id`

The Global ID (global_id) is the globally unique ID of the a package in all of
jarvis. It is a dot-separated string. Typically, the format is:

```
{pipeline_id}.{pkg_id}
```

The Package ID (pkg_id) is the unique ID of the package relative to a pipeline.
This is a simple string (no dots).

For example, from section 5.1, we appended 3 packages: hermes, hermes_mpiio, and
gray_scott. hermes, hermes_mpiio, and gray_scott are also the pkg_ids. The
global_ids would be:

```
test.hermes
test.hermes_mpiio
test.gray_scott
```

Usage:

```
self.global_id
self.pkg_id
```

### `pkg_dir`

The package directory is the location of the class python file on the filesystem.
For example, when calling `jarvis repo create hermes`, the directory
created by this command will be the pkg_dir.

One use case for the pkg_dir is for creating template configuration files.
For example, OrangeFS has a complex XML configuration which would be a pain
to repeat in Python. One could include an OrangeFS XML config in their
package directory and commit as part of their Jarvis repo.

Usage:

```
self.pkg_dir
```

### `shared_dir`

The shared_dir is a directory stored on a filesystem common across all nodes
in the hostfile. Each node has the same view of data in the shared_dir. The
shared_dir contains data for the specific pkg to avoid conflicts in
a pipeline with multiple pkgs.

For example, when deploying Hermes, we assume that each node has the Hermes
configuration file. Each node is expected to have the same configuration file.
We store the Hermes config in the shared_dir.

Usage:

```
self.shared_dir
```

### `private_dir`

This is a directory which is common across all nodes, but nodes do not
have the same view of data.

For example, when deploying OrangeFS, it is required that each node has a file
called pvfs2tab. It essentially stores the protocol + address that OrangeFS
uses for networking. However, the content of this file is different for
each node. Storing it in the shared_dir would be incorrect. This is why we
need the private_dir.

Usage:

```
self.private_dir
```

### `env`

Jarvis pipelines store the current environment in a YAML file, which represents
a python dictionary. The key is the environment variable name (string) and the
value is the intended meaning of the variable. There is a single environment
used for the entire pipeline. Each pipeline stores its own environment to avoid
conflict.

Usage:

```
self.env['VAR_NAME']
```

Environments can be modified using various helper functions:

```
self.track_env(env_track_dict)
self.prepend_env(env_name, val)
self.setenv(env_name, val)
```

Viewing the env YAML file for the current pipeline from the CLI

```
cat `jarvis path`/env.yaml
```

### `mod_env`
a python dictionary. Essentially a copy of `env`. However, `mod_env` also stores the LD_PRELOAD environment variable for interception. This can cause conflict if used irresponsibly. Not every program should be intercepted.

For example, we use this for Hermes to intercept POSIX I/O. However, POSIX is widely-used for I/O so we like to be very specific when it is used.

`mod_env` can be modified using the same functions as `env`.

```
self.track_env(env_track_dict)
self.prepend_env(env_name, val)
self.setenv(env_name, val)
```

### `config`

The Jarvis configuration is stored in `{pkg_dir}/{pkg_id}.yaml`.
Unlike the environment dict, this stores variables that are specific to
the package. They are not global to the pipeline.

For example, OrangeFS and Hermes need to know the desired port number and
RPC protocol. This information is specific to the program, not the entire
pipeline.

Usage:

```
self.config['VAR_NAME']
```

### `jarvis`

The Jarvis CD configuration manager stores various properties global to
all of Jarvis. The most important information is the hostfile and resource_graph,
discussed in the next sections.

Usage:

```
self.jarvis
```

### `hostfile`

The hostfile contains the set of all hosts that Jarvis has access to.
The hostfile format is documented [here](https://github.com/scs-lab/jarvis-util/wiki/4.-Hostfile).

Usage:

```
self.jarvis.hostfile
```

### `resource_graph`

The resource graph can be queried to get storage and networking information
for storing large volumes of data.

```
self.jarvis.resource_graph
```

## Building a Service or Application

Services and Applications implement the same interface, but are logically
slightly different. A service is long-running and would typically require
the users to manually stop it. Applications stop automatically when it
finishes doing what it's doing. Jarvis can deploy services alongside
applications to avoid the manual stop when benchmarking.

### `_init`

The Jarvis constructor (`_init`) is used to initialize global variables.
Don't assume that self.config is initialized.
This is to provide an overview of the parameters of this class.
Default values should almost always be None.

```python
def _init(self):
  self.gray_scott_path = None
```

### `_configure_menu`

The function defines the set of command line options that the user can set.
An example configure menu is below:

```python
def _configure_menu(self):
    """
    Create a CLI menu for the configurator method.
    For thorough documentation of these parameters, view:
    https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing

    :return: List(dict)
    """
    return [
        {
            'name': 'port',
            'msg': 'The port to listen for data on',
            'type': int,
            'default': 8080
        }
    ]
```

This function is called whenever configuring a package. For example,

```bash
jarvis pkg configure hermes --sleep=10 --port=25
```

This will configure hermes to sleep for 10 seconds after launching to give enough
time to fully start Hermes. Sleep is apart of all configure menus by default.

The format of the output dict is documented in more detail
[here](https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing).

### `configure`

It takes as input a
dictionary. The keys of this dict are determined from \_configure_menu function
output. It is responsible for updating the self.config variable appropriately
and generating the application-specific configuration files.

Below is an example for Hermes. This example takes as input the port option,
modifies the hermes_server dict, and then stores the dict in a YAML file
in the shared directory.

```python
def configure(self, **kwargs):
    """
    Converts the Jarvis configuration to application-specific configuration.
    E.g., OrangeFS produces an orangefs.xml file.

    :param config: The human-readable jarvis YAML configuration for the
    application.
    :return: None
    """
    self.update_config(kwargs, rebuild=False)
    hermes_server_conf = {
      'port': self.config['port']
    }
    YamlFile(f'{self.shared_dir}/hermes_server_yaml').save(hermes_server_conf)
```

This function is called whenever configuring a packge. Specifically, this is
called immediately after \_configure_menu. For example,

```
jarvis pkg configure hermes --sleep=10 --port=25
```

will make the kwargs dict be:

```python
{
  'sleep': 10,
  'port': 25
}
```

### `start`

The start function is called during `jarvis ppl run` and `jarvis ppl start`.
This function should execute the program itself.

Below is an example for Hermes:

```python
def start(self):
    """
    Launch an application. E.g., OrangeFS will launch the servers, clients,
    and metadata services on all necessary pkgs.

    :return: None
    """
    self.daemon_pkg = Exec('hermes_daemon',
                            PsshExecInfo(hostfile=self.jarvis.hostfile,
                                         env=self.env,
                                         exec_async=True))
    time.sleep(self.config['sleep'])
    print('Done sleeping')
```

### `stop`

The stop function is called during `jarvis ppl run` and `jarvis ppl stop`.
This function should terminate the program.

Below is an example for Hermes:

```python
def stop(self):
    """
    Stop a running application. E.g., OrangeFS will terminate the servers,
    clients, and metadata services.

    :return: None
    """
    Exec('finalize_hermes',
         PsshExecInfo(hostfile=self.jarvis.hostfile,
                      env=self.env))
    if self.daemon_pkg is not None:
        self.daemon_pkg.wait()
    Kill('hermes_daemon',
         PsshExecInfo(hostfile=self.jarvis.hostfile,
                      env=self.env))
```

This is not typically implemented for Applications, but it is for Services.

### `kill`
This function is called during `jarvis ppl kill`. It should forcibly terminate a program, typically using Kill.

Below is an example for Hermes
```python
def kill(self):
    """
    Forcibly a running application. E.g., OrangeFS will terminate the
    servers, clients, and metadata services.

    :return: None
    """
    Kill('hermes_daemon',
         PsshExecInfo(hostfile=self.jarvis.hostfile,
                      env=self.env))
```

### `clean`

The `clean` function is called during `jarvis ppl clean`.
It clears all intermediate data produced by a pipeline.

Below is the prototype

```python
def clean(self):
    """
    Destroy all data for an application. E.g., OrangeFS will delete all
    metadata and data directories in addition to the orangefs.xml file.

    :return: None
    """
    pass
```

### `status`

The `status` function is called during `jarvis ppl status`
It determines whether or not a service is running. This is not typically
implemented for Applications, but it is for Services.

## Building an Interceptor

Interceptors are used to modify environment variables to route system and library
calls to new functions.

Interceptors have a slightly different interface -- they only have:
`_init`, `_configure_menu`, `configure`, and `modify_env`. The only new function
here is modify_env. The others were defined in the previous section and behave
the exact same way.

### `configure`

Configuring an interceptor tends to be a little different. The interceptors
are not typically responsible for generating configuration files like the
applications and services do. These typically are responsible solely for
modifying the environment.

Below, we show an example of configure for the Hermes MPI I/O interceptor:

```python
def configure(self, **kwargs):
    """
    Converts the Jarvis configuration to application-specific configuration.
    E.g., OrangeFS produces an orangefs.xml file.

    :param kwargs: Configuration parameters for this pkg.
    :return: None
    """
    self.update_config(kwargs, rebuild=False)
    self.config['HERMES_MPIIO'] = self.find_library('hermes_mpiio')
    if self.config['HERMES_MPIIO'] is None:
        raise Exception('Could not find hermes_mpiio')
    print(f'Found libhermes_mpiio.so at {self.config["HERMES_MPIIO"]}')
```

Here we use self.find_library() to check if we can find the shared library
hermes_mpiio in the system paths. This function introspects LD_LIBRARY_PATH
and determines if hermes_mpiio is in the path. It saves the path in the pkg
configuration (self.config).

### `modify_env`

Below is an example of the MPI I/O interceptor for Hermes:

```python
def modify_env(self):
    """
    Modify the jarvis environment.

    :return: None
    """
    self.prepend_env('LD_PRELOAD', self.config['HERMES_MPIIO'])
```

## A Note on `jarvis-util`

`jarvis-cd` aims to provide structure to storing configuration files for simplifying
complex benchmarks.

`jarvis-util` is primarily responsible for handling program execution. This
includes things like executing MPI and PSSH in Python. This is where the
`Exec` and `PsshExecInfo` data structures come from. More information
on `jarvis-util` can be found [here](https://github.com/scs-lab/jarvis-util/wiki).

# Example Package: IOR

```python
"""
This module provides classes and methods to launch the Ior application.
Ior is a benchmark tool for measuring the performance of I/O systems.
It is a simple tool that can be used to measure the performance of a file system.
It is mainly targeted for HPC systems and parallel I/O.
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *
import os


class Ior(Application):
    """
    This class provides methods to launch the Ior application.
    """
    def _init(self):
        """
        Initialize paths
        """
        pass

    def _configure_menu(self):
        """
        Create a CLI menu for the configurator method.
        For thorough documentation of these parameters, view:
        https://github.com/scs-lab/jarvis-util/wiki/3.-Argument-Parsing

        :return: List(dict)
        """
        return [
            {
                'name': 'write',
                'msg': 'Perform a write workload',
                'type': bool,
                'default': True,
                'choices': [],
                'args': [],
            },
            {
                'name': 'read',
                'msg': 'Perform a read workload',
                'type': bool,
                'default': False,
            },
            {
                'name': 'xfer',
                'msg': 'The size of data transfer',
                'type': str,
                'default': '1m',
            },
            {
                'name': 'block',
                'msg': 'Amount of data to generate per-process',
                'type': str,
                'default': '32m',
                'aliases': ['block_size']
            },
            {
                'name': 'api',
                'msg': 'The I/O api to use',
                'type': str,
                'choices': ['posix', 'mpiio', 'hdf5'],
                'default': 'posix',
            },
            {
                'name': 'fpp',
                'msg': 'Use file-per-process',
                'type': bool,
                'default': False,
            },
            {
                'name': 'reps',
                'msg': 'Number of times to repeat',
                'type': int,
                'default': 1,
            },
            {
                'name': 'nprocs',
                'msg': 'Number of processes',
                'type': int,
                'default': 1,
            },
            {
                'name': 'ppn',
                'msg': 'The number of processes per node',
                'type': int,
                'default': 16,
            },
            {
                'name': 'out',
                'msg': 'Path to the output file',
                'type': str,
                'default': '/tmp/ior.bin',
                'aliases': ['output']
            },
            {
                'name': 'log',
                'msg': 'Path to IOR output log',
                'type': str,
                'default': None,
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        self.config['api'] = self.config['api'].upper()

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        cmd = [
            'ior',
            '-k',
            f'-b {self.config["block"]}',
            f'-t {self.config["xfer"]}',
            f'-a {self.config["api"]}',
            f'-o {self.config["out"]}',
        ]
        out = os.path.expandvars(self.config['out'])
        if self.config['write']:
            cmd.append('-w')
        if self.config['read']:
            cmd.append('-r')
        if self.config['fpp']:
            cmd.append('-F')
        if self.config['reps'] > 1:
            cmd.append(f'-i {self.config["reps"]}')
        if '.' in os.path.basename(out):
            os.makedirs(str(pathlib.Path(out).parent),
                        exist_ok=True)
        else:
            os.makedirs(out, exist_ok=True)
        # pipe_stdout=self.config['log']
        Exec('which mpiexec',
             LocalExecInfo(env=self.mod_env))
        Exec(' '.join(cmd),
             MpiExecInfo(env=self.mod_env,
                         hostfile=self.jarvis.hostfile,
                         nprocs=self.config['nprocs'],
                         ppn=self.config['ppn'],
                         do_dbg=self.config['do_dbg'],
                         dbg_port=self.config['dbg_port']))

    def stop(self):
        """
        Stop a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        pass

    def clean(self):
        """
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        Rm(self.config['out'] + '*',
           PsshExecInfo(env=self.env,
                        hostfile=self.jarvis.hostfile))

    def _get_stat(self, stat_dict):
        """
        Get statistics from the application.

        :param stat_dict: A dictionary of statistics.
        :return: None
        """
        stat_dict[f'{self.pkg_id}.runtime'] = self.start_time
```

# Program Execution

```python
from jarvis_util.shell.exec import Exec
```

`Exec` is used to execute a binary program as a subprocess in Python. `Exec` can be used for local, remote, or parallel execution of code. Exec is currently a wrapper around the following libraries:

1. Subprocess: executes a program locally on a machine. We use shell=True here. The intention is to be equivalent to a bash script.
2. SSH: executes a program remotely using SSH. This has only been tested on Linux. It is equivalent to executing "ssh" in the terminal.
3. Parallel SSH (PSSH): executes a program on multiple remote hosts. Relies upon the SSH module.
4. Message Passing Interface (MPI): executes a program in parallel using MPI. Only tested over MPICH at this time.

`Exec` has a simple syntax. It takes as input a command (cmd) and how the command should be executed (`exec_info`). For example, `exec_info` can be used to represent executing the command in parallel using MPI or locally on a machine using subprocess.

```python
from jarvis_util.shell.exec import Exec
Exec(cmd, exec_info)
```

Exec can be called with only specifying "cmd". In this case, the command will be executed locally. It's output will be printed to the terminal.

```python
from jarvis_util.shell.exec import Exec
Exec(cmd)
```

## `ExecInfo`

`ExecInfo` stores all information which may be needed to execute a command with a particular protocol. This includes information such as the location of private/public keys, hostfiles, environment variables. `ExecInfo` also includes parameters for collecting output from commands.

```python
ExecInfo(exec_type=ExecType.LOCAL, nprocs=None, ppn=None,
         user=None, pkey=None, port=None, hostfile=None, env=None,
         sleep_ms=0, sudo=False, cwd=None, hosts=None,
         collect_output=None, pipe_stdout=None, pipe_stderr=None,
         hide_output=None, exec_async=False, stdin=None)
```

### Specifying execution method (e.g., SSH vs MPI)

There are many ways to execute a command: Subprocess, SSH, etc. To specify this, there is an enum with all currently supported methods. The supported methods are:

1. `ExecType.LOCAL`
2. `ExecType.SSH`
3. `ExecType.PSSH`
4. `ExecType.MPI`

Setting `exec_type` will spawn the command using the particular approach. By default, `exec_type` is `ExecType.LOCAL`.

### Managing output from commands

ExecInfo has three parameters for collecting output from commands:

1. `collect_output`: Whether to store the output from the command in a buffer in Python. Will impact memory utilization if the command has large output. This is `False` by default.
2. `pipe_stdout`: Store stdout in a file. By default, this is `None`.
3. `pipe_stderr`: Store stderr in a file. By default, this is `None`.
4. `hide_output`: Don't print output.

Unlike typical subprocess, you can perform any combination of these. Output can be collected at the same time it's being printed. This is particularly useful if you have a long-running process you want to collect output from AND ensure is still progressing. This is accomplished by spawning two threads: one for collecting stderr, and another for collecting stdout.

### Asynchronous execution

ExecInfo enables the ability to execute a command asynchronously. This is particularly useful for running a daemon. For example, deploying a storage system requires the storage system to run as a service. This can cause the program to block forever unless asynchronous execution is enabled. Async execution is specified using the `exec_async=True`.

## `LocalExec`

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.local_exec import LocalExecInfo
```

The simplest way to execute a program locally is as follows:

```python
from jarvis_util.shell.exec import Exec
node = Exec('echo hello')
```

This will print "hello" to the console.

However, if more control is needed, a `LocalExecInfo` contains many helpful paramters.
The following demonstrates various examples of outputs:

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.local_exec import LocalExecInfo

# Will ONLY print to the terminal
node = Exec('echo hello', LocalExecInfo(collect_output=False))
# Will collect AND print to the terminal
node = Exec('echo hello', LocalExecInfo(collect_output=True))
# Will collect BUT NOT print to the terminal
node = Exec('echo hello', LocalExecInfo(collect_output=True,
                                        hide_output=True))
# Will collect, pipe to file, and print to terminal
node = Exec('echo hello', LocalExecInfo(collect_output=True,
                                        pipe_stdout='/tmp/stdout.txt',
                                        pipe_stderr='/tmp/stderr.txt'))
```

To execute a program asynchronously, one can do:

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.local_exec import LocalExecInfo

node = Exec('echo hello', LocalExecInfo(exec_async=True))
node.wait()
```

## `SshExec`

The following code will execute the "hostname" command on the local host using SSH.

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.ssh_exec import SshExecInfo

node = Exec('hostname', SshExecInfo(hosts='localhost'))
```

## `PsshExec`

The following code will execute the "hostname" command on all machines in the hostfile

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.pssh_exec import PsshExecInfo

node = Exec('hostname', PsshExecInfo(hostfile="/tmp/hostfile.txt"))
```

## `MpiExec`

The following code will execute the "hostname" command on the local machine 24 times using MPI.

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.mpi_exec import MpiExecInfo

node = Exec('hostname', MpiExecInfo(hostfile=None,
                                    nprocs=24,
                                    ppn=None))
```

The following code will execute the "hostname" command on 4 nodes (specified in hostfile) using MPI.
"ppn" stands for processes per node.

```python
from jarvis_util.shell.exec import Exec
from jarvis_util.shell.mpi_exec import MpiExecInfo

node = Exec('hostname', MpiExecInfo(hostfile="/tmp/hostfile.txt",
                                    nprocs=4,
                                    ppn=1))
```

# Built-in Wrappers

We have various wrappers to support much shell functionality. At this time, these have been built and tested for Linux. These codes inherit from the `Exec` class shown in Section 1. This way, they can be executed locally or in parallel.

## Creating + Deleting Directories

We provide various wrappers for filesystem commands.

```python
from jarvis_util.shell.filesystem import Mkdir
from jarvis_util.shell.filesystem import Rm

# Creates two directories "path1" and "path2"
Mkdir(['path1', 'path2'])
# Creates a single directory path3
Mkdir('path3')

# Remove two directories (including subdirectories + files)
Rm(['path1', 'path2'])
# Remove a single directory
Rm('path3')
```

## Killing Processes

We provide a wrapper for pkill, which can kill processes in parallel

```python
from jarvis_util.shell.process import Kill

# Kill all processes matching pattern
Kill('hermes')
```

# Pipeline Scripts

Pipeline scripts are useful for storing cross-platform unit tests.
They store all of the information needed to create and execute
a pipeline.

## Running a pipline script

Pipeline scripts are YAML files and can be executed as follows:
```bash
jarvis ppl load yaml /path/to/my_pipeline.yaml
jarvis ppl run
```

Alternatively, if you want to load + run the script:
```bash
jarvis ppl run yaml /path/to/my_pipeline.yaml
```

## Updating a pipeline

To load changes made to a pipeline script, you can run:
```bash
jarvis ppl update yaml
```

The pipeline will store the path so you don't have to repeat.

## Example Pipeline Script

Below is a small example of a file for testing block device I/O
in a task-based I/O system named Chimaera.

The script is named ``test_bdev_io.yaml``

```yaml
name: chimaera_unit_ipc
env: chimaera
pkgs:
  - pkg_type: chimaera_run
    pkg_name: chimaera_run
    sleep: 10
    do_dbg: true
    dbg_port: 4000
  - pkg_type: chimaera_unit_tests
    pkg_name: chimaera_unit_tests
    TEST_CASE: TestBdevIo
    do_dbg: true
    dbg_port: 4001
```

## name: chimaera_unit_ipc

The name of the pipeline that jarvis references.

The following command would focus the pipeline in jarvis:
```bash
jarvis cd chimaera_unit_ipc
```

## env: chimaera

This command loads a named environment file.
It expects the environmnt to already exist.

In this example, the environment is expected to
be named ``chimaera``

```bash
jarvis env build chimaera
```

When you run ``jarvis ppl load yaml test_bdev_io.yaml``,
the environment chimaera will be automatically loaded.

## pkgs:

In this section, we define the parameters to each package
in the pipeline. 

Here, we have two packages, chimaera_run (the server) and
chimaera_unit_tests (the client). 

When you run ``jarvis ppl load yaml test_bdev_io.yaml``,
the following commands will be executed internally by Jarvis:
```bash
jarvis ppl append chimaera_run sleep=10 +do_dbg dbg_port=4000
jarvis ppl append chimaera_unit_tests +do_dbg dbg_port=4001 TEST_CASE=TestBdevIo
```

# Pipeline Tests

Pipeline tests are used to run experiment sets using
a grid search.

## Example File

Below is an example of a pipeline for running various 
configurations of spark KMeans.

```yaml
config:
  name: mm_kmeans_spark
  env: mega_mmap
  pkgs:
    - pkg_type: spark_cluster
      pkg_name: spark_cluster
      num_nodes: 1
    - pkg_type: mm_kmeans_df
      pkg_name: mm_kmeans_df
      path: ${HOME}/mm_data/parquet/kmeans.parquet
      window_size: 4g
      df_size: 4g
      nprocs: 1
      ppn: 16
      type: parquet
      k: 1000
    - pkg_type: mm_kmeans
      pkg_name: mm_kmeans
      path: ${HOME}/mm_data/parquet/*
      window_size: 30g
      api: spark
      max_iter: 4
      k: 8
      do_dbg: False
      dbg_port: 4001
vars:
  mm_kmeans_df.window_size: [16m, 64m, 128m, 1g, 2g, 4g]
  mm_kmeans_df.df_size: [16m, 64m, 128m, 1g, 2g, 4g]
  spark_cluster.num_nodes: [4]
loop:
  - [mm_kmeans_df.window_size, mm_kmeans_df.df_size]
  - [spark_cluster.num_nodes]
repeat: 1
output: "${SHARED_DIR}/output_multi"
```

## config:

This section is the skeleton of a pipeline. It has the same exact parameters
as a [pipeline script](05-pipeline-scripts.md).

This example, the pipeline will be called mm_kmeans_spark and launch a spark 
cluster, dataset generator, and spark kmeans in that order.

## vars:

Each pkg in the pipeline has a set of variables it exposes. In this example,
we vary the dataset size, a window size, and the number of nodes in the spark cluster.

The syntax of variables are: ``pkg_name.var_name``

## loop:

This is represents what test loop should look like. In pseudocode,
the above loop would translate to python roughly as follows:

```python
mm_kmeans_df_window_size = ['16m', '64m', '128m', '1g', '2g', '4g']
mm_kmeans_df_df_size = ['16m', '64m', '128m', '1g', '2g', '4g']
spark_cluster_num_nodes = [4]
for window_size, df_size in mm_kmeans_df_window_size, mm_kmeans_df_df_size:
  for num_nodes in spark_cluster_num_nodes:
    mm_kmeans_spark.configure(window_size, df_size, num_nodes)
```

In this example, a total of 6 cases are executed: 
```
16m 16m 4
64m 64m 4
128m 128m 4
1g 1g 4
2g 2g 4
4g 4g 4
```

By having a separate loop section, you can define certain variables as together or independent
to reduce the number of total test cases. In this example, ``mm_kmeans_df.window_size`` and ``mm_kmeans_df.df_size``
vary together, but independently from ``spark_cluster.num_nodes``.

``mm_kmeans_df.window_size`` and ``mm_kmeans_df.df_size`` must have the same size (in this case 6).

# repeat:

The number of times each experiment should be conducted. For example,
this can be used to calculate the average across experiment runs to
get a better understanding of variability and noise in your study.

In this example, experiments are only conducted once.

# output

This is the directory where the results are stored. Note, jarvis stores 
the pipeline's shared directory, private directory, and configuration directory
in the following three environment variables: ``${SHARED_DIR}``, ``${PRIVATE_DIR}``,
and ``${CONFIG_DIR}``.

By default, the output of this is going to be a dataset with each variable as a parameter:
```
[mm_kmeans_df.window_size] [mm_kmeans_df.df_size] [spark_cluster.num_nodes]
```

To get more columns, pkgs can define a custom ``_get_stat()`` function. This is more
for developers than users: Below is an example of a custom stat for the YCSB benchmark,
which analyzes the output of YCSB for its throughput and total runtime.
```python
class Ycsb:
  def _get_stat(self, stat_dict):
        """
        Get statistics from the application.

        :param stat_dict: A dictionary of statistics.
        :return: None
        """
        output = self.exec.stdout['localhost']
        if 'throughput(ops/sec)' in output:
            throughput = re.search(r'throughput\(ops\/sec\): ([0-9.]+)', output).group(1)
            stat_dict[f'{self.pkg_id}.throughput'] = throughput
        stat_dict[f'{self.pkg_id}.runtime'] = self.start_time
```

# Schedulers

Jarvis can be used inside of job specs. 

Mainly, Jarvis builds a hostfile and allows users to ensure that jarvis commands execute only on a single node.

## Slurm

Below is an example for slurm:
```
#!/bin/bash
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=4    # <- match to OMP_NUM_THREADS
#SBATCH --partition=cpu      # <- or one of: gpuA100x4 gpuA40x4 gpuA100x8 gpuMI100x8
#SBATCH --account=bekn-delta-cpu    # <- match to a "Project" returned by the "accounts" command
#SBATCH --job-name=myjobtest
#SBATCH --time=00:15:00      # hh:mm:ss for the job
#SBATCH --constraint="scratch"
#SBATCH -e slurm-%j.err
#SBATCH -o slurm-%j.out
### GPU options ###
##SBATCH --gpus-per-node=4
##SBATCH --gpu-bind=closest     # <- or closest
##SBATCH --mail-user=you@yourinstitution.edu
##SBATCH --mail-type="BEGIN,END" See sbatch or srun man pages for more email options

spack load iowarp
JARVIS_FIRST=$(jarvis sched hostfile build +slurm_host)
if [ "$JARVIS_FIRST" -eq 0 ]; then
    exit 0
fi
echo "On first node!!!"
# Any other jarvis commands
```

## PBS

```
spack load iowarp
JARVIS_FIRST=$(jarvis sched hostfile build +pbs_host)
if [ "$JARVIS_FIRST" -eq 0 ]; then
    exit 0
fi
echo "On first node!!!"
# Any other jarvis commands
```
