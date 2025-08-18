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
    all_chimaera_tests
    PROPERTIES
      TIMEOUT 60  # 60 second timeout for each test
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

# Custom target to run all unit tests
add_custom_target(test_all_unit
  COMMAND ${TEST_TARGET}
  DEPENDS ${TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running all Chimaera unit tests"
)

# Install test executables (optional)
install(TARGETS ${TEST_TARGET} ${TASK_ARCHIVE_TEST_TARGET}
  RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "Chimaera unit tests configured:")
message(STATUS "  Test target: ${TEST_TARGET}")
message(STATUS "  Test sources: ${TEST_SOURCES}")
message(STATUS "  Task archive test target: ${TASK_ARCHIVE_TEST_TARGET}")
message(STATUS "  Task archive test sources: ${TASK_ARCHIVE_TEST_SOURCES}")
message(STATUS "  CTest enabled: ${CHIMAERA_ENABLE_TESTS}")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin")