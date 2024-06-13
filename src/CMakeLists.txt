#------------------------------------------------------------------------------
# External dependencies
#------------------------------------------------------------------------------
# None for now

#------------------------------------------------------------------------------
# Build Chimaera Client Library
#------------------------------------------------------------------------------
add_library(chimaera_client SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/config_client.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/config_server.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_client.cc
        ../benchmark/test_latency.cc)
target_link_libraries(chimaera_client
  ${Chimaera_CLIENT_LIBRARIES_})

#------------------------------------------------------------------------------
# Build Chimaera Runtime Library
#------------------------------------------------------------------------------
add_library(chimaera_runtime
        work_orchestrator.cc
        chimaera_runtime.cc
        ${ALLOCATOR_LIBRARIES})
add_dependencies(chimaera_runtime ${Chimaera_CLIENT_DEPS})
target_link_libraries(chimaera_runtime ${Chimaera_RUNTIME_LIBRARIES_})
target_compile_definitions(chimaera_runtime PRIVATE ${Chimaera_RUNTIME_DEFINITIONS})
#------------------------------------------------------------------------------
# Build Chimaera Runtime Start Function
#------------------------------------------------------------------------------
add_executable(chimaera_start_runtime
        chimaera_start_runtime.cc)
add_dependencies(chimaera_start_runtime ${Chimaera_RUNTIME_DEPS})
target_link_libraries(chimaera_start_runtime ${Chimaera_RUNTIME_LIBRARIES}
        ${ALLOCATOR_LIBRARIES})

#------------------------------------------------------------------------------
# Build Chimaera Runtime Stop Function
#------------------------------------------------------------------------------
add_executable(chimaera_stop_runtime chimaera_stop_runtime.cc)
add_dependencies(chimaera_stop_runtime ${Chimaera_CLIENT_DEPS})
target_link_libraries(chimaera_stop_runtime ${Chimaera_CLIENT_LIBRARIES}
        ${ALLOCATOR_LIBRARIES})

#-----------------------------------------------------------------------------
# Add file(s) to CMake Install
#-----------------------------------------------------------------------------
install(
  FILES
    ${CHIMAERA_HEADERS}
  DESTINATION
    ${CHIMEARA_INSTALL_INCLUDE_DIR}
  COMPONENT
    headers
)

#-----------------------------------------------------------------------------
# Add Target(s) to CMake Install
#-----------------------------------------------------------------------------
install(
  TARGETS
    chimaera_client
    chimaera_runtime
    chimaera_start_runtime
    chimaera_stop_runtime
  EXPORT
  ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)

#-----------------------------------------------------------------------------
# Add Target(s) to CMake Install for import into other projects
#-----------------------------------------------------------------------------
install(
        EXPORT
        ${CHIMAERA_EXPORTED_TARGETS}
        DESTINATION
        ${CHIMEARA_INSTALL_DATA_DIR}/cmake/hermes
        FILE
        ${CHIMAERA_EXPORTED_TARGETS}.cmake
)

#-----------------------------------------------------------------------------
# Export all exported targets to the build tree for use by parent project
#-----------------------------------------------------------------------------
set(CHIMAERA_EXPORTED_LIBS
        chimaera_client
        chimaera_runtime
        chimaera_start_runtime
        chimaera_stop_runtime
        ${CHIMAERA_EXPORTED_LIBS})
if(NOT CHIMAERA_EXTERNALLY_CONFIGURED)
  EXPORT (
          TARGETS
          ${CHIMAERA_EXPORTED_LIBS}
          FILE
          ${CHIMAERA_EXPORTED_TARGETS}.cmake
  )
endif()

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
  set_coverage_flags(chimaera_client)
  set_coverage_flags(chimaera_runtime)
  set_coverage_flags(chimaera_start_runtime)
  set_coverage_flags(chimaera_stop_runtime)
endif()