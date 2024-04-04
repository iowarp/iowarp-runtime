"""
This module provides classes and methods to launch the LabstorIpcTest application.
LabstorIpcTest is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class ChimaeraBwBench(Application):
    """
    This class provides methods to launch the LabstorIpcTest application.
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
                'name': 'nprocs',
                'msg': 'The number of processes to spawn',
                'type': int,
                'default': None,
            },
            {
                'name': 'ppn',
                'msg': 'The number of processes per node',
                'type': int,
                'default': 1,
            },
            {
                'name': 'async',
                'msg': 'Use async tasks',
                'type': bool,
            },
            {
                'name': 'msg_size',
                'msg': 'The # operations to generate per node',
                'type': str,
                'default': '4k',
            },
            {
                'name': 'ops',
                'msg': 'The # operations to generate per node',
                'type': str,
                'default': '4k',
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        pass

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        nprocs = self.config['nprocs']
        if self.config['nprocs'] is None:
            nprocs = len(self.jarvis.hostfile)
        if self.config['async']:
            do_async = '1'
        else:
            do_async = '0'
        cmd = [
            'bench_chimaera_bw',
            str(self.config['msg_size']),
            self.config['ops'],
            do_async,
        ]
        cmd = ' '.join(cmd)
        Exec(cmd,
             MpiExecInfo(hostfile=self.jarvis.hostfile,
                         nprocs=nprocs,
                         ppn=self.config['ppn'],
                         env=self.env,
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
        pass
