# -----------------------------------------------------------------------------
# Find all packages needed by Chimaera
# -----------------------------------------------------------------------------
# This is for compatability with SPACK
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# HermesShm
find_package(HermesShm CONFIG REQUIRED)
message(STATUS "found hermes_shm.h at ${HSHM_INSTALL_INCLUDE_DIR}")

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
if(BUILD_ZeroMQ_TESTS)
    pkg_check_modules(ZMQ REQUIRED libzmq)
    include_directories(${ZMQ_INCLUDE_DIRS})
    message("Found libzmq at: ${ZMQ_INCLUDE_DIRS}")
endif()

# Boost
find_package(Boost REQUIRED COMPONENTS regex system filesystem fiber REQUIRED)

if(Boost_FOUND)
    message(STATUS "found boost at ${Boost_INCLUDE_DIRS}")
endif()

# Choose an allocator
set(ALLOCATOR_LIBRARIES "")

# jemalloc
if(CHIMAERA_ENABLE_JEMALLOC)
    pkg_check_modules(JEMALLOC jemalloc)
    pkg_search_module(JEMALLOC REQUIRED jemalloc)
    include_directories(${JEMALLOC_INCLUDE_DIRS})
    link_directories(${JEMALLOC_LIBRARY_DIRS})
    set(ALLOCATOR_LIBRARIES ${JEMALLOC_LIBRARIES})
endif()

# mimmalloc
if(CHIMAERA_ENABLE_MIMALLOC)
    find_package(mimalloc REQUIRED)

    if(mimalloc_FOUND)
        message(STATUS "found mimalloc at ${mimalloc_DIR}")
    endif()

    set(ALLOCATOR_LIBRARIES mimalloc)
endif()

# Pybind11
if(CHIMAERA_ENABLE_PYTHON)
    find_first_path_python()
    find_package(pybind11 REQUIRED)
    set(OPTIONAL_LIBS pybind11::embed)
endif()

# -----------------------------------------------------------------------------
# GPU Support Code
# -----------------------------------------------------------------------------

# ENABLE GPU SUPPORT
if(CHIMAERA_ENABLE_CUDA)
    hshm_enable_cuda(17)
endif()

if(CHIMAERA_ENABLE_ROCM)
    hshm_enable_rocm("HIP" 17)
endif()

# Create a chimod runtime library
# Runtime Library Names: namespace_target
macro(add_chimod_runtime_lib namespace target)
    # Create the loadable chimod runtime library
    set(${namespace}_${target}_exports)
    set(${namespace}_${target}_runtime_iter)
    set(${namespace}_${target}_client_iter)
    set(${namespace}_${target}_iter)

    # Create the ${namespace}_${target} library
    if(CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cudacxx)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_CUDA)
    elseif(CHIMAERA_ENABLE_ROCM)
        add_rocm_gpu_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::rocmcxx_gpu)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_ROCM)
    else()
        add_library(${namespace}_${target} ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cxx)
    endif()

    # Add the runtime library to the main project
    add_library(${namespace}::${target} ALIAS ${namespace}_${target})

    if(CHIMAERA_IS_MAIN_PROJECT)
        add_dependencies(${namespace}_${target} chimaera::runtime)
    endif()

    # Link the runtime library to the chimaera runtime
    target_link_libraries(${namespace}_${target} PUBLIC chimaera::runtime)
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target})
    list(APPEND ${namespace}_${target}_runtime_iter ${namespace}_${target})

    # Create the ${target} interface
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE ${namespace}_${target})
    list(APPEND ${namespace}_${target}_exports ${target})
endmacro()

# Create a chimod client library
# Client Libraries: namespace_target_client, target_client
# GPU Client Libraries: namespace_target_client_gpu, target_client_gpu
macro(add_chimod_client_lib namespace target)
    # Create the ${namespace}_${target}_client library
    add_library(${namespace}_${target}_client ${ARGN})
    target_link_libraries(${namespace}_${target}_client PUBLIC chimaera::client_host)
    target_include_directories(${namespace}_${target}_client PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    )
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_client)

    # Create the ${target}_client interface
    add_library(${target}_client INTERFACE)
    target_link_libraries(${target}_client INTERFACE ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_exports ${target}_client)

    # Add chimod library with cuda support
    if(CHIMAERA_ENABLE_CUDA)
        # Create the ${namespace}_${target}_client_gpu library
        add_library(${namespace}_${target}_client_gpu ${ARGN})
        target_link_libraries(${namespace}_${target}_client_gpu PUBLIC hshm::cudacxx chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
        )
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_client_gpu)

        # Create the ${target}_client_gpu interface
        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()

    # Add chimod library with rocm support
    if(CHIMAERA_ENABLE_ROCM)
        # Create the ${namespace}_${target}_client_gpu library
        add_library(${namespace}_${target}_client_gpu ${ARGN})
        target_link_libraries(${namespace}_${target}_client_gpu PUBLIC hshm::rocmcxx_gpu chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
        )
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_gpu)

        # Create the ${target}_client_gpu interface
        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()

    # Create the full iterator
    set(${namespace}_${target}_iter ${${namespace}_${target}_runtime_iter} ${${namespace}_${target}_client_iter})
endmacro()

# Create a chimod library
# Runtime Library Names: namespace_target
# Client Libraries: namespace_target_client, target_client
# GPU Client Libraries: namespace_target_client_gpu, target_client_gpu
macro(add_chimod_library namespace target)
    # Create the loadable chimod runtime library
    set(${namespace}_${target}_exports)
    set(${namespace}_${target}_client_iter)
    set(${namespace}_${target}_runtime_iter)
    set(${namespace}_${target}_iter)

    if(CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cudacxx)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_CUDA)
    elseif(CHIMAERA_ENABLE_ROCM)
        add_rocm_gpu_library(${namespace}_${target} SHARED TRUE ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::rocmcxx_gpu)
        target_compile_definitions(${namespace}_${target} PUBLIC CHIMAERA_ENABLE_ROCM)
    else()
        add_library(${namespace}_${target} ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC hshm::cxx)
    endif()

    if(CHIMAERA_IS_MAIN_PROJECT)
        add_library(${namespace}::${target} ALIAS ${namespace}_${target})
        add_dependencies(${namespace}_${target} chimaera::runtime)
    endif()

    target_link_libraries(${namespace}_${target} PUBLIC chimaera::runtime)
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target})
    list(APPEND ${namespace}_${target}_runtime_iter ${namespace}_${target})
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE ${namespace}_${target})
    list(APPEND ${namespace}_${target}_exports ${target})

    # Add chimod library no gpu client interface
    add_library(${namespace}_${target}_client INTERFACE)
    target_link_libraries(${namespace}_${target}_client INTERFACE chimaera::client_host)
    target_include_directories(${namespace}_${target}_client INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
    list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_client)
    add_library(${target}_client INTERFACE)
    target_link_libraries(${target}_client INTERFACE ${namespace}_${target}_client)
    target_include_directories(${target}_client INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
    list(APPEND ${namespace}_${target}_exports ${target}_client)

    # Add chimod library with cuda support
    if(CHIMAERA_ENABLE_CUDA)
        add_library(${namespace}_${target}_client_gpu INTERFACE)
        target_link_libraries(${namespace}_${target}_client_gpu INTERFACE hshm::cudacxx chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_client_gpu)

        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()

    # Add chimod library with rocm support
    if(CHIMAERA_ENABLE_ROCM)
        add_library(${namespace}_${target}_client_gpu INTERFACE)
        target_link_libraries(${namespace}_${target}_client_gpu INTERFACE hshm::rocmcxx_gpu chimaera::client_gpu)
        target_include_directories(${namespace}_${target}_client_gpu INTERFACE ${CMAKE_INSTALL_PREFIX}/include)
        list(APPEND ${namespace}_${target}_exports ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_client_iter ${namespace}_${target}_gpu)

        add_library(${target}_client_gpu INTERFACE)
        target_link_libraries(${target}_client_gpu INTERFACE ${namespace}_${target}_client_gpu)
        list(APPEND ${namespace}_${target}_exports ${target}_client_gpu)
    endif()

    # Create the full iterator
    set(${namespace}_${target}_iter ${${namespace}_${target}_runtime_iter} ${${namespace}_${target}_client_iter})
endmacro()
