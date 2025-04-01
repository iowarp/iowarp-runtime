"""
This module provides classes and methods to launch the LabstorIpcTest application.
LabstorIpcTest is ....
"""
from jarvis_cd.basic.pkg import Application
from jarvis_util import *


class ChimaeraIoBench(Application):
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
                'name': 'path',
                'msg': 'Path to load',
                'type': str,
                'default': '$HOME/test_chi/test.bin',
            },
            {
                'name': 'xfer',
                'msg': 'Transfer size',
                'type': str,
                'default': '1m',
            },
            {
                'name': 'block',
                'msg': 'Size per process',
                'type': str,
                'default': '64m',
            },
            {
                'name': 'do_read',
                'msg': 'Whether to do read or not',
                'type': bool,
                'default':  True,
            },
        ]

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param kwargs: Configuration parameters for this pkg.
        :return: None
        """
        path = os.path.expandvars(self.config['path'])
        parent_dir = os.path.dirname(path)
        Mkdir(parent_dir)
        self.log(f'Created directory: {parent_dir}')

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        nprocs = self.config['nprocs']
        if self.config['nprocs'] is None:
            nprocs = len(self.jarvis.hostfile)
        path = os.path.expandvars(self.config['path'])
        cmd = [
            'chimaera_io_bench',
            path,
            self.config['xfer'],
            self.config['block'],
            str(int(self.config['do_read'])), 
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

    def kill(self):
        """
        Kills this
        """
        for i in range(3):
            Kill('.*chimaera_bdev_io.*', PsshExecInfo(hostfile=self.jarvis.hostfile))

    def clean(self):
        """
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        path = os.path.expandvars(self.config['path'])
        Rm(path + '*')
