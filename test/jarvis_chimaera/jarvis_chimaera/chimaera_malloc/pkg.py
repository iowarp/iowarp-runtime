"""
This module provides classes and methods to inject the HermesMpiio interceptor.
HermesMpiio intercepts the MPI I/O calls used by a native MPI program and
routes it to Hermes.
"""
from jarvis_cd.basic.pkg import Interceptor
from jarvis_util import *


class ChimaeraMalloc(Interceptor):
    """
    This class provides methods to inject the HermesMpiio interceptor.
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
        return []

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        self.env['CHI_MALLOC'] = self.find_library('chimaera_malloc')
        if self.env['CHI_MALLOC'] is None:
            raise Exception('Could not find hermes_mpi')
        print(f'Found libchimaera_malloc.so at {self.env["CHI_MALLOC"]}')

    def modify_env(self):
        """
        Modify the jarvis environment.

        :return: None
        """
        self.append_env('LD_PRELOAD', self.env['CHI_MALLOC'])
