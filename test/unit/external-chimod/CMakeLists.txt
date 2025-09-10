cmake_minimum_required(VERSION 3.20)
project(external_chimod_test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Load environment variables from .env.cmake if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env.cmake")
  message(STATUS "Loading environment from ${CMAKE_CURRENT_SOURCE_DIR}/.env.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/.env.cmake")
endif()

# Find required Chimaera packages
find_package(chimaera-core REQUIRED)
find_package(chimaera-admin REQUIRED)

# ChimaeraCommon.cmake utilities are included via chimaera-core package

# Add modules directory
add_subdirectory(modules/simple_mod)

# Create test executable
add_executable(test_simple_mod test_simple_mod.cc)

# Link against required libraries
target_link_libraries(test_simple_mod
  external_test::simple_mod_client        # Our ChiMod client
  chimaera::chimaera_admin_client         # Admin client (required)
  chimaera::cxx                           # Main Chimaera library
  ${CMAKE_THREAD_LIBS_INIT}               # Threading support
)

# Include directories
target_include_directories(test_simple_mod PUBLIC
  ${CMAKE_SOURCE_DIR}/modules/simple_mod/include
)

# Enable testing
enable_testing()
add_test(NAME external_chimod_test COMMAND test_simple_mod)