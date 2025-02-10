# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${MOD_NAMESPACE}_TASK_NAME_exports)

# ------------------------------------------------------------------------------
# Build TASK_NAME Task Library
# ------------------------------------------------------------------------------
add_chimod_library(${MOD_NAMESPACE} TASK_NAME TASK_NAME.cc)

# ------------------------------------------------------------------------------
# Install TASK_NAME Task Library
# ------------------------------------------------------------------------------
install(
        TARGETS
        ${${MOD_EXPORTS}}
        EXPORT
        ${CHIMAERA_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR}
)
install(FILES TASK_NAME_monitor.py
        DESTINATION ${CHIMAERA_INSTALL_LIB_DIR})

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
        set_coverage_flags(TASK_NAME)
endif()
