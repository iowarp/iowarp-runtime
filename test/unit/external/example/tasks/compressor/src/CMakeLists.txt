# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_compressor_exports)

# ------------------------------------------------------------------------------
# Build compressor Task Library
# ------------------------------------------------------------------------------
add_chimod_runtime_lib(${REPO_NAMESPACE} compressor compressor_runtime.cc)
add_chimod_client_lib(${REPO_NAMESPACE} compressor compressor_client.cc)

# ------------------------------------------------------------------------------
# Install compressor Task Library
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
install(FILES compressor_monitor.py
        DESTINATION ${CHIMAERA_INSTALL_LIB_DIR})

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
        set_coverage_flags(compressor)
endif()
