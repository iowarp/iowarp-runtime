# ------------------------------------------------------------------------------
# Build Small Message Task Library
# ------------------------------------------------------------------------------
add_chimod_library(chimaera remote_queue remote_queue_runtime.cc)

# ------------------------------------------------------------------------------
# Install Small Message Task Library
# ------------------------------------------------------------------------------
install(
    TARGETS
    ${chimaera_remote_queue_exports}
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
    set_coverage_flags(remote_queue)
endif()
