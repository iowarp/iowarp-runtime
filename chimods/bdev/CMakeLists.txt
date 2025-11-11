cmake_minimum_required(VERSION 3.10)


# Create client library
add_chimod_client(
  SOURCES
    src/bdev_client.cc
  INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/chimods/admin/include  # BDev needs admin headers
)

# Create runtime library
add_chimod_runtime(
  SOURCES
    src/bdev_runtime.cc
    src/autogen/bdev_lib_exec.cc
  INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/chimods/admin/include  # BDev needs admin headers
)