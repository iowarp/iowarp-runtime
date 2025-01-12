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


# Create function to add dependencies to a target
if (CHIMAERA_IS_MAIN_PROJECT)
    function(add_chimaera_run_deps target)
        add_dependencies(${target} chimaera_client chimaera_runtime)
    endfunction()
else()
    function(add_chimaera_run_deps target)
    endfunction()
endif()

# Create a function to make libraries for chimaera
macro(add_chigpu_library namespace target)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${namespace}_${target}_gpu TRUE ${ARGN})
        target_link_libraries(${namespace}_${target}_gpu PUBLIC HermesShm::cudacxx)
        target_compile_definitions(${namespace}_${target}_gpu PUBLIC CHIMAERA_ENABLE_CUDA)

        add_library(${namespace}_${target}_host INTERFACE)
        target_link_libraries(${namespace}_${target}_host INTERFACE ${namespace}_${target}_gpu)
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_library(${namespace}_${target}_gpu TRUE ${ARGN})
        target_link_libraries(${namespace}_${target}_gpu PUBLIC HermesShm::rocmcxx_gpu)
        target_compile_definitions(${namespace}_${target}_gpu PUBLIC CHIMAERA_ENABLE_ROCM)

        add_rocm_host_library(${namespace}_${target}_host TRUE ${ARGN})
        target_link_libraries(${namespace}_${target}_host PUBLIC HermesShm::rocmcxx_host)
    else()
        add_library(${namespace}_${target}_host ${ARGN})
        target_link_libraries(${namespace}_${target} PUBLIC HermesShm::cxx)

        add_library(${namespace}_${target}_gpu INTERFACE)
        target_link_libraries(${namespace}_${target}_gpu INTERFACE ${namespace}_${target}_host)
    endif()

    add_library(${target}_gpu INTERFACE)
    target_link_libraries(${target}_gpu INTERFACE ${namespace}_${target}_gpu)
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_library(${namespace}::${target}_gpu ALIAS ${namespace}_${target}_gpu)
    endif()

    add_library(${target}_host INTERFACE)
    target_link_libraries(${target}_host INTERFACE ${namespace}_${target}_host)
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_library(${namespace}::${target}_host ALIAS ${namespace}_${target}_host)
    endif()
endmacro()

macro(add_chigpu_dependencies namespace target)
    add_dependencies(${namespace}_${target}_host ${ARGN})
    add_dependencies(${namespace}_${target}_gpu ${ARGN})
endmacro()

macro(target_chigpu_link_libraries namespace target)
    target_link_libraries(${namespace}_${target}_host PUBLIC ${ARGN})
    target_link_libraries(${namespace}_${target}_gpu PUBLIC ${ARGN})
endmacro()

macro(target_chigpu_compile_definitions namespace target flag)
    target_compile_definitions(${namespace}_${target}_host ${flag} ${ARGN})
    target_compile_definitions(${namespace}_${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_include_directories namespace target flag)
    target_include_directories(${namespace}_${target}_host ${flag} ${ARGN})
    target_include_directories(${namespace}_${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_link_directories namespace target flag)
    target_link_directories(${namespace}_${target}_host ${flag} ${ARGN})
    target_link_directories(${namespace}_${target}_gpu ${flag} ${ARGN})
endmacro()

macro(target_chigpu_compile_options target flag)
    target_compile_options(${namespace}_${target}_host ${flag} ${ARGN})
    target_compile_options(${namespace}_${target}_gpu ${flag} ${ARGN})
endmacro()

# Create an executable 
macro(add_chigpu_executable target)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_executable(${target} TRUE ${ARGN})
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_executable(${target} TRUE ${ARGN})
    else()
        add_library(${target} ${ARGN})
    endif()
endmacro()

# Create a chimod library
macro(add_chimod_library namespace target)
    add_chigpu_library(${namespace} ${target} ${ARGN})
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_chigpu_dependencies(${namespace} ${target} chimaera::runtime_host)
    endif()
    target_chigpu_link_libraries(${namespace} ${target} chimaera::runtime_host)
    set(${namespace}_${host}_exports 
        ${namespace}_${target}_host
        ${namespace}_${target}_gpu
        ${target}_host
        ${target}_gpu)
endmacro()

# 

# Install chimod library
macro(install_chimod_library namespace target export)
    install(TARGETS 
                ${namespace}_${target}_host
                ${namespace}_${target}_gpu
                ${target}_host
                ${target}_gpu
            EXPORT ${export}
            LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
            ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
            RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR})
endmacro()