cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for admin
# Automatically reads namespace from chimaera_repo.yaml (searches up directory tree)
# This creates the following targets:
#   - chimods_admin_runtime: Server-side execution logic (shared library)
#   - chimods_admin_client:  Client-side API for user processes (shared library)
add_chimod_both(
  CHIMOD_NAME admin
  RUNTIME_SOURCES src/admin_runtime.cc src/autogen/admin_lib_exec.cc
  CLIENT_SOURCES src/admin_client.cc
)

# Install the ChiMod libraries
# Automatically finds and installs chimods_admin_runtime and chimods_admin_client targets
install_chimod(
  CHIMOD_NAME admin
)