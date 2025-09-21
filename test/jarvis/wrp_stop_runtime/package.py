"""
WRP Stop Runtime Jarvis Package

Simple package for stopping local Chimaera runtime with no parameters.
Uses LocalExecInfo for local runtime stopping.
"""

import os
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class WrpStopRuntime(Application):
    """
    Application package for stopping local Chimaera runtime
    """
    
    def _init(self):
        """
        Initialize the application package
        """
        pass
    
    def _configure_menu(self):
        """
        Define the configuration menu - no parameters needed for stop
        """
        return [
            {
                'name': 'stop_binary',
                'msg': 'Path to the chimaera_stop_runtime binary',
                'type': str,
                'default': '${HOME}/iowarp-runtime/build/bin/chimaera_stop_runtime'
            },
            {
                'name': 'grace_period_ms',
                'msg': 'Grace period in milliseconds for shutdown (optional)',
                'type': int,
                'default': 5000
            }
        ]
    
    def _configure(self, **kwargs):
        """
        Configure the stop runtime application with provided parameters
        """
        self.update_config(kwargs, rebuild=False)
    
    def start(self):
        """
        Stop the local Chimaera runtime
        """
        stop_binary = self.config["stop_binary"]
        grace_period_ms = self.config["grace_period_ms"]
        
        if not os.path.exists(stop_binary):
            raise FileNotFoundError(f"Stop runtime binary not found: {stop_binary}")
        
        self.log(f"Stopping local Chimaera runtime using: {stop_binary}")
        self.log(f"Grace period: {grace_period_ms}ms")
        
        # Use LocalExecInfo for local execution only
        Exec(f"{stop_binary} {grace_period_ms}", LocalExecInfo(env=self.env))
        
        self.log("Local Chimaera runtime stopped successfully")
    
    def stop(self):
        """
        No-op for stop operation since this package only stops runtime
        """
        self.log("WrpStopRuntime stop() called - no operation needed")