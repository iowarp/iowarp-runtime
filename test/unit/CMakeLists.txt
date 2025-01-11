project(chimaera)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(boost)
add_subdirectory(ipc)
add_subdirectory(rocm)