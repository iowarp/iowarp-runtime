cmake_minimum_required(VERSION 3.25)
project(external)

find_package(Chimaera CONFIG REQUIRED)
find_package(Example CONFIG REQUIRED)

add_executable(external external.cc)
target_link_libraries(external example::compressor_client)