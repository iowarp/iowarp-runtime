#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(small_message SHARED
        small_message.cc)
add_chimaera_run_deps(small_message)
target_link_libraries(small_message Chimaera::runtime)
target_compile_definitions(small_message PRIVATE ${Chimaera_RUNTIME_DEFINITIONS})
#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
install(
        TARGETS
        small_message
        LIBRARY DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMEARA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMEARA_INSTALL_BIN_DIR}
)
install(FILES small_message_monitor.py
        DESTINATION ${CHIMEARA_INSTALL_LIB_DIR})

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(small_message)
endif()
