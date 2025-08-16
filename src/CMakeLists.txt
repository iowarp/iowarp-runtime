# src/CMakeLists.txt
# Main Chimaera library source code

cmake_minimum_required(VERSION 3.10)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Get all source files
file(GLOB_RECURSE CHIMAERA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Create main library
add_library(chimaera SHARED ${CHIMAERA_SOURCES})

# Add runtime definition
target_compile_definitions(chimaera PRIVATE CHIMAERA_RUNTIME=1)

# Link libraries
target_link_libraries(chimaera
  PUBLIC
    hshm::cxx
    cereal::cereal
    Boost::fiber
    Boost::context
)

# Set target properties
set_target_properties(chimaera PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 1
  PUBLIC_HEADER "${CMAKE_SOURCE_DIR}/include/chimaera/chimaera.h"
)

# Export target for parent scope
set_target_properties(chimaera PROPERTIES EXPORT_NAME chimaera)