#------------------------------------------------------------------------------
# Build BDEV Task Library
#------------------------------------------------------------------------------
add_library(bdev SHARED
        bdev.cc
)
add_chimaera_run_deps(bdev)
target_link_libraries(bdev Chimaera::runtime)
#------------------------------------------------------------------------------
# Install BDEV Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        bdev
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)
install(FILES bdev_monitor.py
        DESTINATION ${CHIMEARA_INSTALL_LIB_DIR})

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(bdev)
endif()
