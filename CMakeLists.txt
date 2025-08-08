cmake_minimum_required(VERSION 3.10)
project(chimaera VERSION 1.0.0)

#------------------------------------------------------------------------------
# SECTION 1: OPTIONS
#------------------------------------------------------------------------------
option(CHIMAERA_ENABLE_CMAKE_DOTENV "Enable reading environment variables from .env.cmake" ON)
option(CHIMAERA_BUILD_RUNTIME "Build runtime components" ON)
option(CHIMAERA_BUILD_CLIENT "Build client components" ON)
option(CHIMAERA_ENABLE_TESTS "Enable testing" OFF)

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

# Set runtime macro based on build type
if(CHIMAERA_BUILD_RUNTIME)
  add_definitions(-DCHIMAERA_RUNTIME=1)
else()
  add_definitions(-DCHIMAERA_RUNTIME=0)
endif()

#------------------------------------------------------------------------------
# SECTION 3: FIND_PACKAGE
#------------------------------------------------------------------------------
# Find required dependencies
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED COMPONENTS regex system filesystem fiber REQUIRED)
find_package(Threads REQUIRED)

# Find Hermes SHM (HSHM)
find_package(HermesShm CONFIG REQUIRED)
if(NOT HermesShm_FOUND)
  message(FATAL_ERROR "Hermes SHM (HSHM) not found")
endif()
message(STATUS "found hermes_shm at ${HermesShm_PREFIX}")

# Note: ZeroMQ support disabled for stub implementation

#------------------------------------------------------------------------------
# SECTION 4: SOURCE COMPILATION
#------------------------------------------------------------------------------
# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${HermesShm_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

# Create main library
file(GLOB_RECURSE CHIMAERA_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cc
  ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Create main library (no utility exclusions needed since they're in util/ now)
add_library(chimaera SHARED ${CHIMAERA_SOURCES})

# Link libraries
target_link_libraries(chimaera
  PUBLIC
    hshm::cxx
    Boost::fiber
    ${Boost_LIBRARIES}
    Threads::Threads
)

# Note: ZMQ linking disabled for stub implementation

# Set target properties
set_target_properties(chimaera PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  PUBLIC_HEADER "${CMAKE_SOURCE_DIR}/include/chimaera/chimaera.h"
)

# Set output directories for binaries and libraries
# Put all outputs in bin directory for easier access
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add utilities
if(CHIMAERA_BUILD_RUNTIME)
  # Build chimaera_start_runtime
  add_executable(chimaera_start_runtime util/chimaera_start_runtime.cc)
  target_link_libraries(chimaera_start_runtime
    PRIVATE chimaera
  )
  
  # Build chimaera_stop_runtime
  add_executable(chimaera_stop_runtime util/chimaera_stop_runtime.cc)
  target_link_libraries(chimaera_stop_runtime
    PRIVATE chimaera
  )
endif()

#------------------------------------------------------------------------------
# SECTION 5: INSTALL CODE
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

# Install executables
if(CHIMAERA_BUILD_RUNTIME)
  install(TARGETS chimaera_start_runtime chimaera_stop_runtime
    RUNTIME DESTINATION bin
  )
endif()

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
  DESTINATION lib/cmake/chimaera
)