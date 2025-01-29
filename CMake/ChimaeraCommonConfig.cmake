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
message("CHIMAERA_ENABLE_CUDA: ${CHIMAERA_ENABLE_CUDA}")
if (CHIMAERA_ENABLE_CUDA)
    hshm_enable_cuda(17)
endif()
if (CHIMAERA_ENABLE_ROCM)
    hshm_enable_rocm("HIP" 17)
endif()

# Create a chimod library
macro(add_chimod_library namespace target)
    # Create the loadable chimod runtime library
    set(${namespace}_${target}_exports)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cudacxx)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_CUDA)
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_gpu_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::rocmcxx_gpu)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_ROCM)
    else()
        add_library(${namespace}_${target} ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cxx)
    endif()
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_library(${namespace}::${target} ALIAS ${namespace}_${target})
        add_dependencies(${namespace}_${target} chimaera::runtime)
    endif()
    target_link_libraries(${namespace}_${target} PUBLIC chimaera::runtime)
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target})
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE ${namespace}_${target})
    list(APPEND ${namespace}_${target}_exports ${target})

    # Add chimod library no gpu client interface
    add_library(${namespace}_${target}_client INTERFACE)
    target_link_libraries(${namespace}_${target}_client INTERFACE chimaera::client)
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client)
    add_library(${target}_client INTERFACE)
    target_link_libraries(${target}_client INTERFACE ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_exports ${target}_client)

    # Add chimod library with cuda support
    if (CHIMAERA_ENABLE_CUDA)
        add_library(${namespace}_${target}_client_gpu INTERFACE)
        target_link_libraries(${namespace}_${target}_client_gpu INTERFACE hshm::cudacxx chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)

        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()

    # Add chimod library with rocm support
    if (CHIMAERA_ENABLE_ROCM)
        add_library(${namespace}_${target}_client_gpu INTERFACE)
        target_link_libraries(${namespace}_${target}_client_gpu INTERFACE hshm::rocmcxx_gpu chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)

        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()
endmacro()
