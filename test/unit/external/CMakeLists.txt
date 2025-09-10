cmake_minimum_required(VERSION 3.20)
project(external_chimod_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the specific ChiMod packages we need
# Each module is now its own package based on namespace from chimaera_repo.yaml
find_package(chimaera-MOD_NAME REQUIRED)
find_package(chimaera-admin REQUIRED)
find_package(chimaera-core REQUIRED)

# Create executable
add_executable(external_mod_name_test
  external_mod_name_test.cc
)

# Link against ChiMod libraries using the dynamic namespace system
# The namespace comes from chimaera_repo.yaml, targets are automatically created
target_link_libraries(external_mod_name_test
  chimaera::MOD_NAME_client     # MOD_NAME client library 
  chimaera::admin_client        # Admin client library (required for all ChiMod clients)
  chimaera::cxx            # Main chimaera library
)

# Removed CHIMAERA_CLIENT compile definition - using runtime IsClient() checks instead

message(STATUS "External ChiMod test configured")
message(STATUS "  Target: external_mod_name_test")
message(STATUS "  Links to: chimaera::MOD_NAME_client, chimaera::admin_client, chimaera::cxx")