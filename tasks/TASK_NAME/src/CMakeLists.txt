#------------------------------------------------------------------------------
# Build TASK_NAME Task Library
#------------------------------------------------------------------------------
add_library(TASK_NAME SHARED
        TASK_NAME.cc)
add_chimaera_run_deps(TASK_NAME)
target_link_libraries(TASK_NAME Chimaera::runtime)
#------------------------------------------------------------------------------
# Install TASK_NAME Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        TASK_NAME
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)
install(FILES TASK_NAME_monitor.py
        DESTINATION ${CHIMEARA_INSTALL_LIB_DIR})

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(TASK_NAME)
endif()
