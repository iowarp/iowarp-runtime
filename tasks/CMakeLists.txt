
cmake_minimum_required(VERSION 3.25)
project(chimaera)
set(MOD_NAMESPACE chimaera)

# FIND CHIMAERA
if(NOT CHIMAERA_IS_MAIN_PROJECT)
  find_package(Chimaera CONFIG REQUIRED)
endif()

# SET INSTALL VARIABLES
if(NOT CHIMAERA_INSTALL_BIN_DIR)
  set(CHIMAERA_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()

if(NOT CHIMAERA_INSTALL_LIB_DIR)
  set(CHIMAERA_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()

if(NOT CHIMAERA_INSTALL_INCLUDE_DIR)
  set(CHIMAERA_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()

if(NOT CHIMAERA_INSTALL_DATA_DIR)
  set(CHIMAERA_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()

if(NOT CHIMAERA_EXPORTED_TARGETS)
  set(CHIMAERA_EXPORTED_TARGETS Chimaeraexports)
endif()

# ADD SUBDIRECTORIES
add_subdirectory(TASK_NAME)
add_subdirectory(bdev)
add_subdirectory(chimaera_admin)
add_subdirectory(remote_queue)
add_subdirectory(small_message)
add_subdirectory(worch_proc_round_robin)
add_subdirectory(worch_queue_round_robin)

# INSTALL TARGETS
if(NOT CHIMAERA_IS_MAIN_PROJECT)
  install(EXPORT ${CHIMAERA_EXPORTED_TARGETS}
    FILE ${CHIMAERA_EXPORTED_TARGETS}Config.cmake
    NAMESPACE chimaera::
    DESTINATION cmake
  )
endif()
