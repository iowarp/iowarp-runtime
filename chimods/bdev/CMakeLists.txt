cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for bdev
# Automatically reads namespace from chimaera_repo.yaml (searches up directory tree)
# This creates the following targets:
#   - chimods_bdev_runtime: Server-side execution logic (shared library)
#   - chimods_bdev_client:  Client-side API for user processes (shared library)
add_chimod_both(
  CHIMOD_NAME bdev
  RUNTIME_SOURCES src/bdev_runtime.cc
  CLIENT_SOURCES src/bdev_client.cc
)

# Get target names for additional configuration
get_property(RUNTIME_TARGET GLOBAL PROPERTY bdev_RUNTIME_TARGET)
get_property(CLIENT_TARGET GLOBAL PROPERTY bdev_CLIENT_TARGET)

# Add admin module headers to include path for bdev runtime and client
if(TARGET ${RUNTIME_TARGET})
  target_include_directories(${RUNTIME_TARGET} PUBLIC
    ${CMAKE_SOURCE_DIR}/chimods/admin/include
  )
  # Add POSIX RT library dependency for async I/O
  target_link_libraries(${RUNTIME_TARGET} PRIVATE rt)
endif()

if(TARGET ${CLIENT_TARGET})
  target_include_directories(${CLIENT_TARGET} PUBLIC
    ${CMAKE_SOURCE_DIR}/chimods/admin/include
  )
endif()

# Install the ChiMod libraries
# Automatically finds and installs chimods_bdev_runtime and chimods_bdev_client targets
install_chimod(
  CHIMOD_NAME bdev
)