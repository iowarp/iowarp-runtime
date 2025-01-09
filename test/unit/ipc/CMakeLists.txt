project(chimaera)

set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------------
# Build Tests
#------------------------------------------------------------------------------

add_executable(test_ipc_exec
        ${TEST_MAIN}/main_mpi.cc
        test_finalize.cc
        test_ipc.cc
        test_serialize.cc
        test_type_sizes.cc
)
add_dependencies(test_ipc_exec ${Chimaera_CLIENT_DEPS})
target_link_libraries(test_ipc_exec
        chimaera_client
        Catch2::Catch2 MPI::MPI_CXX OpenMP::OpenMP_CXX
        ${OPTIONAL_LIBS})
target_compile_definitions(test_ipc_exec PRIVATE CHIMAERA_ENABLE_PYTHON)

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
        test_ipc_exec
        LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR})

#-----------------------------------------------------------------------------
# Coverage
#-----------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
    set_coverage_flags(test_ipc_exec)
endif()
