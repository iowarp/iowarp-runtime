project(chimaera)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(CHIMAERA_ENABLE_ROCM)
    add_subdirectory(rocm)
endif()

if(CHIMAERA_ENABLE_CUDA)
    add_subdirectory(cuda)
endif()