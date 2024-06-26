cmake_minimum_required(VERSION 3.10)
project(chimaera)

#-----------------------------------------------------------------------------
# Define Options
#-----------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" ON)
option(BUILD_MPI_TESTS "Build tests which depend on MPI" ON)
option(BUILD_OpenMP_TESTS "Build tests which depend on OpenMP" ON)
option(BUILD_ZeroMQ_TESTS "Build tests which depend on ZeroMQ" OFF)
option(CHIMAERA_ENABLE_COVERAGE "Check how well tests cover code" OFF)
option(CHIMAERA_ENABLE_DOXYGEN "Check how well the code is documented" OFF)
option(CHIMAERA_TASK_DEBUG "Debug tasks" OFF)
option(CHIMAERA_REMOTE_DEBUG "Debug remote tasks" OFF)
option(CHIMAERA_ENABLE_JEMALLOC "Use jemalloc as the allocator" OFF)
option(CHIMAERA_ENABLE_MIMALLOC "Use mimalloc as the allocator" OFF)

#-----------------------------------------------------------------------------
# Compiler Optimization
#-----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("IN DEBUG MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    add_compile_definitions(HERMES_LOG_VERBOSITY=10)
else()
    message("IN RELEASE MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
    add_compile_definitions(HERMES_LOG_VERBOSITY=1)
endif()
add_compile_options(-march=native -fomit-frame-pointer)
if (CHIMAERA_REMOTE_DEBUG)
    add_compile_definitions(CHIMAERA_REMOTE_DEBUG)
endif()
if (CHIMAERA_TASK_DEBUG)
    message("Adding the chimaera task debug option")
    add_compile_definitions(CHIMAERA_TASK_DEBUG)
endif()

#-----------------------------------------------------------------------------
# Targets built within this project are exported at Install time for use
# by other projects.
#-----------------------------------------------------------------------------
if(NOT CHIMAERA_EXPORTED_TARGETS)
    set(CHIMAERA_EXPORTED_TARGETS "chimaera-targets")
endif()

#-----------------------------------------------------------------------------
# Find Packages
#-----------------------------------------------------------------------------
# This is for compatability with SPACK
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

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

#------------------------------------------------------------------------------
# Setup CMake Environment
#------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries.")

#------------------------------------------------------------------------------
# Setup install and output Directories
#------------------------------------------------------------------------------
if(NOT CHIMEARA_INSTALL_BIN_DIR)
    set(CHIMEARA_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()
if(NOT CHIMEARA_INSTALL_LIB_DIR)
    set(CHIMEARA_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()
if(NOT CHIMEARA_INSTALL_INCLUDE_DIR)
    set(CHIMEARA_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()
if(NOT CHIMEARA_INSTALL_DATA_DIR)
    set(CHIMEARA_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()

#-----------------------------------------------------------------------------
# Build Chimaera Main Packages
#-----------------------------------------------------------------------------
# Main includes
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# Task includes
include_directories(${CMAKE_SOURCE_DIR}/tasks)
include_directories(${CMAKE_SOURCE_DIR}/tasks/chimaera_admin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/small_message/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/remote_queue/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/worch_proc_round_robin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/worch_queue_round_robin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/proc_queue/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/bdev/include)

set(Chimaera_CLIENT_LIBRARIES_
        ${HermesShm_LIBRARIES}
        yaml-cpp
        cereal::cereal
        thallium
        -ldl -lrt -lc -pthread
        ${ALLOCATOR_LIBRARIES})
set(Chimaera_CLIENT_LIBRARIES
        ${Chimaera_CLIENT_LIBRARIES_}
        chimaera_client)
set(Chimaera_CLIENT_DEPS
        chimaera_client)
set(Chimaera_RUNTIME_LIBRARIES_
        ${Chimaera_CLIENT_LIBRARIES}
        ${Boost_LIBRARIES})
set(Chimaera_RUNTIME_LIBRARIES
        ${Chimaera_RUNTIME_LIBRARIES_}
        chimaera_runtime)
set(Chimaera_RUNTIME_DEPS
        chimaera_client chimaera_runtime)
set(Chimaera_RUNTIME_DEFINITIONS
        CHIMAERA_RUNTIME)

set(TEST_MAIN ${CMAKE_SOURCE_DIR}/test/unit)
add_subdirectory(tasks)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(benchmark)

#-----------------------------------------------------------------------------
# Build + Enable Testing
#-----------------------------------------------------------------------------
add_custom_target(lint COMMAND bash ${CMAKE_SOURCE_DIR}/ci/lint.sh ${CMAKE_SOURCE_DIR})
# General function used to hook ctest to python test tool lib
function(jarvis_test test_type test_name)
    add_test(NAME ${test_name}
            COMMAND jarvis pipeline run yaml "${CMAKE_SOURCE_DIR}/test/unit/pipelines/${test_type}/${test_name}.yaml")
endfunction()

enable_testing()

#-----------------------------------------------------------------------------
# Install HRUN Headers
#-----------------------------------------------------------------------------
install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})
