# ChimaeraCommon.cmake
# Common utilities and dependencies for Chimaera project and external libraries

#------------------------------------------------------------------------------
# COMMON DEPENDENCIES
#------------------------------------------------------------------------------

# Find HermesShm - the shared memory framework used by Chimaera
# HermesShm is compiled with Boost support, so it provides the necessary Boost dependencies
find_package(HermesShm CONFIG REQUIRED)

# Find cereal for serialization support
find_package(cereal REQUIRED)

# Find Boost for fiber context support (required by types.h)
find_package(Boost REQUIRED COMPONENTS fiber context)

#------------------------------------------------------------------------------
# UTILITY FUNCTIONS
#------------------------------------------------------------------------------

# Function to read repository namespace from chimaera_repo.yaml
# Searches up the directory tree from the given path to find chimaera_repo.yaml
function(read_repo_namespace output_var start_path)
  set(current_path "${start_path}")
  set(namespace "chimaera")  # Default fallback
  
  # Search up the directory tree for chimaera_repo.yaml
  while(NOT "${current_path}" STREQUAL "/" AND NOT "${current_path}" STREQUAL "")
    set(repo_file "${current_path}/chimaera_repo.yaml")
    if(EXISTS "${repo_file}")
      # Read and parse the YAML file
      file(READ "${repo_file}" REPO_YAML_CONTENT)
      string(REGEX MATCH "namespace: *([^\n\r]+)" NAMESPACE_MATCH "${REPO_YAML_CONTENT}")
      if(NAMESPACE_MATCH)
        string(REGEX REPLACE "namespace: *" "" namespace "${NAMESPACE_MATCH}")
        string(STRIP "${namespace}" namespace)
        break()
      endif()
    endif()
    
    # Move up one directory
    get_filename_component(current_path "${current_path}" DIRECTORY)
  endwhile()
  
  set(${output_var} "${namespace}" PARENT_SCOPE)
endfunction()

# Function to create both client and runtime libraries for a ChiMod
# Creates targets in format: ${NAMESPACE}_${CHIMOD_NAME}_runtime and ${NAMESPACE}_${CHIMOD_NAME}_client
function(add_chimod_both)
  cmake_parse_arguments(ARG
    ""  # options
    "NAMESPACE;CHIMOD_NAME"  # one value args
    "RUNTIME_SOURCES;CLIENT_SOURCES"  # multi value args
    ${ARGN}
  )
  
  # Use project namespace if not provided
  if(NOT ARG_NAMESPACE)
    # Try to read from local chimaera_repo.yaml first
    read_repo_namespace(REPO_NAMESPACE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(DEFINED CHIMAERA_NAMESPACE)
      set(ARG_NAMESPACE ${CHIMAERA_NAMESPACE})
    else()
      set(ARG_NAMESPACE "${REPO_NAMESPACE}")
    endif()
  endif()
  
  if(NOT ARG_CHIMOD_NAME)
    message(FATAL_ERROR "add_chimod_both: CHIMOD_NAME is required")
  endif()
  
  # Create target names using namespace_chimod_type format
  set(RUNTIME_TARGET_NAME "${ARG_NAMESPACE}_${ARG_CHIMOD_NAME}_runtime")
  set(CLIENT_TARGET_NAME "${ARG_NAMESPACE}_${ARG_CHIMOD_NAME}_client")
  
  # Create runtime library
  if(ARG_RUNTIME_SOURCES)
    add_library(${RUNTIME_TARGET_NAME} SHARED ${ARG_RUNTIME_SOURCES})
    target_link_libraries(${RUNTIME_TARGET_NAME} PUBLIC chimaera)
    target_include_directories(${RUNTIME_TARGET_NAME} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(${RUNTIME_TARGET_NAME} PRIVATE
      CHI_CHIMOD_NAME="${ARG_CHIMOD_NAME}"
      CHI_NAMESPACE="${ARG_NAMESPACE}"
      CHIMAERA_RUNTIME=1
    )
    
    # Create namespace alias for external consumption
    add_library(${ARG_NAMESPACE}::${ARG_CHIMOD_NAME}_runtime ALIAS ${RUNTIME_TARGET_NAME})
    
    # Set global property for referencing this target
    set_property(GLOBAL PROPERTY ${ARG_CHIMOD_NAME}_RUNTIME_TARGET ${RUNTIME_TARGET_NAME})
  endif()
  
  # Create client library
  if(ARG_CLIENT_SOURCES)
    add_library(${CLIENT_TARGET_NAME} SHARED ${ARG_CLIENT_SOURCES})
    target_link_libraries(${CLIENT_TARGET_NAME} PUBLIC chimaera)
    target_include_directories(${CLIENT_TARGET_NAME} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(${CLIENT_TARGET_NAME} PRIVATE
      CHI_CHIMOD_NAME="${ARG_CHIMOD_NAME}"
      CHI_NAMESPACE="${ARG_NAMESPACE}"
      CHIMAERA_CLIENT=1
      CHIMAERA_RUNTIME=1
    )
    
    # Create namespace alias for external consumption
    add_library(${ARG_NAMESPACE}::${ARG_CHIMOD_NAME}_client ALIAS ${CLIENT_TARGET_NAME})
    
    # Set global property for referencing this target
    set_property(GLOBAL PROPERTY ${ARG_CHIMOD_NAME}_CLIENT_TARGET ${CLIENT_TARGET_NAME})
  endif()
endfunction()

# Function to install ChiMod libraries
# Automatically finds targets created by add_chimod_both using global properties
function(install_chimod)
  cmake_parse_arguments(ARG
    ""  # options
    "NAMESPACE;CHIMOD_NAME;RUNTIME_TARGET;CLIENT_TARGET"  # one value args
    ""  # multi value args
    ${ARGN}
  )
  
  # Use project namespace if not provided
  if(NOT ARG_NAMESPACE)
    # Try to read from local chimaera_repo.yaml first
    read_repo_namespace(REPO_NAMESPACE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(DEFINED CHIMAERA_NAMESPACE)
      set(ARG_NAMESPACE ${CHIMAERA_NAMESPACE})
    else()
      set(ARG_NAMESPACE "${REPO_NAMESPACE}")
    endif()
  endif()
  
  if(NOT ARG_CHIMOD_NAME)
    message(FATAL_ERROR "install_chimod: CHIMOD_NAME is required")
  endif()
  
  # Get target names from global properties or compute them
  set(TARGETS_TO_INSTALL "")
  
  if(ARG_RUNTIME_TARGET)
    # Use explicitly provided target name
    if(TARGET ${ARG_RUNTIME_TARGET})
      list(APPEND TARGETS_TO_INSTALL ${ARG_RUNTIME_TARGET})
    endif()
  else()
    # Get runtime target from global property or compute it
    get_property(RUNTIME_TARGET GLOBAL PROPERTY ${ARG_CHIMOD_NAME}_RUNTIME_TARGET)
    if(NOT RUNTIME_TARGET)
      set(RUNTIME_TARGET "${ARG_NAMESPACE}_${ARG_CHIMOD_NAME}_runtime")
    endif()
    if(TARGET ${RUNTIME_TARGET})
      list(APPEND TARGETS_TO_INSTALL ${RUNTIME_TARGET})
    endif()
  endif()
  
  if(ARG_CLIENT_TARGET)
    # Use explicitly provided target name
    if(TARGET ${ARG_CLIENT_TARGET})
      list(APPEND TARGETS_TO_INSTALL ${ARG_CLIENT_TARGET})
    endif()
  else()
    # Get client target from global property or compute it
    get_property(CLIENT_TARGET GLOBAL PROPERTY ${ARG_CHIMOD_NAME}_CLIENT_TARGET)
    if(NOT CLIENT_TARGET)
      set(CLIENT_TARGET "${ARG_NAMESPACE}_${ARG_CHIMOD_NAME}_client")
    endif()
    if(TARGET ${CLIENT_TARGET})
      list(APPEND TARGETS_TO_INSTALL ${CLIENT_TARGET})
    endif()
  endif()
  
  if(TARGETS_TO_INSTALL)
    # Use the package name format that CMake expects for automatic config generation
    set(MODULE_PACKAGE_NAME "${ARG_NAMESPACE}-${ARG_CHIMOD_NAME}")
    set(MODULE_EXPORT_NAME "${MODULE_PACKAGE_NAME}")
    
    # Install targets with module-specific export set
    install(TARGETS ${TARGETS_TO_INSTALL}
      EXPORT ${MODULE_EXPORT_NAME}
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      RUNTIME DESTINATION bin
      INCLUDES DESTINATION include
    )
    
    # Install headers if they exist
    set(CHIMOD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    if(EXISTS "${CHIMOD_INCLUDE_DIR}")
      install(DIRECTORY "${CHIMOD_INCLUDE_DIR}/"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
      )
    endif()
    
    # Export targets file - CMake will automatically generate config files
    install(EXPORT ${MODULE_EXPORT_NAME}
      FILE ${MODULE_EXPORT_NAME}.cmake
      NAMESPACE ${ARG_NAMESPACE}::
      DESTINATION cmake/${MODULE_PACKAGE_NAME}
    )
    
    message(STATUS "Created module package: ${ARG_NAMESPACE}::${ARG_CHIMOD_NAME}")
    message(STATUS "  Export set: ${MODULE_EXPORT_NAME}")
    message(STATUS "  Config location: cmake/${MODULE_PACKAGE_NAME}")
  endif()
endfunction()