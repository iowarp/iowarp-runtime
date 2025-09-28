"""
Chimaera WRP Runtime Jarvis Package

Service package for deploying and managing Chimaera runtime across multiple nodes
in a distributed computing environment. This package provides comprehensive
configuration management for Chimaera's worker threads, memory segments,
networking, and logging subsystems.

Features:
- Automatic configuration file generation from runtime parameters
- Distributed deployment using PSSH across multiple nodes
- Environment variable management for runtime/client initialization  
- Comprehensive lifecycle management (start, stop, kill, clean, status)
- Robust error handling and cleanup procedures

The package assumes chimaera_start_runtime and chimaera_stop_runtime binaries
are available in the PATH environment variable on all target nodes.
"""

import os
import tempfile
import yaml
from jarvis_cd.basic.pkg import Service
from jarvis_cd.shell import Exec, PsshExecInfo
from jarvis_cd.shell.process import Kill, Rm


class WrpRuntime(Service):
    """
    Service package for starting Chimaera runtime on multiple nodes
    """
    
    def _init(self):
        """
        Initialize the service package
        """
        self.config_file_path = None
        self.daemon_process = None
    
    def _configure_menu(self):
        """
        Define the configuration menu for Chimaera runtime parameters
        """
        return [
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
                'default': 'info',
                'choices': ['debug', 'info', 'warn', 'error']
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
            }
        ]
    
    def _configure(self, **kwargs):
        """
        Configure the Chimaera runtime service with provided parameters
        """
        # Validate configuration
        if self.config['port'] <= 0 or self.config['port'] > 65535:
            raise ValueError(f"Invalid port number: {self.config['port']}")
            
        if self.config['low_latency_threads'] <= 0:
            raise ValueError("Low latency threads must be positive")
            
        if self.config['high_latency_threads'] <= 0:
            raise ValueError("High latency threads must be positive")
        
        # Generate configuration file and store in shared directory
        self.config_file_path = self._generate_config()
        
        # Set environment variables as required for RuntimeInit and ClientInit
        self.setenv('CHI_SERVER_CONF', self.config_file_path)
        self.setenv('CHI_CLIENT_CONF', self.config_file_path) 
        
    
    def _generate_config(self):
        """
        Generate Chimaera configuration file from menu parameters
        Store in shared directory as required
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
        
        # Store configuration file in shared directory as specified
        config_path = f"{self.shared_dir}/chimaera_config.yaml"
        
        # Create shared directory if it doesn't exist
        os.makedirs(self.shared_dir, exist_ok=True)
        
        # Write configuration to shared directory
        with open(config_path, 'w') as f:
            yaml.dump(config, f, default_flow_style=False, indent=2)
            
        return config_path
    
    def start(self):
        """
        Start the Chimaera runtime service on all nodes
        """
        try:
            if not self.config_file_path:
                raise RuntimeError("Configuration file not generated. Call configure() first.")
            
            self.log(f"Starting Chimaera runtime with config: {self.config_file_path}")
            self.log("Using chimaera_start_runtime from PATH environment variable")
            
            # Use PsshExecInfo to launch runtime on all nodes specified in hostfile
            # Use env as specified in requirements (not mod_env)
            cmd = f"export CHI_SERVER_CONF={self.config_file_path} && chimaera_start_runtime"
            
            self.daemon_process = Exec(cmd, PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env,
                exec_async=True
            )).run()
            
            self.log("Chimaera runtime started successfully on all nodes")
            
        except Exception as e:
            self.log(f"Error starting Chimaera runtime: {e}")
            # Clean up on failure
            try:
                self.clean()
            except:
                pass  # Ignore cleanup errors during failure
            raise
    
    def stop(self):
        """
        Stop the Chimaera runtime service on all nodes
        """
        try:
            self.log("Stopping Chimaera runtime on all nodes")
            self.log("Using chimaera_stop_runtime from PATH environment variable")
            
            # Use PsshExecInfo to stop on all nodes
            # Use env as specified in requirements (not mod_env)
            Exec("chimaera_stop_runtime", PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env
            )).run()
            
            # Wait for daemon to finish if it exists
            if self.daemon_process is not None:
                self.daemon_process.wait()
            
            self.log("Chimaera runtime stopped successfully on all nodes")
            
        except Exception as e:
            self.log(f"Error stopping Chimaera runtime: {e}")
            raise
    
    def kill(self):
        """
        Forcibly terminate the Chimaera runtime service on all nodes
        """
        try:
            self.log("Force killing Chimaera runtime processes on all nodes")
            
            # Kill chimaera_start_runtime processes
            # Use env as specified in requirements (not mod_env)
            Kill('chimaera_start_runtime', PsshExecInfo(
                hostfile=self.jarvis.hostfile,
                env=self.env
            )).run()
            
            # Wait for daemon to finish if it exists
            if self.daemon_process is not None:
                self.daemon_process.wait()
                
            self.log("Chimaera runtime processes killed on all nodes")
            
        except Exception as e:
            self.log(f"Error killing Chimaera runtime: {e}")
            raise
    
    def clean(self):
        """
        Clean up package data and temporary files
        """
        try:
            self.log("Cleaning Chimaera runtime data")
            
            # Remove configuration file from shared directory
            if self.config_file_path and os.path.exists(self.config_file_path):
                os.unlink(self.config_file_path)
                self.log(f"Removed configuration file: {self.config_file_path}")
            
            # Clean up log files if they're temporary
            log_file = self.config.get('log_file', '')
            if log_file.startswith('/tmp/') and os.path.exists(log_file):
                try:
                    os.unlink(log_file)
                    self.log(f"Removed temporary log file: {log_file}")
                except:
                    pass
                    
            self.log("Chimaera runtime cleanup completed")
            
        except Exception as e:
            self.log(f"Error during cleanup: {e}")
            # Don't raise exception in cleanup
    
    def status(self) -> str:
        """
        Return current package status
        """
        try:
            # Check if daemon process is still running
            if self.daemon_process is not None:
                # Check if the process is still active
                if hasattr(self.daemon_process, 'poll'):
                    if self.daemon_process.poll() is None:
                        return "running"
                    else:
                        return "stopped"
                else:
                    return "running"  # Assume running if we can't check
            else:
                return "stopped"
        except:
            return "unknown"