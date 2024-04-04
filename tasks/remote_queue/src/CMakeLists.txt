#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
include_directories(myapp PRIVATE ${libfabric_INCLUDE_DIRS})
link_directories(${libfabric_LIBRARY_DIRS})
add_library(remote_queue SHARED
        remote_queue.cc)
add_dependencies(remote_queue ${Chimaera_RUNTIME_DEPS})
target_link_libraries(remote_queue
        chimaera_runtime ${libfabric_LIBRARIES})

#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        remote_queue
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
        remote_queue
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
    set_coverage_flags(remote_queue)
endif()
