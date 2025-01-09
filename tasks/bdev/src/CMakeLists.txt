#------------------------------------------------------------------------------
# Build BDEV Task Library
#------------------------------------------------------------------------------
add_chimod_library(bdev bdev.cc)
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
