#------------------------------------------------------------------------------
# Build Chimaera Client Library
#------------------------------------------------------------------------------
set(CLIENT_SOURCES
        config_client.cc
        config_server.cc
        module.cc
        task.cc
)
add_chigpu_library(chimaera client ${CLIENT_SOURCES})

#------------------------------------------------------------------------------
# Build Chimaera Runtime Library
#------------------------------------------------------------------------------
set(RUNTIME_SOURCES 
        config_client.cc
        config_server.cc
        module.cc
        task.cc
        module_registry.cc
        work_orchestrator.cc
        chimaera_runtime.cc
        python_wrapper.cc
        reinforce_worker.cc
        worker.cc
        queue_manager.cc
)
add_chigpu_library(chimaera runtime ${RUNTIME_SOURCES})
target_chigpu_link_libraries(chimaera runtime PUBLIC runtime_deps)
target_chigpu_compile_definitions(chimaera runtime PUBLIC HSHM_DEFAULT_THREAD_MODEL=hshm::thread::Argobots)
#------------------------------------------------------------------------------
# Build Chimaera Runtime Start Function
#------------------------------------------------------------------------------
add_chigpu_executable(chimaera_start_runtime
        chimaera_start_runtime.cc)
add_dependencies(chimaera_start_runtime chimaera::runtime_gpu)
target_link_libraries(chimaera_start_runtime PUBLIC chimaera::runtime_gpu)

#------------------------------------------------------------------------------
# Build Chimaera Runtime Stop Function
#------------------------------------------------------------------------------
add_executable(chimaera_stop_runtime chimaera_stop_runtime.cc)
add_dependencies(chimaera_stop_runtime chimaera::client_host)
target_link_libraries(chimaera_stop_runtime PUBLIC chimaera::client_host)

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
    chimaera_client_host
    chimaera_client_gpu
    client_host
    client_gpu

    chimaera_runtime_host
    chimaera_runtime_gpu
    runtime_host
    runtime_gpu

    chimaera_start_runtime
    chimaera_stop_runtime
    runtime_deps)

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