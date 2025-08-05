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
    message("ENABLING ROCM")
    hshm_enable_rocm("HIP" 17)
endif()

# Create a chimod runtime library
# Runtime Library Names: namespace_target
macro(add_chimod_runtime_lib_body namespace target ext)
    # Scoped names
    set(scoped_target ${namespace}_${target}${ext})
    set(unscoped_target ${target}${ext})
    set(alias_target ${namespace}::${target}${ext})

    # Create the ${namespace}_${target} library
    if(CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${scoped_target} SHARED TRUE ${ARGN})
        target_compile_definitions(${scoped_target} PUBLIC CHIMAERA_ENABLE_CUDA)
    elseif(CHIMAERA_ENABLE_ROCM)
        add_rocm_gpu_library(${scoped_target} SHARED TRUE ${ARGN})
        target_compile_definitions(${scoped_target} PUBLIC CHIMAERA_ENABLE_ROCM)
    else()
        add_library(${scoped_target} ${ARGN})
    endif()

    # Link the runtime library to the chimaera runtime
    target_link_libraries(${scoped_target} PUBLIC chimaera::runtime)
    list(APPEND ${namespace}_${target}_exports ${scoped_target})

    # Create the ${unscoped_target} interface
    add_library(${unscoped_target} INTERFACE)
    target_link_libraries(${unscoped_target} INTERFACE ${scoped_target})
    list(APPEND ${namespace}_${target}_exports ${unscoped_target})

    # Add the runtime library to the main project
    add_library(${alias_target} ALIAS ${scoped_target})

    if(CHIMAERA_IS_MAIN_PROJECT)
        add_dependencies(${scoped_target} chimaera::runtime)
    endif()
endmacro()

# Create a chimod runtime library
# Runtime Library Names: namespace_target
macro(add_chimod_runtime_lib namespace target)
    add_chimod_runtime_lib_body(${namespace} ${target} "" ${ARGN})
    if(CHIMAERA_ENABLE_CUDA)
        target_link_libraries(${scoped_target} PUBLIC ${namespace}_${target}_client_gpu_run)
    elseif(CHIMAERA_ENABLE_ROCM)
        target_link_libraries(${scoped_target} PUBLIC ${namespace}_${target}_client_gpu_run)
    else()
        target_link_libraries(${scoped_target} PUBLIC ${namespace}_${target}_client_run)
    endif()
endmacro()

# Create chimod client lib header-only
macro(add_chimod_client_lib_interface namespace target ext chi_lib)
    # Scoped names
    set(scoped_target ${namespace}_${target}${ext})
    set(unscoped_target ${target}${ext})
    set(alias_target ${namespace}::${target}${ext})

    # Add scoped target
    add_library(${scoped_target} ${libtype} INTERFACE)

    if(NOT ${chi_lib} STREQUAL "")
        target_link_libraries(${scoped_target} INTERFACE ${chi_lib})
    endif()

    target_include_directories(${scoped_target}
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        INTERFACE $<INSTALL_INTERFACE:include>
    )
    list(APPEND ${namespace}_${target}_exports ${scoped_target})

    # Create the ${unscoped_target} interface
    add_library(${unscoped_target} INTERFACE)
    target_link_libraries(${unscoped_target} INTERFACE ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_exports ${unscoped_target})

    # Create the ${alias_target} alias
    add_library(${alias_target} ALIAS ${scoped_target})
endmacro()

# Create chimod client lib for host
macro(add_chimod_client_lib_host namespace target libtype ext chi_lib)
    # Scoped names
    set(scoped_target ${namespace}_${target}${ext})
    set(unscoped_target ${target}${ext})
    set(alias_target ${namespace}::${target}${ext})

    # Add scoped target
    add_library(${scoped_target} ${libtype} ${ARGN})
    target_link_libraries(${scoped_target} PUBLIC ${chi_lib})
    target_include_directories(${scoped_target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        PUBLIC $<INSTALL_INTERFACE:include>
    )
    list(APPEND ${namespace}_${target}_exports ${scoped_target})

    # Create the ${unscoped_target} interface
    add_library(${unscoped_target} INTERFACE)
    target_link_libraries(${unscoped_target} INTERFACE ${namespace}_${target}_client)
    list(APPEND ${namespace}_${target}_exports ${unscoped_target})

    # Create the ${alias_target} alias
    add_library(${alias_target} ALIAS ${scoped_target})
endmacro()

# Create chimod client lib for cuda
macro(add_chimod_client_lib_cuda namespace target libtype ext chi_lib)
    # Scoped names
    set(scoped_target ${namespace}_${target}${ext})
    set(unscoped_target ${target}${ext})
    set(alias_target ${namespace}::${target}${ext})

    # Add scoped target
    add_cuda_library(${scoped_target} ${libtype} TRUE ${ARGN})
    target_link_libraries(${scoped_target} PUBLIC ${chi_lib})
    target_include_directories(${scoped_target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        PUBLIC $<INSTALL_INTERFACE:include>
    )
    list(APPEND ${namespace}_${target}_exports ${scoped_target})

    # Create the ${target}_client_gpu interface
    add_library(${unscoped_target} INTERFACE)
    target_link_libraries(${unscoped_target} INTERFACE ${scoped_target})
    list(APPEND ${namespace}_${target}_exports ${unscoped_target})

    # Create the ${namespace}::${target}_client alias
    add_library(${alias_target} ALIAS ${namespace}_${target}_client)
endmacro()

# Create chimod client lib for rocm
macro(add_chimod_client_lib_rocm namespace target libtype ext chi_lib)
    # Scoped names
    set(scoped_target ${namespace}_${target}${ext})
    set(unscoped_target ${target}${ext})
    set(alias_target ${namespace}::${target}${ext})

    # Create the ${scoped_target} library
    add_rocm_gpu_library(${scoped_target} ${libtype} TRUE ${ARGN})
    target_link_libraries(${scoped_target} PUBLIC ${chi_lib})
    target_include_directories(${scoped_target}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        PUBLIC $<INSTALL_INTERFACE:include>
    )
    list(APPEND ${namespace}_${target}_exports ${scoped_target})

    # Create the ${unscoped_target} interface
    add_library(${unscoped_target} INTERFACE)
    target_link_libraries(${unscoped_target} INTERFACE ${scoped_target})
    list(APPEND ${namespace}_${target}_exports ${unscoped_target})

    # Create the ${namespace}::${target}_client alias
    add_library(${alias_target} ALIAS ${namespace}_${target}_client)
endmacro()

# Create a chimod client library
# Client Libraries: namespace_target_client, target_client
# GPU Client Libraries: namespace_target_client_gpu, target_client_gpu
macro(add_chimod_client_lib namespace target)
    # add_chimod_client_lib_interface(${namespace} ${target} _client chimaera::client_host)
    # add_chimod_client_lib_interface(${namespace} ${target} _client_run "")

    # if(CHIMAERA_ENABLE_CUDA)
    # # Add chimod library with cuda support
    # add_chimod_client_lib_interface(${namespace} ${target} _client_gpu chimaera::client_gpu)
    # add_chimod_client_lib_interface(${namespace} ${target} _client_gpu_run "")
    # endif()

    # if(CHIMAERA_ENABLE_ROCM)
    # # Add chimod library with rocm support
    # add_chimod_client_lib_interface(${namespace} ${target} _client_gpu chimaera::client_gpu)
    # add_chimod_client_lib_interface(${namespace} ${target} _client_gpu_run "")
    # endif()

    # Create the ${namespace}_${target}_client library
    add_chimod_client_lib_host(${namespace} ${target} SHARED _client chimaera::client_host ${ARGN})
    add_chimod_runtime_lib_body(${namespace} ${target} _client_run ${ARGN})

    if(CHIMAERA_ENABLE_CUDA)
        # Add chimod library with cuda support
        add_chimod_client_lib_cuda(${namespace} ${target} STATIC _client_gpu chimaera::client_gpu ${ARGN})
        add_chimod_runtime_lib_body(${namespace} ${target} _client_gpu_run ${ARGN})
    endif()

    if(CHIMAERA_ENABLE_ROCM)
        # Add chimod library with rocm support
        add_chimod_client_lib_rocm(${namespace} ${target} STATIC _client_gpu chimaera::client_gpu ${ARGN})
        add_chimod_runtime_lib_body(${namespace} ${target} _client_gpu_run ${ARGN})
    endif()
endmacro()
