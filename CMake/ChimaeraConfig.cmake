# Find Chimaera header and library.

# -----------------------------------------------------------------------------
# Define constants
# -----------------------------------------------------------------------------
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
option(CHIMAERA_ENABLE_ROCM "Enable ROCm support" @CHIMAERA_ENABLE_ROCM@)
option(CHIMAERA_ENABLE_CUDA "Enable CUDA support" @CHIMAERA_ENABLE_CUDA@)

set(CHIMAERA_PREFIX @CMAKE_INSTALL_PREFIX@)
set(CHIMAERA_LIB_DIR @CHIMAERA_INSTALL_LIB_DIR@)
set(CHIMAERA_INCLUDE_DIR @CHIMAERA_INSTALL_INCLUDE_DIR@)
set(CHIMAERA_BIN_DIR @CHIMAERA_INSTALL_BIN_DIR@)

set(Chimaera_PREFIX ${CHIMAERA_PREFIX})
set(Chimaera_LIB_DIR ${CHIMAERA_LIB_DIR})
set(Chimaera_INCLUDE_DIR ${CHIMAERA_INCLUDE_DIR})
set(Chimaera_BIN_DIR ${CHIMAERA_BIN_DIR})

# Find the Chimaera Package
include(@CMAKE_INSTALL_PREFIX@/cmake/ChimaeraCoreConfig.cmake)
include(@CMAKE_INSTALL_PREFIX@/cmake/ChimaeraCommonConfig.cmake)
include_directories("@CHIMAERA_INSTALL_INCLUDE_DIR@")
link_directories("@CHIMAERA_INSTALL_LIB_DIR@")

# Add my library to RPATH
list(APPEND CMAKE_INSTALL_RPATH "@CHIMAERA_INSTALL_LIB_DIR@")
