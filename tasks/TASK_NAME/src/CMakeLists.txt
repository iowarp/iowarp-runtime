#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(TASK_NAME SHARED
        TASK_NAME.cc)
if ("${Chimaera_RUNTIME_DEPS}")
    add_dependencies(TASK_NAME ${Chimaera_RUNTIME_DEPS})
endif()
target_link_libraries(TASK_NAME ${Chimaera_RUNTIME_LIBRARIES})
target_compile_definitions(TASK_NAME PRIVATE ${Chimaera_RUNTIME_DEFINITIONS})
#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        TASK_NAME
        EXPORT
        ${CHIMAERA_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)
install(FILES TASK_NAME_monitor.py
        DESTINATION ${CHIMEARA_INSTALL_LIB_DIR})

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
        TASK_NAME
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
    set_coverage_flags(TASK_NAME)
endif()
