cmake_minimum_required(VERSION 3.10)
project(chimaera VERSION 1.0.0)

# Read namespace from chimaera_repo.yaml in project root (function defined in ChimaeraCommon.cmake)
# This will be called after the utilities are included below

#------------------------------------------------------------------------------
# SECTION 1: OPTIONS
#------------------------------------------------------------------------------
option(CHIMAERA_ENABLE_CMAKE_DOTENV "Enable reading environment variables from .env.cmake" ON)
option(CHIMAERA_ENABLE_TESTS "Enable testing" ON)

# Read environment variables from .env.cmake if enabled
if(CHIMAERA_ENABLE_CMAKE_DOTENV)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.env.cmake")
    include("${CMAKE_SOURCE_DIR}/.env.cmake")
  endif()
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile commands export
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable RPATH
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

#------------------------------------------------------------------------------
# SECTION 2: COMPILER OPTIMIZATION
#------------------------------------------------------------------------------
# Add compiler flags following Google C++ style guide
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
# Disable some problematic warnings for external dependencies
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-unused-variable")

# Debug configuration
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Release configuration
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")


#------------------------------------------------------------------------------
# SECTION 3: CHIMOD CMAKE UTILITIES AND DEPENDENCIES
#------------------------------------------------------------------------------
# Include common utilities and dependencies early so find_package calls are available
include(cmake/ChimaeraCommon.cmake)

#------------------------------------------------------------------------------
# SECTION 4: DEPENDENCY VERIFICATION
#------------------------------------------------------------------------------
# Verify that required dependencies were found by ChimaeraCommon.cmake
if(NOT HermesShm_FOUND)
  message(FATAL_ERROR "Hermes SHM (HSHM) not found")
endif()
message(STATUS "found hermes_shm at ${HermesShm_PREFIX}")

# Note: ZeroMQ support disabled for stub implementation

#------------------------------------------------------------------------------
# SECTION 5: SOURCE COMPILATION
#------------------------------------------------------------------------------
# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${HermesShm_INCLUDE_DIRS})

# Add source subdirectory to build main library
add_subdirectory(src)

# Set output directories for binaries and libraries
# Put all outputs in bin directory for easier access
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Read namespace from chimaera_repo.yaml in project root
read_repo_namespace(CHIMAERA_NAMESPACE "${CMAKE_SOURCE_DIR}")
# Set namespace as cache variable so it's available to all CMakeLists.txt
set(CHIMAERA_NAMESPACE "${CHIMAERA_NAMESPACE}" CACHE STRING "Project namespace for chimod targets")
message(STATUS "Chimaera namespace: ${CHIMAERA_NAMESPACE}")

# Add chimods subdirectory
add_subdirectory(chimods)

# Add utilities subdirectory
add_subdirectory(util)

# Add test subdirectory if testing is enabled
if(CHIMAERA_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(test)
  message(STATUS "Unit tests enabled - added test subdirectory")
endif()

#------------------------------------------------------------------------------
# SECTION 6: INSTALL CODE
#------------------------------------------------------------------------------
# Install library
install(TARGETS chimaera
  EXPORT ChimaeraTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  PUBLIC_HEADER DESTINATION include/chimaera
)

# Install headers
install(DIRECTORY include/chimaera
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)

# Install executables (handled by util/CMakeLists.txt)

# Install config files
install(DIRECTORY config/
  DESTINATION etc/chimaera
  FILES_MATCHING PATTERN "*"
)

# Create and install export targets
install(EXPORT ChimaeraTargets
  FILE ChimaeraTargets.cmake
  NAMESPACE Chimaera::
  DESTINATION lib/cmake/chimaera
)

# Configure and install config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/ChimaeraConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ChimaeraConfig.cmake
  INSTALL_DESTINATION lib/cmake/chimaera
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ChimaeraConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ChimaeraConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ChimaeraConfigVersion.cmake
  cmake/ChimaeraCommon.cmake
  DESTINATION lib/cmake/chimaera
)