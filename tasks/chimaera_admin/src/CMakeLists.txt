#------------------------------------------------------------------------------
# Build Hrun Admin Task Library
#------------------------------------------------------------------------------
add_library(chimaera_admin SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_admin.cc)
add_dependencies(chimaera_admin ${Chimaera_RUNTIME_DEPS})
target_link_libraries(chimaera_admin ${Chimaera_RUNTIME_LIBRARIES})

#------------------------------------------------------------------------------
# Install Hrun Admin Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        chimaera_admin
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
        chimaera_admin
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
    set_coverage_flags(chimaera_admin)
endif()
