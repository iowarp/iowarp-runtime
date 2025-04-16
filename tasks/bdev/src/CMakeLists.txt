# ------------------------------------------------------------------------------
# Set variables
# ------------------------------------------------------------------------------
set(MOD_EXPORTS ${REPO_NAMESPACE}_bdev_exports)

# ------------------------------------------------------------------------------
# Build BDEV Task Library
# ------------------------------------------------------------------------------
add_chimod_runtime_lib(${REPO_NAMESPACE} bdev bdev_runtime.cc)
add_chimod_client_lib(${REPO_NAMESPACE} bdev bdev_client.cc)

foreach(target IN LISTS ${REPO_NAMESPACE}_bdev_runtime_iter)
        target_link_libraries(${target} PUBLIC cufile)
endforeach()

# ------------------------------------------------------------------------------
# Install BDEV Task Library
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
install(FILES bdev_monitor.py
        DESTINATION ${CHIMAERA_INSTALL_LIB_DIR})

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
        set_coverage_flags(bdev)
endif()
