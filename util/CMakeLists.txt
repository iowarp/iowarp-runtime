# util/CMakeLists.txt
# Utility executables for Chimaera runtime

cmake_minimum_required(VERSION 3.10)

# Build chimaera_start_runtime
add_executable(chimaera_start_runtime
  ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_start_runtime.cc
)
target_compile_definitions(chimaera_start_runtime PRIVATE CHIMAERA_RUNTIME=1)
target_link_libraries(chimaera_start_runtime
  PUBLIC chimaera
)

# Build chimaera_stop_runtime  
add_executable(chimaera_stop_runtime
  ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_stop_runtime.cc
)
# chimaera_stop_runtime is client-only code, so do not define CHIMAERA_RUNTIME
target_link_libraries(chimaera_stop_runtime
  PUBLIC chimaera
)
target_include_directories(chimaera_stop_runtime PRIVATE
  ${CMAKE_SOURCE_DIR}/chimods/admin/include
)

# Install utilities
install(TARGETS chimaera_start_runtime chimaera_stop_runtime
  RUNTIME DESTINATION bin
)