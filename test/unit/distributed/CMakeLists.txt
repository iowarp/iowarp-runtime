# CMakeLists.txt for Chimaera distributed unit tests
cmake_minimum_required(VERSION 3.10)

# Include common utilities for dependency management
include(${CMAKE_SOURCE_DIR}/cmake/ChimaeraCommon.cmake)

# Distributed BDev test executable
set(DISTRIBUTED_BDEV_TEST_TARGET chimaera_distributed_bdev_tests)
set(DISTRIBUTED_BDEV_TEST_SOURCES
  test_distributed_bdev.cc
)

# Create distributed bdev test executable
add_executable(${DISTRIBUTED_BDEV_TEST_TARGET} ${DISTRIBUTED_BDEV_TEST_SOURCES})

# Include directories for distributed bdev tests
target_include_directories(${DISTRIBUTED_BDEV_TEST_TARGET} PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/test  # For simple_test.h
  ${CMAKE_SOURCE_DIR}/chimods/admin/include  # For admin tasks
  ${CMAKE_SOURCE_DIR}/chimods/bdev/include   # For bdev tasks and client
)

# Link against required libraries for distributed bdev tests
target_link_libraries(${DISTRIBUTED_BDEV_TEST_TARGET}
  chimaera_admin_runtime          # Admin module runtime
  chimaera_admin_client           # Admin module client
  chimaera_bdev_runtime           # Bdev module runtime
  chimaera_bdev_client            # Bdev module client
  hshm::cxx                       # HermesShm library
  ${CMAKE_THREAD_LIBS_INIT}       # Threading support
)

# Set C++ standard for distributed bdev tests
set_target_properties(${DISTRIBUTED_BDEV_TEST_TARGET} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Copy distributed bdev test executable to bin directory
set_target_properties(${DISTRIBUTED_BDEV_TEST_TARGET} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Enable CTest integration if testing is enabled
if(CHIMAERA_ENABLE_TESTS)
  # Distributed BDev Tests - DirectHash
  add_test(
    NAME distributed_bdev_direct_hash_tests
    COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case direct
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  set_tests_properties(distributed_bdev_direct_hash_tests PROPERTIES
    ENVIRONMENT "CHI_REPO_PATH=${CMAKE_BINARY_DIR}/bin"
  )

  # Distributed BDev Tests - Range
  add_test(
    NAME distributed_bdev_range_tests
    COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case range
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  set_tests_properties(distributed_bdev_range_tests PROPERTIES
    ENVIRONMENT "CHI_REPO_PATH=${CMAKE_BINARY_DIR}/bin"
  )

  # Distributed BDev Tests - Broadcast
  add_test(
    NAME distributed_bdev_broadcast_tests
    COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case broadcast
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  set_tests_properties(distributed_bdev_broadcast_tests PROPERTIES
    ENVIRONMENT "CHI_REPO_PATH=${CMAKE_BINARY_DIR}/bin"
  )

  # Set test properties for timeout and environment
  set_tests_properties(
    distributed_bdev_direct_hash_tests
    distributed_bdev_range_tests
    distributed_bdev_broadcast_tests
    PROPERTIES
      TIMEOUT 300  # 300 second timeout for distributed tests
      ENVIRONMENT "CHI_REPO_PATH=${CMAKE_BINARY_DIR}/bin;CHIMAERA_TEST_MODE=1"
  )
endif()

# Custom targets for running distributed tests
add_custom_target(test_distributed_bdev
  COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case direct
  DEPENDS ${DISTRIBUTED_BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera distributed bdev tests (DirectHash)"
)

add_custom_target(test_distributed_bdev_direct
  COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case direct
  DEPENDS ${DISTRIBUTED_BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera distributed bdev DirectHash tests"
)

add_custom_target(test_distributed_bdev_range
  COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case range
  DEPENDS ${DISTRIBUTED_BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera distributed bdev Range tests"
)

add_custom_target(test_distributed_bdev_broadcast
  COMMAND ${DISTRIBUTED_BDEV_TEST_TARGET} --num-nodes 3 --test-case broadcast
  DEPENDS ${DISTRIBUTED_BDEV_TEST_TARGET}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  COMMENT "Running Chimaera distributed bdev Broadcast tests"
)

# Install test executables (optional)
install(TARGETS ${DISTRIBUTED_BDEV_TEST_TARGET}
  RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "Chimaera distributed unit tests configured:")
message(STATUS "  Distributed BDev test target: ${DISTRIBUTED_BDEV_TEST_TARGET}")
message(STATUS "  Distributed BDev test sources: ${DISTRIBUTED_BDEV_TEST_SOURCES}")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin")
