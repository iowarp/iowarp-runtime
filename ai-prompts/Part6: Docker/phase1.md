@CLAUDE.md We want to build a docker container for starting the runtime and give an example docker compose file.

The container should accept the following parameters:
1. A runtime configuration, in the format of @config/chimaera_default.yaml
2. A hostfile containing the ip addresses of hosts where containers may be spawned
3. Total shared memory size

The container should do:
1. Set the correct environment variables
2. Execute chi_start_runtime

Base this container code on jarvis @test/jarvis_wrp_runtime/jarvis_wrp_runtime/wrp_runtime/pkg.py

The container is intended to be a daemon.