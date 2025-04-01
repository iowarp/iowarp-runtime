project(chimaera)

set(CMAKE_CXX_STANDARD 17)

# ------------------------------------------------------------------------------
# Build Tests
# ------------------------------------------------------------------------------
set(TEST_LIBS
        chimaera::client_host
        Catch2::Catch2 MPI::MPI_CXX OpenMP::OpenMP_CXX)
set(TEST_OPTS "")

if(BUILD_ZeroMQ_TESTS)
        set(TEST_LIBS ${TEST_LIBS} ZeroMQ::ZeroMQ)
        set(TEST_OPTS "${TEST_OPTS} -DCHIMERA_ENABLE_ZMQ")
endif()

add_executable(bench_chimaera_latency
        test_latency.cc)
add_dependencies(bench_chimaera_latency chimaera::client_host)
target_link_libraries(bench_chimaera_latency ${TEST_LIBS})
target_compile_definitions(bench_chimaera_latency PUBLIC ${TEST_OPTS})

add_executable(bench_chimaera_bw
        test_bw.cc)
add_dependencies(bench_chimaera_bw chimaera::client_host)
target_link_libraries(bench_chimaera_bw ${TEST_LIBS})
target_compile_definitions(bench_chimaera_bw PUBLIC ${TEST_OPTS})

add_executable(bench_chimaera_zlib
        test_zlib.cc
)
add_dependencies(bench_chimaera_zlib chimaera::client_host)
target_link_libraries(bench_chimaera_zlib ${TEST_LIBS})
target_compile_definitions(bench_chimaera_zlib PUBLIC ${TEST_OPTS})

add_executable(chimaera_io_bench test_bdev_io.cc)
target_link_libraries(chimaera_io_bench ${TEST_LIBS})

# ------------------------------------------------------------------------------
# Test Cases
# ------------------------------------------------------------------------------

# STRING TESTS
# add_test(NAME test_ipc COMMAND
# ${CMAKE_BINARY_DIR}/bin/test_messages "TestIpc")

# ------------------------------------------------------------------------------
# Install Targets
# ------------------------------------------------------------------------------
install(TARGETS
        bench_chimaera_latency
        bench_chimaera_bw
        bench_chimaera_zlib
        chimaera_io_bench
        LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR})

# -----------------------------------------------------------------------------
# Coverage
# -----------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
        set_coverage_flags(test_ipc_exec)
endif()
