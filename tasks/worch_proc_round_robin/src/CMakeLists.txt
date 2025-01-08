#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(worch_proc_round_robin SHARED
        worch_proc_round_robin.cc)
add_chimaera_run_deps(worch_proc_round_robin)
target_link_libraries(worch_proc_round_robin Chimaera::runtime)
#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        worch_proc_round_robin
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(worch_proc_round_robin)
endif()
