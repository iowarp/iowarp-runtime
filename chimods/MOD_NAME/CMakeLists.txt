cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for MOD_NAME
# Automatically reads namespace from chimaera_repo.yaml (searches up directory tree)
# This creates the following targets:
#   - chimods_MOD_NAME_runtime: Server-side execution logic (shared library)
#   - chimods_MOD_NAME_client:  Client-side API for user processes (shared library)
add_chimod_both(
  CHIMOD_NAME MOD_NAME
  RUNTIME_SOURCES src/MOD_NAME_runtime.cc
  CLIENT_SOURCES src/MOD_NAME_client.cc
)

# Get target names for additional configuration
get_property(RUNTIME_TARGET GLOBAL PROPERTY MOD_NAME_RUNTIME_TARGET)
get_property(CLIENT_TARGET GLOBAL PROPERTY MOD_NAME_CLIENT_TARGET)

# Add admin module headers to include path for MOD_NAME runtime and client
if(TARGET ${RUNTIME_TARGET})
  target_include_directories(${RUNTIME_TARGET} PUBLIC
    ${CMAKE_SOURCE_DIR}/chimods/admin/include
  )
endif()

if(TARGET ${CLIENT_TARGET})
  target_include_directories(${CLIENT_TARGET} PUBLIC
    ${CMAKE_SOURCE_DIR}/chimods/admin/include
  )
endif()

# Install the ChiMod libraries
# Automatically finds and installs chimods_MOD_NAME_runtime and chimods_MOD_NAME_client targets
install_chimod(
  CHIMOD_NAME MOD_NAME
)