# CMakeLists.txt for Chimaera unit tests
cmake_minimum_required(VERSION 3.10)

# Include common utilities for dependency management
include(${CMAKE_SOURCE_DIR}/cmake/ChimaeraCommon.cmake)

# Test executable name
set(TEST_TARGET chimaera_unit_tests)

# Test source files
set(TEST_SOURCES
  test_chimaera_runtime_simple.cc
)

# Task archive test executable (separate from main tests)
set(TASK_ARCHIVE_TEST_TARGET chimaera_task_archive_tests)
set(TASK_ARCHIVE_TEST_SOURCES
  test_task_archive.cc
)

# Flush correctness test executable (separate test for flush functionality)
set(FLUSH_TEST_TARGET chimaera_flush_correctness_tests)
set(FLUSH_TEST_SOURCES
  test_flush_correctness.cc
)

# Bdev ChiMod test executable (separate test for bdev functionality)
set(BDEV_TEST_TARGET chimaera_bdev_chimod_tests)
set(BDEV_TEST_SOURCES
  test_bdev_chimod.cc
)

# Boost Fiber test executable (separate test for boost context validation)
set(FIBER_TEST_TARGET chimaera_boost_fiber_tests)
set(FIBER_TEST_SOURCES
  test_boost_fiber.cc
)

# Worker Fiber Pattern test executable (test for worker.cc/task.cc pattern)
set(WORKER_FIBER_TEST_TARGET chimaera_worker_fiber_pattern_tests)
set(WORKER_FIBER_TEST_SOURCES
  test_worker_fiber_pattern.cc
)

# Segfault Debug test executable (debug segfault in ExecTask is_started)
set(SEGFAULT_DEBUG_TEST_TARGET chimaera_segfault_debug_tests)
set(SEGFAULT_DEBUG_TEST_SOURCES
  test_segfault_debug.cc
)


# Create task archive test executable
add_executable(${TASK_ARCHIVE_TEST_TARGET} ${TASK_ARCHIVE_TEST_SOURCES})

# Include directories for task archive tests
target_include_directories(${TASK_ARCHIVE_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/test  # For simple_test.h
  ${CMAKE_SOURCE_DIR}/chimods/admin/include  # For admin tasks
  ${HermesShm_INCLUDE_DIRS}
)

# Link against required libraries for task archive tests
target_link_libraries(${TASK_ARCHIVE_TEST_TARGET}
  chimaera                        # Main Chimaera library
  chimaera_admin_runtime          # Admin module runtime for task types
  chimaera_admin_client           # Admin module client for task types
  ${HermesShm_LIBRARIES}          # HSHM libraries
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for task archive tests
set_target_properties(${TASK_ARCHIVE_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Compiler flags for task archive tests
target_compile_definitions(${TASK_ARCHIVE_TEST_TARGET} PRIVATE
  CHIMAERA_RUNTIME=1  # Enable runtime-specific code paths in headers
)

# Copy task archive test executable to bin directory
set_target_properties(${TASK_ARCHIVE_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create flush correctness test executable
add_executable(${FLUSH_TEST_TARGET} ${FLUSH_TEST_SOURCES})

# Include directories for flush tests
target_include_directories(${FLUSH_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/test  # For test utilities
  ${CMAKE_SOURCE_DIR}/chimods/admin/include  # For admin tasks
  ${CMAKE_SOURCE_DIR}/chimods/MOD_NAME/include  # For MOD_NAME tasks
  ${HermesShm_INCLUDE_DIRS}
)

# Link against required libraries for flush tests
target_link_libraries(${FLUSH_TEST_TARGET}
  chimaera                        # Main Chimaera library
  chimaera_admin_runtime          # Admin module runtime
  chimaera_admin_client           # Admin module client
  chimaera_MOD_NAME_runtime       # MOD_NAME module runtime for work tracking
  chimaera_MOD_NAME_client        # MOD_NAME module client
  ${HermesShm_LIBRARIES}          # HSHM libraries
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for flush tests
set_target_properties(${FLUSH_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Compiler flags for flush tests
target_compile_definitions(${FLUSH_TEST_TARGET} PRIVATE
  CHIMAERA_RUNTIME=1  # Enable runtime-specific code paths in headers
)

# Copy flush test executable to bin directory
set_target_properties(${FLUSH_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create bdev test executable
add_executable(${BDEV_TEST_TARGET} ${BDEV_TEST_SOURCES})

# Include directories for bdev tests
target_include_directories(${BDEV_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/test  # For simple_test.h
  ${CMAKE_SOURCE_DIR}/chimods/admin/include  # For admin tasks
  ${CMAKE_SOURCE_DIR}/chimods/bdev/include   # For bdev tasks and client
  ${HermesShm_INCLUDE_DIRS}
)

# Link against required libraries for bdev tests
target_link_libraries(${BDEV_TEST_TARGET}
  chimaera                        # Main Chimaera library
  chimaera_admin_runtime          # Admin module runtime
  chimaera_admin_client           # Admin module client
  chimaera_bdev_runtime           # Bdev module runtime
  chimaera_bdev_client            # Bdev module client
  ${HermesShm_LIBRARIES}          # HSHM libraries
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for bdev tests
set_target_properties(${BDEV_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Compiler flags for bdev tests
target_compile_definitions(${BDEV_TEST_TARGET} PRIVATE
  CHIMAERA_RUNTIME=1  # Enable runtime-specific code paths in headers
)

# Copy bdev test executable to bin directory
set_target_properties(${BDEV_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create boost fiber test executable
add_executable(${FIBER_TEST_TARGET} ${FIBER_TEST_SOURCES})

# Include directories for fiber tests
target_include_directories(${FIBER_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# Find Boost context library for fiber tests
find_package(Boost REQUIRED COMPONENTS context)

# Link against hermes-shm target for fiber tests (which includes boost)
target_link_libraries(${FIBER_TEST_TARGET}
  hshm::cxx                       # Hermes-SHM target (includes boost dependencies)
  Boost::context                  # Boost context library for fiber operations
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for fiber tests
set_target_properties(${FIBER_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Copy fiber test executable to bin directory
set_target_properties(${FIBER_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create worker fiber pattern test executable
add_executable(${WORKER_FIBER_TEST_TARGET} ${WORKER_FIBER_TEST_SOURCES})

# Include directories for worker fiber pattern tests
target_include_directories(${WORKER_FIBER_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# Link against hermes-shm target for worker fiber pattern tests
target_link_libraries(${WORKER_FIBER_TEST_TARGET}
  hshm::cxx                       # Hermes-SHM target (includes boost dependencies)
  Boost::context                  # Boost context library for fiber operations
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for worker fiber pattern tests
set_target_properties(${WORKER_FIBER_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Copy worker fiber pattern test executable to bin directory
set_target_properties(${WORKER_FIBER_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create segfault debug test executable
add_executable(${SEGFAULT_DEBUG_TEST_TARGET} ${SEGFAULT_DEBUG_TEST_SOURCES})

# Include directories for segfault debug tests
target_include_directories(${SEGFAULT_DEBUG_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# Link against hermes-shm target for segfault debug tests
target_link_libraries(${SEGFAULT_DEBUG_TEST_TARGET}
  hshm::cxx                       # Hermes-SHM target (includes boost dependencies)
  Boost::context                  # Boost context library for fiber operations
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for segfault debug tests
set_target_properties(${SEGFAULT_DEBUG_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Copy segfault debug test executable to bin directory
set_target_properties(${SEGFAULT_DEBUG_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


# Create test executable
add_executable(${TEST_TARGET} ${TEST_SOURCES})

# Include directories
target_include_directories(${TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/test  # For simple_test.h
  ${HermesShm_INCLUDE_DIRS}
)

# Link against required libraries
target_link_libraries(${TEST_TARGET}
  chimaera                        # Main Chimaera library
  ${HermesShm_LIBRARIES}          # HSHM libraries
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard
set_target_properties(${TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Compiler flags for tests (enable runtime macros)
target_compile_definitions(${TEST_TARGET} PRIVATE
  CHIMAERA_RUNTIME=1  # Enable runtime-specific code paths in headers
)

# Copy test executable to bin directory for easier access
set_target_properties(${TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Enable CTest integration if testing is enabled
if(CHIMAERA_ENABLE_TESTS)
  # Task Archive Tests
  add_test(
    NAME task_archive_basic_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][input_in],[task_archive][output_in],[task_archive][input_out],[task_archive][output_out]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_bulk_transfer_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][bulk_transfer]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_non_task_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][non_task]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_task_base_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][task_base]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_admin_tasks_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][admin_tasks]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_bidirectional_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][bidirectional]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_container_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][container]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_error_handling_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][error_handling]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_performance_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][performance]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME task_archive_integration_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][integration]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  # Comprehensive task archive test
  add_test(
    NAME all_task_archive_tests
    COMMAND ${TASK_ARCHIVE_TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  # Add test cases with different tags
  add_test(
    NAME runtime_initialization_tests
    COMMAND ${TEST_TARGET} "[runtime][initialization]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME client_initialization_tests  
    COMMAND ${TEST_TARGET} "[client][initialization]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME mod_name_task_tests
    COMMAND ${TEST_TARGET} "[task][mod_name]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME error_handling_tests
    COMMAND ${TEST_TARGET} "[error][edge_cases]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME concurrent_tests
    COMMAND ${TEST_TARGET} "[concurrent][stress]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME memory_tests
    COMMAND ${TEST_TARGET} "[memory][cleanup]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME performance_tests
    COMMAND ${TEST_TARGET} "[performance][timing]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  # Flush Correctness Tests
  add_test(
    NAME flush_basic_tests
    COMMAND ${FLUSH_TEST_TARGET} "[flush][admin]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME flush_work_orchestrator_tests
    COMMAND ${FLUSH_TEST_TARGET} "[flush][work_orchestrator]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME flush_stress_tests
    COMMAND ${FLUSH_TEST_TARGET} "[flush][stress]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  # Comprehensive flush test
  add_test(
    NAME all_flush_tests
    COMMAND ${FLUSH_TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  # Bdev ChiMod Tests
  add_test(
    NAME bdev_container_creation_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][create]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_block_allocation_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][allocate]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_io_operations_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][io]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_async_operations_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][async]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_free_management_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][free]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_performance_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][performance]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_error_handling_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][error]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  add_test(
    NAME bdev_cleanup_tests
    COMMAND ${BDEV_TEST_TARGET} "[bdev][cleanup]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  # Comprehensive bdev test
  add_test(
    NAME all_bdev_tests
    COMMAND ${BDEV_TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  # Add a comprehensive test that runs all tests
  add_test(
    NAME all_chimaera_tests
    COMMAND ${TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  
  # Set test properties for timeout and environment
  set_tests_properties(
    task_archive_basic_tests
    task_archive_bulk_transfer_tests
    task_archive_non_task_tests
    task_archive_task_base_tests
    task_archive_admin_tasks_tests
    task_archive_bidirectional_tests
    task_archive_container_tests
    task_archive_error_handling_tests
    task_archive_performance_tests
    task_archive_integration_tests
    all_task_archive_tests
    runtime_initialization_tests
    client_initialization_tests
    mod_name_task_tests
    error_handling_tests
    concurrent_tests
    memory_tests
    performance_tests
    flush_basic_tests
    flush_work_orchestrator_tests
    flush_stress_tests
    all_flush_tests
    bdev_container_creation_tests
    bdev_block_allocation_tests
    bdev_io_operations_tests
    bdev_async_operations_tests
    bdev_free_management_tests
    bdev_performance_tests
    bdev_error_handling_tests
    bdev_cleanup_tests
    all_bdev_tests
    all_chimaera_tests
    PROPERTIES
      TIMEOUT 120  # 120 second timeout for each test (increased for I/O tests)
      ENVIRONMENT "CHIMAERA_TEST_MODE=1"
  )
endif()

# Custom targets for running specific test categories

# Task Archive Test Targets
add_custom_target(test_task_archive
  COMMAND ${TASK_ARCHIVE_TEST_TARGET}
  DEPENDS ${TASK_ARCHIVE_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera task archive tests"
)

add_custom_target(test_task_archive_basic
  COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][input_in],[task_archive][output_in],[task_archive][input_out],[task_archive][output_out]"
  DEPENDS ${TASK_ARCHIVE_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera task archive basic tests"
)

add_custom_target(test_task_archive_serialization
  COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][task_base],[task_archive][admin_tasks]"
  DEPENDS ${TASK_ARCHIVE_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera task serialization tests"
)

add_custom_target(test_task_archive_performance
  COMMAND ${TASK_ARCHIVE_TEST_TARGET} "[task_archive][performance]"
  DEPENDS ${TASK_ARCHIVE_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera task archive performance tests"
)

add_custom_target(test_runtime
  COMMAND ${TEST_TARGET} "[runtime]"
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera runtime tests"
)

add_custom_target(test_client
  COMMAND ${TEST_TARGET} "[client]"
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera client tests"
)

add_custom_target(test_tasks
  COMMAND ${TEST_TARGET} "[task]"
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera task tests"
)

add_custom_target(test_errors
  COMMAND ${TEST_TARGET} "[error]"
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera error handling tests"
)

add_custom_target(test_performance
  COMMAND ${TEST_TARGET} "[performance]"
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera performance tests"
)

# Custom targets for flush tests
add_custom_target(test_flush
  COMMAND ${FLUSH_TEST_TARGET}
  DEPENDS ${FLUSH_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera flush correctness tests"
)

add_custom_target(test_flush_basic
  COMMAND ${FLUSH_TEST_TARGET} "[flush][admin]"
  DEPENDS ${FLUSH_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera flush basic tests"
)

add_custom_target(test_flush_work_orchestrator
  COMMAND ${FLUSH_TEST_TARGET} "[flush][work_orchestrator]"
  DEPENDS ${FLUSH_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera flush WorkOrchestrator integration tests"
)

add_custom_target(test_flush_stress
  COMMAND ${FLUSH_TEST_TARGET} "[flush][stress]"
  DEPENDS ${FLUSH_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera flush stress tests"
)

# Custom targets for bdev tests
add_custom_target(test_bdev
  COMMAND ${BDEV_TEST_TARGET}
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev ChiMod tests"
)

add_custom_target(test_bdev_create
  COMMAND ${BDEV_TEST_TARGET} "[bdev][create]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev container creation tests"
)

add_custom_target(test_bdev_allocate
  COMMAND ${BDEV_TEST_TARGET} "[bdev][allocate]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev block allocation tests"
)

add_custom_target(test_bdev_io
  COMMAND ${BDEV_TEST_TARGET} "[bdev][io]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev I/O operation tests"
)

add_custom_target(test_bdev_async
  COMMAND ${BDEV_TEST_TARGET} "[bdev][async]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev asynchronous I/O tests"
)

add_custom_target(test_bdev_performance
  COMMAND ${BDEV_TEST_TARGET} "[bdev][performance]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev performance metrics tests"
)

add_custom_target(test_bdev_error
  COMMAND ${BDEV_TEST_TARGET} "[bdev][error]"
  DEPENDS ${BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera bdev error handling tests"
)

# Custom target to run all unit tests
add_custom_target(test_all_unit
  COMMAND ${TEST_TARGET}
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running all Chimaera unit tests"
)

# Install test executables (optional)
install(TARGETS ${TEST_TARGET} ${TASK_ARCHIVE_TEST_TARGET} ${FLUSH_TEST_TARGET} ${BDEV_TEST_TARGET} ${FIBER_TEST_TARGET} ${WORKER_FIBER_TEST_TARGET} ${SEGFAULT_DEBUG_TEST_TARGET}
  RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "Chimaera unit tests configured:")
message(STATUS "  Test target: ${TEST_TARGET}")
message(STATUS "  Test sources: ${TEST_SOURCES}")
message(STATUS "  Task archive test target: ${TASK_ARCHIVE_TEST_TARGET}")
message(STATUS "  Task archive test sources: ${TASK_ARCHIVE_TEST_SOURCES}")
message(STATUS "  Flush test target: ${FLUSH_TEST_TARGET}")
message(STATUS "  Flush test sources: ${FLUSH_TEST_SOURCES}")
message(STATUS "  Bdev test target: ${BDEV_TEST_TARGET}")
message(STATUS "  Bdev test sources: ${BDEV_TEST_SOURCES}")
message(STATUS "  CTest enabled: ${CHIMAERA_ENABLE_TESTS}")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin")