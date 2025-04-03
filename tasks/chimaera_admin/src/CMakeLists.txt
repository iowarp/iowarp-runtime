# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_chimaera_admin_exports)

# ------------------------------------------------------------------------------
# Build CHI Admin Task Library
# ------------------------------------------------------------------------------
add_chimod_runtime_lib(${REPO_NAMESPACE} chimaera_admin chimaera_admin_runtime.cc)
add_chimod_client_lib(${REPO_NAMESPACE} chimaera_admin chimaera_admin_client.cc)

# ------------------------------------------------------------------------------
# Install CHI Admin Task Library
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

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(chimaera_admin)
endif()
