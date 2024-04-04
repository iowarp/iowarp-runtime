#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(small_message SHARED
        small_message.cc)
add_dependencies(small_message ${Chimaera_RUNTIME_DEPS})
target_link_libraries(small_message ${Chimaera_RUNTIME_LIBRARIES})

#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        small_message
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
        small_message
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
    set_coverage_flags(small_message)
endif()
