"""
This module provides classes and methods to launch the HermesRun service.
chimaera_codegen is ....
"""

from jarvis_cd.basic.pkg import Service, Color
from jarvis_util import *


class ChimaeraRun(Service):
    """
    This class provides methods to launch the HermesRun service.
    """
    def _init(self):
        """
        Initialize paths
        """
        self.daemon_pkg = None
        self.hostfile_path = f'{self.shared_dir}/hostfile'
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
                'name': 'num_nodes',
                'msg': 'Number of nodes to run chimaera_codegen on. 0 means all',
                'type': int,
                'default': 0,
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'data_shm',
                'msg': 'Data buffering space',
                'type': str,
                'default': '8g',
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'rdata_shm',
                'msg': 'Runtime data buffering space',
                'type': str,
                'default': '8g',
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'task_shm',
                'msg': 'Task buffering space',
                'type': str,
                'default': '0g',
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'shm_name',
                'msg': 'The base shared-memory name',
                'type': str,
                'default': 'chimaera_shm_${USER}',
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'port',
                'msg': 'The port to listen for data on',
                'type': int,
                'default': 8080,
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'provider',
                'msg': 'The libfabric provider type to use (e.g., sockets)',
                'type': str,
                'default': None,
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'domain',
                'msg': 'The libfabric domain to use (e.g., lo)',
                'type': str,
                'default': None,
                'class': 'communication',
                'rank': 1,
            },
            {
                'name': 'rpc_cpus',
                'msg': 'the mapping of rpc threads to cpus',
                'type': list,
                'default': None,
                'class': 'communication',
                'rank': 1,
                'args': [
                    {
                        'name': 'cpu_id',
                        'msg': 'An integer representing CPU ID',
                        'type': int,
                    }
                ],
            },
            {
                'name': 'qdepth',
                'msg': 'The depth of queues',
                'type': int,
                'default': 100000,
                'class': 'queuing',
                'rank': 1,
            },
            {
                'name': 'pqdepth',
                'msg': 'The depth of the process queue',
                'type': int,
                'default': 48,
                'class': 'queuing',
                'rank': 1,
            },
            {
                'name': 'qlanes',
                'msg': 'The number of lanes per queue',
                'type': int,
                'default': 4,
                'class': 'queuing',
                'rank': 1,
            },
            {
                'name': 'worker_cpus',
                'msg': 'the mapping of workers to cpu cores',
                'type': list,
                'default': None,
                'class': 'work orchestrator',
                'rank': 1,
                'args': [
                    {
                        'name': 'cpu_id',
                        'msg': 'An integer representing CPU ID',
                        'type': int,
                    }
                ],
            },
            {
                'name': 'reinforce_cpu',
                'msg': 'the mapping of the reinforce worker to cpu',
                'type': int,
                'default': 3,
                'class': 'work orchestrator',
                'rank': 1,
            },
            {
                'name': 'monitor_window',
                'msg': 'Amount of time to sample task models (seconds)',
                'type': int,
                'default': 1,
                'class': 'work orchestrator',
                'rank': 1,
            },
            {
                'name': 'monitor_gap',
                'msg': 'Distance between monitoring phases (seconds)',
                'type': int,
                'default': 5,
                'class': 'work orchestrator',
                'rank': 1,
            },
            {
                'name': 'monitor_out',
                'msg': 'Output of monitoring samples',
                'type': str,
                'default': '',
                'class': 'work orchestrator',
                'rank': 1,
            },
            {
                'name': 'modules',
                'msg': 'Output of monitoring samples',
                'type': list,
                'default': '',
                'class': 'module registry',
                'args': [
                    {
                        'name': 'mod',
                        'msg': 'The module name to be included',
                        'type': str
                    },
                ],
                'rank': 1,
            },
        ]

    def get_hostfile(self):
        self.hostfile = self.jarvis.hostfile
        if self.config['num_nodes'] > 0 and self.hostfile.path is not None:
            self.hostfile = Hostfile(hostfile=self.hostfile_path)

    def _configure(self, **kwargs):
        """
        Converts the Jarvis configuration to application-specific configuration.
        E.g., OrangeFS produces an orangefs.xml file.

        :param config: The human-readable jarvis YAML configuration for the
        application.
        :return: None
        """
        rg = self.jarvis.resource_graph

        # Create hostfile
        self.hostfile = self.jarvis.hostfile
        if self.config['num_nodes'] > 0 and self.hostfile.path is not None:
            self.hostfile = self.hostfile.subset(self.config['num_nodes'])
            self.hostfile.save(self.hostfile_path)

        # Begin making chimaera_run config
        chimaera_server = {
            'work_orchestrator': {
                'reinforce_cpu': self.config['reinforce_cpu'],
                'monitor_window': self.config['monitor_window'],
                'monitor_gap': self.config['monitor_gap']
            },
            'queue_manager': {
                'queue_depth': self.config['qdepth'],
                'proc_queue_depth': self.config['pqdepth'],
                'max_lanes': self.config['qlanes'],
                'max_queues': 1024,
                'shm_name': self.config['shm_name'],
                'shm_size': self.config['task_shm'],
                'data_shm_size': self.config['data_shm'],
                'rdata_shm_size': self.config['rdata_shm'],
            }
        }
        if self.config['worker_cpus'] is not None:
            chimaera_server['work_orchestrator']['cpus'] = self.config['worker_cpus']
        if len(self.config['monitor_out']):
            self.env['CHIMAERA_MONITOR_OUT'] = os.path.expandvars(self.config['monitor_out'])
            os.makedirs(self.env['CHIMAERA_MONITOR_OUT'], exist_ok=True)

        # Get network Info 
        net_info = rg.find_net_info(self.hostfile, strip_ips=True, 
                                    local=len(self.hostfile) == 1, env=self.env)
        provider = self.config['provider']
        if provider is None:
            opts = net_info['provider'].unique().list()
            if len(opts) == 0:
                self.log(
                    'WARNING: No networks discovered. '
                    'This can happen if you are building the pipeline on a node that is not'
                    'where you plan to run the test.'
                    'If this is the case, run `jarvis ppl update`` in your batch job before running'
                    'this pipeline.', Color.RED)
            order = ['sockets', 'tcp', 'udp', 'verbs', 'ib']
            for opt in order:
                if opt in opts:
                    provider = opt
                    break
            if provider is None:
                provider = opts[0]
        self.log(f'Provider: {provider}')
        net_info_save = net_info
        net_info = net_info[lambda r: str(r['provider']) == provider,
                            ['provider', 'domain']]
        if len(net_info) == 0:
            self.log(net_info_save)
            raise Exception(f'Failed to find chimaera_run provider {provider}')
        net_info = net_info.rows[0]
        protocol = net_info['provider']
        domain = net_info['domain']
        hostfile_path = self.hostfile.path
        if self.hostfile.path is None:
            hostfile_path = ''
            domain = ''
        if self.config['domain'] is not None:
            domain = self.config['domain']
        chimaera_server['rpc'] = {
            'host_file': hostfile_path,
            'protocol': protocol,
            'domain': domain,
            'port': self.config['port'],
        }
        if self.config['rpc_cpus'] is not None:
            chimaera_server['rpc']['cpus'] = self.config['rpc_cpus']
        if self.hostfile.path is None:
            chimaera_server['rpc']['host_names'] = self.hostfile.hosts

        # Add some initial modules to the registry
        chimaera_server['module_registry'] = self.config['modules']

        # Save Chimaera configuration
        chimaera_server_yaml = f'{self.shared_dir}/chimaera_server.yaml'
        YamlFile(chimaera_server_yaml).save(chimaera_server)
        self.env['CHIMAERA_CONF'] = chimaera_server_yaml

    def start(self):
        """
        Launch an application. E.g., OrangeFS will launch the servers, clients,
        and metadata services on all necessary pkgs.

        :return: None
        """
        self.log(self.env['CHIMAERA_CONF'])
        self.get_hostfile()
        self.daemon_pkg = Exec('chimaera_start_runtime',
                                PsshExecInfo(hostfile=self.hostfile,
                                             env=self.mod_env,
                                             exec_async=True,
                                             do_dbg=self.config['do_dbg'],
                                             dbg_port=self.config['dbg_port'],
                                             hide_output=self.config['hide_output'],
                                             pipe_stdout=self.config['stdout'],
                                             pipe_stderr=self.config['stderr']))
        time.sleep(self.config['sleep'])
        self.log('Done sleeping')

    def stop(self):
        """
        Stop a running application. E.g., OrangeFS will terminate the servers,
        clients, and metadata services.

        :return: None
        """
        self.log('Stopping chimaera_run')
        self.get_hostfile()
        Exec('chimaera_stop_runtime',
             LocalExecInfo(hostfile=self.hostfile,
                           env=self.env,
                           exec_async=False,
                           # do_dbg=self.config['do_dbg'],
                           # dbg_port=self.config['dbg_port'] + 2,
                           hide_output=self.config['hide_output']))
        self.log('Client Exited?')
        if self.daemon_pkg is not None:
            self.daemon_pkg.wait()
        self.log('Daemon Exited?')

    def kill(self):
        self.get_hostfile()
        for i in range(5):
            Kill('.*chimaera.*',
                PsshExecInfo(hostfile=self.hostfile,
                            env=self.env))
        self.log('Client Exited (killed 5 times)', Color.YELLOW)
        if self.daemon_pkg is not None:
            self.daemon_pkg.wait()
        self.log('Daemon Exited', Color.YELLOW)

    def clean(self):
        """
        Destroy all data for an application. E.g., OrangeFS will delete all
        metadata and data directories in addition to the orangefs.xml file.

        :return: None
        """
        pass

    def status(self):
        """
        Check whether or not an application is running. E.g., are OrangeFS
        servers running?

        :return: True or false
        """
        self.get_hostfile()
        stats = Exec('ps -ef | grep .*chimaera_run.*', 
             PsshExecInfo(hostfile=self.hostfile,
             env=self.env,
             collect_output=True,
             hide_output=True))
        running = []
        for host, output in stats.stdout.items():
            for line in output.splitlines():
                if 'grep' in line:
                    continue
                if 'chimaera_run' not in line:
                    continue
                self.log(f'Chimaera is running on {host}', Color.CYAN)
                running.append(host)
                break
        is_running = len(running) == len(self.hostfile)
        self.log(f'Chimaera is running on {len(running)}/{len(self.hostfile)} nodes', Color.YELLOW)
        return is_running
