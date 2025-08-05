# Collect all source files for the chimaera library
file(GLOB_RECURSE CHIMAERA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

# Remove malloc sources as they might be separate
list(FILTER CHIMAERA_SOURCES EXCLUDE REGEX "malloc/")

# Create chimaera_runtime target
add_library(chimaera_runtime ${CHIMAERA_SOURCES})

# Set include directories
target_include_directories(chimaera_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set compile definitions - CHIMAERA_RUNTIME macro for runtime code
target_compile_definitions(chimaera_runtime PUBLIC CHIMAERA_RUNTIME)

# Link to hermes_shm
target_link_libraries(chimaera_runtime PUBLIC hshm::cxx)

# Create chimaera_client target
add_library(chimaera_client ${CHIMAERA_SOURCES})

# Set include directories
target_include_directories(chimaera_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link to hermes_shm (no CHIMAERA_RUNTIME macro for client)
target_link_libraries(chimaera_client PUBLIC hshm::cxx)

# Create aliases with chimaera:: namespace (expected by build system)
add_library(chimaera::runtime ALIAS chimaera_runtime)
add_library(chimaera::client_host ALIAS chimaera_client)

# Also create chi:: namespace aliases as requested
add_library(chi::runtime ALIAS chimaera_runtime)
add_library(chi::client ALIAS chimaera_client)

# Install targets for export
install(TARGETS chimaera_runtime chimaera_client
        EXPORT ${CHIMAERA_EXPORTED_TARGETS}
        LIBRARY DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${CHIMAERA_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${CHIMAERA_INSTALL_BIN_DIR})

# Add subdirectories for additional components
add_subdirectory(malloc)
add_subdirectory(util)