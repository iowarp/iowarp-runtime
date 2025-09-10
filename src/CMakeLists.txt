# src/CMakeLists.txt
# Main Chimaera library source code

cmake_minimum_required(VERSION 3.10)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/chimods/admin/include)

# Get all source files
file(GLOB_RECURSE CHIMAERA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Create main library
add_library(cxx SHARED ${CHIMAERA_SOURCES})

# Add runtime definition
# Removed CHIMAERA_RUNTIME compile definition - using runtime IsRuntime() checks instead

# Link libraries
target_link_libraries(cxx
  PUBLIC
    hshm::cxx
    cereal::cereal
    Boost::fiber
    Boost::context
    # chimods_admin_client  # Admin client library for global task routing - TODO: Enable when admin chimod is built
)

# Set target properties
set_target_properties(cxx PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  PUBLIC_HEADER "${CMAKE_SOURCE_DIR}/include/chimaera/chimaera.h"
)

# Export target for parent scope
set_target_properties(cxx PROPERTIES EXPORT_NAME cxx)

# Create namespace alias for external consumption  
add_library(${CHIMAERA_NAMESPACE}::cxx ALIAS cxx)