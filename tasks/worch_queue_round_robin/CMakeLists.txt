#------------------------------------------------------------------------------
# Build CHI Admin Task Library
#------------------------------------------------------------------------------
include_directories(include)
add_subdirectory(src)

#-----------------------------------------------------------------------------
# Install CHI Admin Task Library Headers
#-----------------------------------------------------------------------------
install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})
