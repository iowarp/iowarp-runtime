# Find Chimaera header and library.

# This module defines the following uncached variables:
set(Chimaera_FOUND ON)
# Set Chimaera dirs
# Chimaera_INCLUDE_DIRS
# Chimaera_LIBRARY_DIRS
# Chimaera_CLIENT_LIBRARIES
# Chimaera_RUNTIME_LIBRARIES
# Chimaera_RUNTIME_DEFINITIONS

#-----------------------------------------------------------------------------
# Define constants
#-----------------------------------------------------------------------------
set(Chimaera_VERSION_MAJOR @Chimaera_VERSION_MAJOR@)
set(Chimaera_VERSION_MINOR @Chimaera_VERSION_MINOR@)
set(Chimaera_VERSION_PATCH @Chimaera_VERSION_PATCH@)

option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" @BUILD_SHARED_LIBS@)
option(BUILD_MPI_TESTS "Build tests which depend on MPI" @BUILD_MPI_TESTS@)
option(BUILD_OpenMP_TESTS "Build tests which depend on OpenMP" @BUILD_OpenMP_TESTS@)
option(BUILD_ZeroMQ_TESTS "Build tests which depend on ZeroMQ" @BUILD_ZeroMQ_TESTS@)
option(CHIMAERA_ENABLE_COVERAGE "Check how well tests cover code" @CHIMAERA_ENABLE_COVERAGE@)
option(CHIMAERA_ENABLE_DOXYGEN "Check how well the code is documented" @CHIMAERA_ENABLE_DOXYGEN@)
option(CHIMAERA_ENABLE_JEMALLOC "Use jemalloc as the allocator" @CHIMAERA_ENABLE_JEMALLOC@)
option(CHIMAERA_ENABLE_MIMALLOC "Use mimalloc as the allocator" @CHIMAERA_ENABLE_MIMALLOC@)
option(CHIMAERA_ENABLE_PYTHON "Use pybind11" @CHIMAERA_ENABLE_PYTHON@)

#-----------------------------------------------------------------------------
# Find Chimaera header
#-----------------------------------------------------------------------------
find_path(
  Chimaera_INCLUDE_DIR
        chimaera/chimaera_types.h
  HINTS ENV PATH ENV CPATH
)
if (NOT Chimaera_INCLUDE_DIR)
  message(STATUS "FindChimaera: Could not find Chimaera.h")
  set(Chimaera_FOUND FALSE)
  return()
endif()
get_filename_component(Chimaera_DIR ${Chimaera_INCLUDE_DIR} PATH)

#-----------------------------------------------------------------------------
# Find Chimaera library
#-----------------------------------------------------------------------------
find_library(
        Chimaera_LIBRARY
        NAMES chimaera_client
        HINTS ENV LD_LIBRARY_PATH ENV PATH
)
if (NOT Chimaera_LIBRARY)
    message(STATUS "FindChimaera: Could not find libchimaera_client.so")
    set(Chimaera_FOUND OFF)
    message(STATUS "LIBS: $ENV{LD_LIBRARY_PATH}")
    return()
endif()
set(Chimaera_LIBRARY_DIR "")
get_filename_component(Chimaera_LIBRARY_DIR ${Chimaera_LIBRARY} PATH)
message("Chimaera_LIBRARY_DIR: ${Chimaera_LIBRARY_DIR}")

#-----------------------------------------------------------------------------
# Find all packages needed by Chimaera
#-----------------------------------------------------------------------------

# HermesShm
find_package(HermesShm CONFIG REQUIRED)
message(STATUS "found hermes_shm.h at ${HermesShm_INCLUDE_DIRS}")
include_directories(${HermesShm_INCLUDE_DIRS})
link_directories(${HermesShm_LIBRARY_DIRS})

# YAML-CPP
find_package(yaml-cpp REQUIRED)
message(STATUS "found yaml-cpp at ${yaml-cpp_DIR}")

# Catch2
find_package(Catch2 3.0.1 REQUIRED)
message(STATUS "found catch2.h at ${Catch2_CXX_INCLUDE_DIRS}")

# MPICH
if(BUILD_MPI_TESTS)
    find_package(MPI REQUIRED COMPONENTS C CXX)
    message(STATUS "found mpi.h at ${MPI_CXX_INCLUDE_DIRS}")
endif()

# OpenMP
if(BUILD_OpenMP_TESTS)
    find_package(OpenMP REQUIRED COMPONENTS C CXX)
    message(STATUS "found omp.h at ${OpenMP_CXX_INCLUDE_DIRS}")
endif()

# Cereal
find_package(cereal REQUIRED)
if(cereal)
    message(STATUS "found cereal")
endif()

# Pkg-Config
find_package(PkgConfig REQUIRED)
if(PkgConfig)
    message(STATUS "found pkg config")
endif()

# Zeromq
if (BUILD_ZeroMQ_TESTS)
    pkg_check_modules(ZMQ REQUIRED libzmq)
    include_directories(${ZMQ_INCLUDE_DIRS})
    message("Found libzmq at: ${ZMQ_INCLUDE_DIRS}")
endif()

# Thallium
find_package(thallium CONFIG REQUIRED)
if(thallium_FOUND)
    message(STATUS "found thallium at ${thallium_DIR}")
endif()

# Boost
find_package(Boost REQUIRED COMPONENTS regex system filesystem fiber REQUIRED)
if (Boost_FOUND)
    message(STATUS "found boost at ${Boost_INCLUDE_DIRS}")
endif()
include_directories(${Boost_INCLUDE_DIRS})

# Choose an allocator
set(ALLOCATOR_LIBRARIES "")

# jemalloc
if (CHIMAERA_ENABLE_JEMALLOC)
    pkg_check_modules (JEMALLOC jemalloc)
    pkg_search_module(JEMALLOC REQUIRED jemalloc)
    include_directories(${JEMALLOC_INCLUDE_DIRS})
    link_directories(${JEMALLOC_LIBRARY_DIRS})
    set(ALLOCATOR_LIBRARIES ${JEMALLOC_LIBRARIES})
endif()

# mimmalloc
if (CHIMAERA_ENABLE_MIMALLOC)
    find_package(mimalloc REQUIRED)
    if (mimalloc_FOUND)
        message(STATUS "found mimalloc at ${mimalloc_DIR}")
    endif()
    set(ALLOCATOR_LIBRARIES mimalloc)
endif()

# Pybind11
if (CHIMAERA_ENABLE_PYTHON)
    find_package(pybind11 REQUIRED)
    set(OPTIONAL_LIBS pybind11::embed)
endif()

#-----------------------------------------------------------------------------
# Mark Chimaera as found and set all needed packages
#-----------------------------------------------------------------------------
set(Chimaera_FOUND ON)
set(Chimaera_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} ${Chimaera_INCLUDE_DIR})
set(Chimaera_LIBRARY_DIRS
        ${Boost_LIBRARY_DIRS}
        ${HermesShm_LIBRARY_DIRS}
        ${Chimaera_LIBRARY_DIR})
set(_Chimaera_CLIENT_LIBRARIES
        ${HermesShm_LIBRARIES}
        yaml-cpp
        cereal::cereal
        thallium
        -ldl -lrt -lc -pthread
        ${ALLOCATOR_LIBRARIES})
set(Chimaera_CLIENT_LIBRARIES
        ${_Chimaera_CLIENT_LIBRARIES}
        chimaera_client)
set(Chimaera_CLIENT_LIBRARY_DIRS ${Chimaera_LIBRARY_DIRS})
set(_Chimaera_RUNTIME_LIBRARIES
        ${_Chimaera_CLIENT_LIBRARIES}
        ${Boost_LIBRARIES}
        ${OPTIONAL_LIBS}
)
set(Chimaera_RUNTIME_LIBRARIES
        ${_Chimaera_RUNTIME_LIBRARIES}
        chimaera_runtime)
set(Chimaera_RUNTIME_DEFINITIONS
        CHIMAERA_RUNTIME)
if (CHIMAERA_ENABLE_PYTHON)
    set(Chimaera_RUNTIME_DEFINITIONS
            ${Chimaera_RUNTIME_DEFINITIONS}
            CHIMAERA_ENABLE_PYTHON)
endif()
set(Chimaera_RUNTIME_DEPS "")
