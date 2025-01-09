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
macro(add_chigpu_library target)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_library(${target} ${ARGN})
        target_link_libraries(${target} PUBLIC HermesShm::cudacxx)
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_library(${target} ${ARGN})
        target_link_libraries(${target} PUBLIC HermesShm::rocmcxx)
    else()
        add_library(${target} ${ARGN})
        target_link_libraries(${target} PUBLIC HermesShm::cxx)
    endif()
endmacro()

# Create an executable 
macro(add_chigpu_executable target)
    if (CHIMAERA_ENABLE_CUDA)
        add_cuda_executable(${target} ${ARGN})
    elseif (CHIMAERA_ENABLE_ROCM)
        add_rocm_executable(${target} ${ARGN})
    else()
        add_library(${target} ${ARGN})
    endif()
endmacro()

# Create a chimod library
macro(add_chimod_library target)
    add_chigpu_library(${target} ${ARGN})
    if (CHIMAERA_IS_MAIN_PROJECT)
        add_dependencies(${target} Chimaera::runtime)
    endif()
    target_link_libraries(${target} PUBLIC Chimaera::runtime)
endmacro()