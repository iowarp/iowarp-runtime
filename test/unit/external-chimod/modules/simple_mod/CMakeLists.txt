cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for simple_mod
# Automatically reads namespace from chimaera_repo.yaml (searches up directory tree)
# This creates the following targets:
#   - external_test_simple_mod_runtime: Server-side execution logic (shared library)
#   - external_test_simple_mod_client:  Client-side API for user processes (shared library)
add_chimod_both(
  CHIMOD_NAME simple_mod
  RUNTIME_SOURCES src/simple_mod_runtime.cc src/autogen/simple_mod_lib_exec.cc
  CLIENT_SOURCES src/simple_mod_client.cc
)

# Install the ChiMod libraries
# Automatically finds and installs external_test_simple_mod_runtime and external_test_simple_mod_client targets
install_chimod(
  CHIMOD_NAME simple_mod
)