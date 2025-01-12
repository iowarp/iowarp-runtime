#-----------------------------------------------------------------------------
# Find all packages needed by Chimaera
#-----------------------------------------------------------------------------
# This is for compatability with SPACK
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# HermesShm
find_package(HermesShm CONFIG REQUIRED)
message(STATUS "found hermes_shm.h at ${HermesShm_INCLUDE_DIRS}")

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

# Zeromq
if (BUILD_ZeroMQ_TESTS)
    pkg_check_modules(ZMQ REQUIRED libzmq)
    include_directories(${ZMQ_INCLUDE_DIRS})
    message("Found libzmq at: ${ZMQ_INCLUDE_DIRS}")
endif()

# Boost
find_package(Boost REQUIRED COMPONENTS regex system filesystem fiber REQUIRED)
if (Boost_FOUND)
    message(STATUS "found boost at ${Boost_INCLUDE_DIRS}")
endif()

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
# GPU Support Code
#-----------------------------------------------------------------------------

# ENABLE GPU SUPPORT
if (CHIMAERA_ENABLE_CUDA)
    hermes_enable_cuda(17)
endif()
if (CHIMAERA_ENABLE_ROCM)
    hermes_enable_rocm("HIP" 17)
endif()

# Create a function to make libraries for chimaera
macro(add_chigpu_library namespace target)
    set(${namespace}_${target}_exports)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${target}_gpu TRUE ${ARGN})
        target_link_libraries(${target}_gpu PUBLIC HermesShm::cudacxx)
        target_compile_definitions(${target}_gpu PUBLIC CHIMAERA_ENABLE_CUDA)
        list(APPEND ${namespace}_${target}_exports ${target}_gpu)

        add_library(${target}_host ALIAS ${target}_gpu)
        set(${target}_host_alias ON)
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_library(${target}_gpu TRUE ${ARGN})
        target_link_libraries(${target}_gpu PUBLIC HermesShm::rocmcxx_gpu)
        target_compile_definitions(${target}_gpu PUBLIC CHIMAERA_ENABLE_ROCM)
        list(APPEND ${namespace}_${target}_exports ${target}_gpu)

        add_rocm_host_library(${target}_host TRUE ${ARGN})
        target_link_libraries(${target}_host PUBLIC HermesShm::rocmcxx_host)
        list(APPEND ${namespace}_${target}_exports ${target}_host)
    else()
        add_library(${target}_gpu ${ARGN})
        target_link_libraries(${target}_gpu PUBLIC HermesShm::cxx)
        list(APPEND ${namespace}_${target}_exports ${target}_gpu)

        add_library(${target}_host ALIAS ${target}_gpu)
        set(${target}_host_alias ON)
    endif()

    if (CHIMAERA_IS_MAIN_PROJECT)
        add_library(${namespace}::${target}_gpu ALIAS ${target}_gpu)
    endif()

    if (CHIMAERA_IS_MAIN_PROJECT)
        if (NOT ${target}_host_alias)
            add_library(${namespace}::${target}_host ALIAS ${target}_host)
        else ()
            add_library(${namespace}::${target}_host ALIAS ${target}_gpu)
        endif()
    endif()
endmacro()

macro(add_chigpu_dependencies namespace target) 
    if (NOT ${target}_host_alias)
        add_dependencies(${target}_host ${ARGN})
    endif()
    add_dependencies(${target}_gpu ${ARGN})
endmacro()

macro(target_chigpu_link_libraries namespace target)
    if (NOT ${target}_host_alias)
        target_link_libraries(${target}_host PUBLIC ${ARGN})
    endif()
    target_link_libraries(${target}_gpu PUBLIC ${ARGN})
endmacro()

macro(target_chigpu_compile_definitions namespace target flag)
    if (NOT ${target}_host_alias)
        target_compile_definitions(${target}_host ${flag} ${ARGN})
    endif()
    target_compile_definitions(${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_include_directories namespace target flag)
    if (NOT ${target}_host_alias)
        target_include_directories(${target}_host ${flag} ${ARGN})
    endif()
    target_include_directories(${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_link_directories namespace target flag)
    if (NOT ${target}_host_alias)
        target_link_directories(${target}_host ${flag} ${ARGN})
    endif()
    target_link_directories(${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_compile_options target flag)
    if (NOT ${target}_host_alias)
        target_compile_options(${target}_host ${flag} ${ARGN})
    endif()
    target_compile_options(${target}_gpu ${flag} ${ARGN})
endmacro()

# Create an executable 
macro(add_chigpu_executable target)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_executable(${target} TRUE ${ARGN})
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_host_executable(${target} ${ARGN})
    else()
        add_executable(${target} ${ARGN})
    endif()
endmacro()

# Create a chimod library
macro(add_chimod_library namespace target)
    add_chigpu_library(${namespace} ${target} ${ARGN})
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_chigpu_dependencies(${namespace} ${target} chimaera::runtime_host)
    endif()
    target_chigpu_link_libraries(${namespace} ${target} chimaera::runtime_host)
endmacro()
