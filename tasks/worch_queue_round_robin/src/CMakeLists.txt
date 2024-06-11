#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(worch_queue_round_robin SHARED
        worch_queue_round_robin.cc)
add_dependencies(worch_queue_round_robin ${Chimaera_RUNTIME_DEPS})
target_link_libraries(worch_queue_round_robin ${Chimaera_RUNTIME_LIBRARIES})
target_compile_definitions(worch_queue_round_robin PRIVATE ${Chimaera_RUNTIME_DEFINITIONS})
#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        worch_queue_round_robin
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
        worch_queue_round_robin
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
    set_coverage_flags(worch_queue_round_robin)
endif()
