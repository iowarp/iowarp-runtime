# ------------------------------------------------------------------------------
# Build Chimaera Malloc Interceptor
# ------------------------------------------------------------------------------
add_library(chimaera_malloc chimaera_malloc.cc)
add_dependencies(chimaera_malloc chimaera::client_host)
target_link_libraries(chimaera_malloc PUBLIC chimaera::client_host)

# -----------------------------------------------------------------------------
# Add Target(s) to CMake Install
# -----------------------------------------------------------------------------
set(CHIMAERA_EXPORTED_LIBS
  chimaera_malloc)

install(
  TARGETS
  ${CHIMAERA_EXPORTED_LIBS}
  EXPORT
  ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR}
)
install(
  FILES
  chimaera_monitor.py
  DESTINATION
  ${CHIMAERA_INSTALL_LIB_DIR}
  COMPONENT
  headers
)

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
  set_coverage_flags(chimaera_malloc)
endif()