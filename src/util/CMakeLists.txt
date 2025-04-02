# ------------------------------------------------------------------------------
# Build Chimaera Runtime Start Function
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_CUDA)
  add_cuda_executable(chimaera_start_runtime TRUE chimaera_start_runtime.cc)
elseif(CHIMAERA_ENABLE_ROCM)
  add_rocm_gpu_executable(chimaera_start_runtime TRUE chimaera_start_runtime.cc)
else()
  add_executable(chimaera_start_runtime chimaera_start_runtime.cc)
endif()

add_dependencies(chimaera_start_runtime chimaera_runtime)
target_link_libraries(chimaera_start_runtime PUBLIC chimaera_runtime)

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Stop Function
# ------------------------------------------------------------------------------
add_executable(chimaera_stop_runtime chimaera_stop_runtime.cc)
add_dependencies(chimaera_stop_runtime chimaera::client_host)
target_link_libraries(chimaera_stop_runtime PUBLIC chimaera::client_host)

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Stat Function
# ------------------------------------------------------------------------------
add_executable(chimaera_stat_runtime chimaera_stat_runtime.cc)
add_dependencies(chimaera_stat_runtime chimaera::client_host)
target_link_libraries(chimaera_stat_runtime PUBLIC chimaera::client_host)

# -----------------------------------------------------------------------------
# Add file(s) to CMake Install
# -----------------------------------------------------------------------------
install(
  FILES
  ${CHIMAERA_HEADERS}
  DESTINATION
  ${CHIMAERA_INSTALL_INCLUDE_DIR}
  COMPONENT
  headers
)

# -----------------------------------------------------------------------------
# Add Target(s) to CMake Install
# -----------------------------------------------------------------------------
set(CHIMAERA_EXPORTED_LIBS
  chimaera_start_runtime
  chimaera_stop_runtime
  chimaera_stat_runtime)

install(
  TARGETS
  ${CHIMAERA_EXPORTED_LIBS}
  EXPORT
  ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR}
)

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
  set_coverage_flags(chimaera_start_runtime)
  set_coverage_flags(chimaera_stat_runtime)
  set_coverage_flags(chimaera_stop_runtime)
endif()