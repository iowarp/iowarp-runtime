"""
IOWarp Runtime Service Package

This package deploys and manages the IOWarp (Chimaera) runtime service across distributed nodes.
Assumes chimaera has been installed and binaries are available in PATH.
"""
from jarvis_cd.core.pkg import Service
from jarvis_cd.shell import Exec, LocalExecInfo, PsshExecInfo
from jarvis_cd.shell.process import Kill, Which
from jarvis_cd.util import SizeType
import os
import yaml


class WrpRuntime(Service):
    """
    IOWarp Runtime Service

    Manages the Chimaera runtime deployment across distributed nodes,
    including configuration generation and runtime lifecycle management.

    Assumes chimaera_start_runtime and chimaera_stop_runtime are installed
    and available in PATH.
    """

    def _init(self):
        """Initialize package-specific variables"""
        self.config_file = None

    def _configure_menu(self):
        """Define configuration options for IOWarp runtime"""
        return [
            {
                'name': 'low_latency_workers',
                'msg': 'Number of low-latency worker threads',
                'type': int,
                'default': 4
            },
            {
                'name': 'high_latency_workers',
                'msg': 'Number of high-latency worker threads',
                'type': int,
                'default': 2
            },
            {
                'name': 'reinforcement_workers',
                'msg': 'Number of reinforcement worker threads',
                'type': int,
                'default': 1
            },
            {
                'name': 'process_reaper_workers',
                'msg': 'Number of process reaper worker threads',
                'type': int,
                'default': 1
            },
            {
                'name': 'main_segment_size',
                'msg': 'Main memory segment size (e.g., 1G, 512M)',
                'type': str,
                'default': '1G'
            },
            {
                'name': 'client_data_segment_size',
                'msg': 'Client data segment size (e.g., 512M, 256M)',
                'type': str,
                'default': '512M'
            },
            {
                'name': 'runtime_data_segment_size',
                'msg': 'Runtime data segment size (e.g., 512M, 256M)',
                'type': str,
                'default': '512M'
            },
            {
                'name': 'zmq_port',
                'msg': 'ZeroMQ port for networking',
                'type': int,
                'default': 5555
            },
            {
                'name': 'task_queue_lanes',
                'msg': 'Number of concurrent task queue lanes',
                'type': int,
                'default': 4
            },
            {
                'name': 'log_level',
                'msg': 'Logging level',
                'type': str,
                'choices': ['debug', 'info', 'warning', 'error'],
                'default': 'info'
            },
            {
                'name': 'stack_size',
                'msg': 'Stack size per task (bytes)',
                'type': int,
                'default': 65536
            },
            {
                'name': 'queue_depth',
                'msg': 'Task queue depth',
                'type': int,
                'default': 10000
            },
            {
                'name': 'heartbeat_interval',
                'msg': 'Runtime heartbeat interval (milliseconds)',
                'type': int,
                'default': 1000
            },
            {
                'name': 'task_timeout',
                'msg': 'Task timeout (milliseconds)',
                'type': int,
                'default': 30000
            }
        ]

    def _configure(self, **kwargs):
        """Configure the IOWarp runtime service"""
        # Set configuration file path in shared directory
        self.config_file = f'{self.shared_dir}/chimaera_config.yaml'

        # Set the CHI_SERVER_CONF environment variable
        # This is what both RuntimeInit and ClientInit check
        self.setenv('CHI_SERVER_CONF', self.config_file)

        # Generate chimaera configuration
        self._generate_config()

        self.log(f"IOWarp runtime configured")
        self.log(f"  Config file: {self.config_file}")
        self.log(f"  CHI_SERVER_CONF: {self.config_file}")

    def _generate_config(self):
        """Generate Chimaera runtime configuration file"""
        # Parse size strings to bytes
        main_size = SizeType(self.config['main_segment_size']).bytes
        client_size = SizeType(self.config['client_data_segment_size']).bytes
        runtime_size = SizeType(self.config['runtime_data_segment_size']).bytes

        # Build configuration dictionary matching chimaera_default.yaml format
        config_dict = {
            'workers': {
                'low_latency_threads': self.config['low_latency_workers'],
                'high_latency_threads': self.config['high_latency_workers'],
                'reinforcement_threads': self.config['reinforcement_workers'],
                'process_reaper_threads': self.config['process_reaper_workers']
            },
            'memory': {
                'main_segment_size': main_size,
                'client_data_segment_size': client_size,
                'runtime_data_segment_size': runtime_size
            },
            'network': {
                'zmq_port': self.config['zmq_port']
            },
            'logging': {
                'level': self.config['log_level'],
                'file': f"{self.shared_dir}/chimaera.log"
            },
            'performance': {
                'stack_size': self.config['stack_size'],
                'queue_depth': self.config['queue_depth']
            },
            'task_queue': {
                'lanes': self.config['task_queue_lanes']
            },
            'shared_memory': {
                'main_segment_name': 'chi_main_segment_${USER}',
                'client_data_segment_name': 'chi_client_data_segment_${USER}',
                'runtime_data_segment_name': 'chi_runtime_data_segment_${USER}'
            },
            'runtime': {
                'heartbeat_interval': self.config['heartbeat_interval'],
                'task_timeout': self.config['task_timeout']
            }
        }

        # Write configuration to YAML file
        with open(self.config_file, 'w') as f:
            f.write('# Chimaera Runtime Configuration\n')
            f.write('# Generated by Jarvis IOWarp Runtime Package\n\n')
            yaml.dump(config_dict, f, default_flow_style=False, sort_keys=False)

        self.log(f"Generated Chimaera configuration: {self.config_file}")

    def start(self):
        """Start the IOWarp runtime service on all nodes"""
        # Verify chimaera_start_runtime is available
        Which('chimaera_start_runtime', LocalExecInfo(env=self.env)).run()

        # Launch runtime on all nodes using PsshExecInfo
        # IMPORTANT: Use env (shared environment), not mod_env
        self.log(f"Starting IOWarp runtime on all nodes")
        self.log(f"  Config (CHI_SERVER_CONF): {self.config_file}")
        self.log(f"  Nodes: {len(self.jarvis.hostfile)}")

        # The chimaera_start_runtime binary will read CHI_SERVER_CONF from environment
        cmd = 'chimaera_start_runtime'

        Exec(cmd, LocalExecInfo(
            env=self.env,  # Use env, not mod_env
            hostfile=self.jarvis.hostfile,
            exec_async=True
        )).run()

        self.sleep()

        self.log("IOWarp runtime started successfully on all nodes")

    def stop(self):
        """Stop the IOWarp runtime service on all nodes"""
        self.log("Stopping IOWarp runtime on all nodes")

        # Verify chimaera_stop_runtime is available
        try:
            Which('chimaera_stop_runtime', LocalExecInfo(env=self.env)).run()
        except:
            self.log("chimaera_stop_runtime not found, attempting forcible kill")
            self.kill()
            return

        # Use chimaera_stop_runtime to gracefully shutdown
        # The stop binary will also read CHI_SERVER_CONF from environment
        cmd = 'chimaera_stop_runtime'

        Exec(cmd, LocalExecInfo(
            env=self.env,
            hostfile=self.jarvis.hostfile
        )).run()

        self.log("IOWarp runtime stopped on all nodes")

    def kill(self):
        """Forcibly terminate the IOWarp runtime on all nodes"""
        self.log("Forcibly killing IOWarp runtime on all nodes")

        Kill('chimaera_start_runtime', PsshExecInfo(
            hostfile=self.jarvis.hostfile
        )).run()

        self.log("IOWarp runtime killed on all nodes")

    def status(self) -> str:
        """Check IOWarp runtime status"""
        # Could enhance this by checking if processes are actually running
        # For now, return basic status
        return "unknown"

    def clean(self):
        """Clean IOWarp runtime data and temporary files"""
        self.log("Cleaning IOWarp runtime data")

        # Remove configuration file
        if self.config_file and os.path.exists(self.config_file):
            os.remove(self.config_file)

        # Remove log file
        log_file = f'{self.shared_dir}/chimaera.log'
        if os.path.exists(log_file):
            os.remove(log_file)

        # Clean shared memory segments on all nodes
        self.log("Cleaning shared memory segments on all nodes")
        cmd = 'rm -f /dev/shm/chi_*'
        Exec(cmd, PsshExecInfo(
            hostfile=self.jarvis.hostfile
        )).run()

        self.log("Cleanup completed")
