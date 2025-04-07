# ------------------------------------------------------------------------------
# Build Chimaera Client Library
# ------------------------------------------------------------------------------
set(CLIENT_SOURCES
  api/chimaera_client.cc
  config/config_client.cc
  config/config_server.cc
  module_registry/module.cc
  module_registry/task.cc
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
  api/chimaera_runtime.cc
  api/chimaera_client.cc
  config/config_client.cc
  config/config_server.cc
  module_registry/module.cc
  module_registry/task.cc
  module_registry/module_registry.cc
  work_orchestrator/work_orchestrator.cc
  work_orchestrator/python_wrapper.cc
  work_orchestrator/reinforce_worker.cc
  work_orchestrator/worker.cc
  queue_manager/queue_manager.cc
)
set(chimaera_runtime_exports)

if(CHIMAERA_ENABLE_CUDA)
  add_cuda_library(chimaera_runtime SHARED TRUE ${RUNTIME_SOURCES})
  add_dependencies(chimaera_runtime chimaera::chimaera_admin_client_gpu)
  target_link_libraries(chimaera_runtime PUBLIC hshm::cudacxx chimaera::chimaera_admin_client_gpu)
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

# ------------------------------------------------------------------------------
# Build Utilities
# ------------------------------------------------------------------------------
add_subdirectory(util)
add_subdirectory(malloc)

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
  work_orchestrator/chimaera_monitor.py
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
endif()