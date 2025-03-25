cmake_minimum_required(VERSION 3.25)
project(internal_test)

add_subdirectory(tasks) # A mod repo named tasks

add_executable(test_internal internal.cc)
add_dependencies(test_internal example::compressor_client)
target_link_libraries(test_internal example::compressor_client)