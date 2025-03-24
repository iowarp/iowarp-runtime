# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${MOD_NAMESPACE}_MOD_NAME_exports)

# ------------------------------------------------------------------------------
# Build MOD_NAME Task Library
# ------------------------------------------------------------------------------
add_chimod_runtime_lib(${MOD_NAMESPACE} MOD_NAME MOD_NAME_runtime.cc)
add_chimod_client_lib(${MOD_NAMESPACE} MOD_NAME MOD_NAME_client.cc)

# ------------------------------------------------------------------------------
# Install MOD_NAME Task Library
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
install(FILES MOD_NAME_monitor.py
        DESTINATION ${CHIMAERA_INSTALL_LIB_DIR})

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
        set_coverage_flags(MOD_NAME)
endif()
