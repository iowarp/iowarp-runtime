project(chimaera)

set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------------
# Build Tests
#------------------------------------------------------------------------------

add_rocm_gpu_executable(test_rocm_exec FALSE
        test_rocm.cc
)
add_dependencies(test_rocm_exec chimaera::client_gpu)
target_link_libraries(test_rocm_exec PUBLIC
        chimaera::client_gpu)

#------------------------------------------------------------------------------
# Test Cases
#------------------------------------------------------------------------------

# STRING TESTS
#add_test(NAME test_ipc COMMAND
#        ${CMAKE_BINARY_DIR}/bin/test_messages "TestIpc")

#------------------------------------------------------------------------------
# Install Targets
#------------------------------------------------------------------------------
install(TARGETS
        test_rocm_exec
        LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR})

#-----------------------------------------------------------------------------
# Coverage
#-----------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(test_rocm_exec)
endif()
