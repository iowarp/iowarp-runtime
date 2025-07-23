# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_MOD_NAME_exports)

# ------------------------------------------------------------------------------
# Build MOD_NAME Task Library
# ------------------------------------------------------------------------------
add_chimod_client_lib(${REPO_NAMESPACE} MOD_NAME MOD_NAME_client.cc)
# Target for client (no gpu) chimod: ${REPO_NAMESPACE}_MOD_NAME_client
# Target for client (no gpu) for the runtime runtime: ${REPO_NAMESPACE}_MOD_NAME_client_run
# Target for client (with gpu): ${REPO_NAMESPACE}_MOD_NAME_client_gpu
# Target for client (with gpu) for the runtime: ${REPO_NAMESPACE}_MOD_NAME_client_gpu_run
add_chimod_runtime_lib(${REPO_NAMESPACE} MOD_NAME MOD_NAME_runtime.cc)
# Target for runtime chimod: ${REPO_NAMESPACE}_MOD_NAME 
# This automatically links to client (with gpu) if enabled, and client without gpu otherwise.
# CHIMAERA_ENABLE_CUDA will be set if compiled with cuda support
# CHIMAERA_ENABLE_ROCM will be set if compiled with rocm support

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
