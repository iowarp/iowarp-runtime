# Chimaera Modules (ChiMods) Directory
# This directory contains standard Chimaera modules

cmake_minimum_required(VERSION 3.10)

message(STATUS "Building Chimaera modules (ChiMods)...")

# Add individual module subdirectories
add_subdirectory(MOD_NAME)

# Add admin module (critical system module)
add_subdirectory(admin)

# Additional modules can be added here as they are developed
# add_subdirectory(other_module_name)