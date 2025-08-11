cmake_minimum_required(VERSION 3.10)

# Create both client and runtime libraries for MOD_NAME
add_chimod_both(
  NAMESPACE chimaera
  CHIMOD_NAME MOD_NAME
  RUNTIME_SOURCES src/MOD_NAME_runtime.cc
  CLIENT_SOURCES src/MOD_NAME_client.cc
)

# Install the ChiMod
install_chimod(
  NAMESPACE chimaera
  CHIMOD_NAME MOD_NAME
  RUNTIME_TARGET MOD_NAME_runtime
  CLIENT_TARGET MOD_NAME_client
)