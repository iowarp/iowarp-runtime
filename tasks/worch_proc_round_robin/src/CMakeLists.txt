# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_worch_proc_round_robin_exports)

# ------------------------------------------------------------------------------
# Build Small Message Task Library
# ------------------------------------------------------------------------------
add_chimod_runtime_lib(${REPO_NAMESPACE} worch_proc_round_robin worch_proc_round_robin_runtime.cc)
add_chimod_client_lib(${REPO_NAMESPACE} worch_proc_round_robin worch_proc_round_robin_client.cc)

# ------------------------------------------------------------------------------
# Install Small Message Task Library
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
    set_coverage_flags(worch_proc_round_robin)
endif()
