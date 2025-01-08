#------------------------------------------------------------------------------
# Build Chimaera Client Library
#------------------------------------------------------------------------------
add_library(chimaera_client SHARED
        config_client.cc
        config_server.cc
        chimaera_client.cc
        module.cc
        task.cc
)
target_link_libraries(chimaera_client client_deps)
add_library(Chimaera::client ALIAS chimaera_client)
add_library(client INTERFACE)
target_link_libraries(client INTERFACE chimaera_client)
#------------------------------------------------------------------------------
# Build Chimaera Runtime Library
#------------------------------------------------------------------------------
add_library(chimaera_runtime
        config_client.cc
        config_server.cc
        chimaera_client.cc
        module.cc
        task.cc
        module_registry.cc
        work_orchestrator.cc
        chimaera_runtime.cc
        python_wrapper.cc
        reinforce_worker.cc
        worker.cc
)
add_dependencies(chimaera_runtime ${Chimaera_CLIENT_DEPS})
target_link_libraries(chimaera_runtime runtime_deps)
add_library(Chimaera::runtime ALIAS chimaera_runtime)
add_library(runtime INTERFACE)
target_link_libraries(runtime INTERFACE chimaera_runtime)
#------------------------------------------------------------------------------
# Build Chimaera Runtime Start Function
#------------------------------------------------------------------------------
add_executable(chimaera_start_runtime
        chimaera_start_runtime.cc)
add_dependencies(chimaera_start_runtime ${Chimaera_RUNTIME_DEPS})
target_link_libraries(chimaera_start_runtime chimaera_runtime
        ${ALLOCATOR_LIBRARIES})

#------------------------------------------------------------------------------
# Build Chimaera Runtime Stop Function
#------------------------------------------------------------------------------
add_executable(chimaera_stop_runtime chimaera_stop_runtime.cc)
add_dependencies(chimaera_stop_runtime ${Chimaera_CLIENT_DEPS})
target_link_libraries(chimaera_stop_runtime chimaera_client
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
set(CHIMAERA_EXPORTED_LIBS
    client 
    runtime
    chimaera_client
    chimaera_runtime
    chimaera_start_runtime
    chimaera_stop_runtime
    runtime_deps
    client_deps)

install(
  TARGETS
    ${CHIMAERA_EXPORTED_LIBS}
  EXPORT
    ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)
install(
        FILES
        chimaera_monitor.py
        DESTINATION
        ${CHIMEARA_INSTALL_LIB_DIR}
        COMPONENT
        headers
)

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
  set_coverage_flags(chimaera_client)
  set_coverage_flags(chimaera_runtime)
  set_coverage_flags(chimaera_start_runtime)
  set_coverage_flags(chimaera_stop_runtime)
endif()