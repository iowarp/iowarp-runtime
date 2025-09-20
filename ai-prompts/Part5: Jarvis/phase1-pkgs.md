@CLAUDE.md Build a series of jarvis packages for deploying this repo. Read @doc/jarvis/jarvis-pkg-dev.md
to see how.

## wrp_start_runtime

A Service type package. Contains all parameters necessary to build the chimaera configuration.

The path to the generated chimaera configuration should be stored in the environment variable RuntimeInit and ClientInit 
check if no path is provided to wrp_start_runtime.

Use PsshExecInfo to launch the runtime on all nodes in the provided hostfile. 

## wrp_stop_runtime

No parameters.

Use LocalExecInfo to stop the runtime. Only local is needed.
