"""
WRP Start Runtime Jarvis Package

Service type package for deploying Chimaera runtime across multiple nodes.
Contains all parameters necessary to build the chimaera configuration.
Stores generated configuration path in environment variable for runtime/client initialization.
"""

import os
import tempfile
import yaml
from jarvis_cd.basic.pkg import Service
from jarvis_util import *


class WrpRuntime(Service):
    """
    Service package for starting Chimaera runtime on multiple nodes
    """
    
    def _init(self):
        """
        Initialize the service package
        """
        pass
    
    def _configure_menu(self):
        """
        Define the configuration menu for Chimaera runtime parameters
        """
        return [
            {
                'name': 'config_file',
                'msg': 'Path to custom Chimaera configuration file (optional - will generate default if not provided)',
                'type': str,
                'default': '${HOME}/chimaera_config.yaml'
            },
            {
                'name': 'low_latency_threads',
                'msg': 'Number of low latency worker threads',
                'type': int,
                'default': 4
            },
            {
                'name': 'high_latency_threads',
                'msg': 'Number of high latency worker threads',
                'type': int,
                'default': 2
            },
            {
                'name': 'reinforcement_threads',
                'msg': 'Number of reinforcement worker threads',
                'type': int,
                'default': 1
            },
            {
                'name': 'process_reaper_threads',
                'msg': 'Number of process reaper threads',
                'type': int,
                'default': 1
            },
            {
                'name': 'main_segment_size',
                'msg': 'Main memory segment size in bytes (default: 1GB)',
                'type': int,
                'default': 1073741824
            },
            {
                'name': 'client_data_segment_size',
                'msg': 'Client data segment size in bytes (default: 512MB)',
                'type': int,
                'default': 536870912
            },
            {
                'name': 'runtime_data_segment_size',
                'msg': 'Runtime data segment size in bytes (default: 512MB)',
                'type': int,
                'default': 536870912
            },
            {
                'name': 'port',
                'msg': 'ZeroMQ port for network communication',
                'type': int,
                'default': 5555
            },
            {
                'name': 'logging_level',
                'msg': 'Logging level (debug, info, warn, error)',
                'type': str,
                'default': 'info'
            },
            {
                'name': 'log_file',
                'msg': 'Path to log file',
                'type': str,
                'default': '/tmp/chimaera.log'
            },
            {
                'name': 'stack_size',
                'msg': 'Stack size per task in bytes (default: 64KB)',
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
                'name': 'task_queue_lanes',
                'msg': 'Number of concurrent lanes for task processing',
                'type': int,
                'default': 4
            },
            {
                'name': 'heartbeat_interval',
                'msg': 'Heartbeat interval in milliseconds',
                'type': int,
                'default': 1000
            },
            {
                'name': 'task_timeout',
                'msg': 'Task timeout in milliseconds',
                'type': int,
                'default': 30000
            },
            {
                'name': 'hostfile',
                'msg': 'Path to hostfile for distributed deployment',
                'type': str,
                'default': '${HOME}/hostfile'
            }
        ]
    
    def _configure(self, **kwargs):
        """
        Configure the Chimaera runtime service with provided parameters
        """
        self.update_config(kwargs, rebuild=False)
    
    def _generate_config(self):
        """
        Generate Chimaera configuration file from menu parameters
        """
        # Create configuration dictionary from menu parameters
        config = {
            "workers": {
                "low_latency_threads": self.config["low_latency_threads"],
                "high_latency_threads": self.config["high_latency_threads"],
                "reinforcement_threads": self.config["reinforcement_threads"],
                "process_reaper_threads": self.config["process_reaper_threads"]
            },
            "memory": {
                "main_segment_size": self.config["main_segment_size"],
                "client_data_segment_size": self.config["client_data_segment_size"],
                "runtime_data_segment_size": self.config["runtime_data_segment_size"]
            },
            "network": {
                "port": self.config["port"]
            },
            "logging": {
                "level": self.config["logging_level"],
                "file": self.config["log_file"]
            },
            "performance": {
                "stack_size": self.config["stack_size"],
                "queue_depth": self.config["queue_depth"]
            },
            "task_queue": {
                "lanes": self.config["task_queue_lanes"]
            },
            "shared_memory": {
                "main_segment_name": "chi_main_segment_${USER}",
                "client_data_segment_name": "chi_client_data_segment_${USER}",
                "runtime_data_segment_name": "chi_runtime_data_segment_${USER}"
            },
            "runtime": {
                "heartbeat_interval": self.config["heartbeat_interval"],
                "task_timeout": self.config["task_timeout"]
            }
        }
        
        # Create temporary configuration file
        fd, config_path = tempfile.mkstemp(suffix='.yaml', prefix='chimaera_config_')
        try:
            with os.fdopen(fd, 'w') as f:
                yaml.dump(config, f, default_flow_style=False, indent=2)
        except:
            os.close(fd)
            raise
            
        return config_path
    
    def start(self):
        """
        Start the Chimaera runtime service on all nodes
        """
        # Check if custom config file exists, otherwise generate one
        if "config_file" in self.config and os.path.exists(self.config["config_file"]):
            config_path = self.config["config_file"]
            self.log(f"Using custom configuration file: {config_path}")
        else:
            config_path = self._generate_config()
            self.log(f"Generated configuration file: {config_path}")
        
        # Store configuration path in environment for runtime/client initialization
        self.env["CHI_SERVER_CONF"] = config_path
        self.env["CHI_CLIENT_CONF"] = config_path
        
        # Log the configuration being used
        self.log(f"Chimaera configuration path stored in CHI_SERVER_CONF and CHI_CLIENT_CONF: {config_path}")
        
        # Create PsshExecInfo to launch runtime on all nodes
        hostfile = self.config["hostfile"]
        
        if not os.path.exists(hostfile):
            raise FileNotFoundError(f"Hostfile not found: {hostfile}")
        
        self.log(f"Starting Chimaera runtime on nodes from hostfile: {hostfile}")
        self.log("Using chimaera_start_runtime from PATH environment variable")
        
        # Use PsshExecInfo to launch on all nodes specified in hostfile
        self.daemon_pkg = Exec(f"export CHI_SERVER_CONF={config_path} && chimaera_start_runtime",
                               PsshExecInfo(hostfile=hostfile, env=self.env, exec_async=True))
        
        self.log("Chimaera runtime started successfully on all nodes")
    
    def stop(self):
        """
        Stop the Chimaera runtime service on all nodes
        """
        hostfile = self.config["hostfile"]
        
        if not os.path.exists(hostfile):
            self.log(f"Warning: Hostfile not found: {hostfile}")
            return
            
        self.log(f"Stopping Chimaera runtime on nodes from hostfile: {hostfile}")
        self.log("Using chimaera_stop_runtime from PATH environment variable")
        
        # Use PsshExecInfo to stop on all nodes
        Exec("chimaera_stop_runtime", PsshExecInfo(hostfile=hostfile, env=self.env))
        
        # Wait for daemon to finish if it exists
        if hasattr(self, 'daemon_pkg') and self.daemon_pkg is not None:
            self.daemon_pkg.wait()
        
        # Clean up generated config file if we created it
        if "CHI_SERVER_CONF" in self.env:
            try:
                os.unlink(self.env["CHI_SERVER_CONF"])
                self.log(f"Cleaned up temporary config file: {self.env['CHI_SERVER_CONF']}")
            except:
                pass  # Ignore cleanup errors
                
        self.log("Chimaera runtime stopped successfully on all nodes")