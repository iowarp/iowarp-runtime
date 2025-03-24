# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_chimaera_admin_exports)

# ------------------------------------------------------------------------------
# Build CHI Admin Task Library
# ------------------------------------------------------------------------------
add_chimod_library(${REPO_NAMESPACE} chimaera_admin ${CMAKE_CURRENT_SOURCE_DIR}/chimaera_admin_runtime.cc)

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
