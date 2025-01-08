#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
include_directories(myapp PRIVATE ${libfabric_INCLUDE_DIRS})
link_directories(${libfabric_LIBRARY_DIRS})
add_library(remote_queue SHARED
        remote_queue.cc)
add_chimaera_run_deps(remote_queue)
target_link_libraries(remote_queue
        chimaera_runtime ${libfabric_LIBRARIES})
target_compile_definitions(remote_queue PRIVATE ${Chimaera_RUNTIME_DEFINITIONS})
#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        remote_queue
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(remote_queue)
endif()
