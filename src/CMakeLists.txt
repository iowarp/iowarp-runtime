# ------------------------------------------------------------------------------
# Build Chimaera Client Library
# ------------------------------------------------------------------------------
set(CLIENT_SOURCES
  chimaera_client.cc
  config_client.cc
  config_server.cc
  module.cc
  task.cc
)
set(chimaera_client_exports)

# Create the no-gpu client library
add_library(chimaera_client_host ${CLIENT_SOURCES})
target_link_libraries(chimaera_client_host PUBLIC hshm::cxx)
target_include_directories(chimaera_client_host
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PUBLIC $<INSTALL_INTERFACE:include>
)
add_library(client_host INTERFACE)
target_link_libraries(client_host INTERFACE chimaera_client_host)
add_library(chimaera::client_host ALIAS chimaera_client_host)
list(APPEND chimaera_client_exports chimaera_client_host client_host)

# Create the CUDA gpu client library
if(CHIMAERA_ENABLE_CUDA)
  add_cuda_library(chimaera_client_gpu STATIC TRUE ${CLIENT_SOURCES})
  target_link_libraries(chimaera_client_gpu PUBLIC hshm::cudacxx)
  target_include_directories(chimaera_client_gpu
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    PUBLIC $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(chimaera_client_gpu PUBLIC CHIMAERA_ENABLE_CUDA)
  add_library(client_gpu INTERFACE)
  target_link_libraries(client_gpu INTERFACE chimaera_client_gpu)
  add_library(chimaera::client_gpu ALIAS chimaera_client_gpu)
  list(APPEND chimaera_client_exports chimaera_client_gpu client_gpu)
endif()

# Create the ROCM gpu client library
if(CHIMAERA_ENABLE_ROCM)
  add_rocm_gpu_library(chimaera_client_gpu STATIC TRUE ${CLIENT_SOURCES})
  target_link_libraries(chimaera_client_gpu PUBLIC hshm::rocmcxx_gpu)
  target_include_directories(chimaera_client_gpu
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    PUBLIC $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(chimaera_client_gpu PUBLIC CHIMAERA_ENABLE_ROCM)
  add_library(client_gpu INTERFACE)
  target_link_libraries(client_gpu INTERFACE chimaera_client_gpu)
  add_library(chimaera::client_gpu ALIAS chimaera_client_gpu)
  list(APPEND chimaera_client_exports chimaera_client_gpu client_gpu)
endif()

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Library
# ------------------------------------------------------------------------------
set(RUNTIME_SOURCES
  chimaera_client.cc
  config_client.cc
  config_server.cc
  module.cc
  task.cc
  module_registry.cc
  work_orchestrator.cc
  chimaera_runtime.cc
  python_wrapper.cc
  reinforce_worker.cc
  worker.cc
  queue_manager.cc
)
set(chimaera_runtime_exports)

if(CHIMAERA_ENABLE_CUDA)
  add_cuda_library(chimaera_runtime SHARED TRUE ${RUNTIME_SOURCES})
  target_link_libraries(chimaera_runtime PUBLIC hshm::cudacxx)
  target_compile_definitions(chimaera_runtime PUBLIC CHIMAERA_ENABLE_CUDA)

# add_cuda_library(chimaera_runtime_init SHARED FALSE chimaera_runtime_init.cc)
elseif(CHIMAERA_ENABLE_ROCM)
  add_rocm_gpu_library(chimaera_runtime SHARED TRUE ${RUNTIME_SOURCES})
  target_link_libraries(chimaera_runtime PUBLIC hshm::rocmcxx_gpu)
  target_compile_definitions(chimaera_runtime PUBLIC CHIMAERA_ENABLE_ROCM)

# add_rocm_gpu_library(chimaera_runtime_init SHARED TRUE chimaera_runtime_init.cc)
else()
  add_library(chimaera_runtime ${RUNTIME_SOURCES})
  target_link_libraries(chimaera_runtime PUBLIC hshm::cxx)
endif()

target_link_libraries(chimaera_runtime PUBLIC runtime_deps)
target_include_directories(chimaera_runtime
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  PUBLIC $<INSTALL_INTERFACE:include>
)
target_compile_definitions(chimaera_runtime PUBLIC HSHM_DEFAULT_THREAD_MODEL=hshm::thread::Argobots)
add_library(runtime INTERFACE)
target_link_libraries(runtime INTERFACE chimaera_runtime)
add_library(chimaera::runtime ALIAS chimaera_runtime)
list(APPEND chimaera_runtime_exports chimaera_runtime runtime)

# add_dependencies(chimaera_runtime_init chimaera_runtime)
# target_link_libraries(chimaera_runtime_init PUBLIC chimaera_runtime)
# list(APPEND chimaera_runtime_exports chimaera_runtime_init)

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Start Function
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_CUDA)
  add_cuda_executable(chimaera_start_runtime TRUE chimaera_start_runtime.cc)
elseif(CHIMAERA_ENABLE_ROCM)
  add_rocm_gpu_executable(chimaera_start_runtime TRUE chimaera_start_runtime.cc)
else()
  add_executable(chimaera_start_runtime chimaera_start_runtime.cc)
endif()

add_dependencies(chimaera_start_runtime chimaera_runtime)
target_link_libraries(chimaera_start_runtime PUBLIC chimaera_runtime)

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Stop Function
# ------------------------------------------------------------------------------
add_executable(chimaera_stop_runtime chimaera_stop_runtime.cc)
add_dependencies(chimaera_stop_runtime chimaera::client_host)
target_link_libraries(chimaera_stop_runtime PUBLIC chimaera::client_host)

# ------------------------------------------------------------------------------
# Build Chimaera Runtime Stat Function
# ------------------------------------------------------------------------------
add_executable(chimaera_stat_runtime chimaera_stat_runtime.cc)
add_dependencies(chimaera_stat_runtime chimaera::client_host)
target_link_libraries(chimaera_stat_runtime PUBLIC chimaera::client_host)

# ------------------------------------------------------------------------------
# Build Chimaera Malloc Interceptor
# ------------------------------------------------------------------------------
add_library(chimaera_malloc chimaera_malloc.cc)
add_dependencies(chimaera_malloc chimaera::client_host)
target_link_libraries(chimaera_malloc PUBLIC chimaera::client_host)

# -----------------------------------------------------------------------------
# Add file(s) to CMake Install
# -----------------------------------------------------------------------------
install(
  FILES
  ${CHIMAERA_HEADERS}
  DESTINATION
  ${CHIMAERA_INSTALL_INCLUDE_DIR}
  COMPONENT
  headers
)

# -----------------------------------------------------------------------------
# Add Target(s) to CMake Install
# -----------------------------------------------------------------------------
set(CHIMAERA_EXPORTED_LIBS
  ${chimaera_client_exports}
  ${chimaera_runtime_exports}
  chimaera_start_runtime
  chimaera_stop_runtime
  chimaera_stat_runtime
  chimaera_malloc
  runtime_deps)

install(
  TARGETS
  ${CHIMAERA_EXPORTED_LIBS}
  EXPORT
  ${CHIMAERA_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR}
)
install(
  FILES
  chimaera_monitor.py
  DESTINATION
  ${CHIMAERA_INSTALL_LIB_DIR}
  COMPONENT
  headers
)

# ------------------------------------------------------------------------------
# Coverage
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_COVERAGE)
  set_coverage_flags(chimaera_client)
  set_coverage_flags(chimaera_runtime)
  set_coverage_flags(chimaera_start_runtime)
  set_coverage_flags(chimaera_stat_runtime)
  set_coverage_flags(chimaera_stop_runtime)
endif()