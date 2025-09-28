@CLAUDE.md Build a jarvis package for deploying this repo. Read @docs/jarvis/package_dev_guide.md
to see how. Create the jarvis repo in a new directory test/jarvis_wrp_runtime. 

## wrp_runtime

A Service type package. Contains all parameters necessary to build the chimaera configuration.

The path to the generated chimaera configuration should be stored in the environment variable RuntimeInit and ClientInit check. Store in the shared directory.

Use PsshExecInfo to launch the runtime on all nodes in the provided hostfile. Use env for the 
environment, not mod_env.
