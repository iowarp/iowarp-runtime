# ------------------------------------------------------------------------------
# Build Small Message Task Library
# ------------------------------------------------------------------------------
add_chimod_library(chimaera worch_queue_round_robin worch_queue_round_robin.cc)

# ------------------------------------------------------------------------------
# Install Small Message Task Library
# ------------------------------------------------------------------------------
install(
    TARGETS
    ${chimaera_worch_queue_round_robin_exports}
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
    set_coverage_flags(worch_queue_round_robin)
endif()
