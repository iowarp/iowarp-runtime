project(chimaera)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/tasks/CHI_admin/include)
add_subdirectory(boost)
add_subdirectory(ipc)