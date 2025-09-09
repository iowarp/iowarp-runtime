# External ChiMod Linking Test

This directory contains an example of how external applications can link to Chimaera ChiMod libraries using the CMake export system.

## Overview

The test demonstrates:
- Finding and linking to installed Chimaera ChiMod libraries
- Using the namespace-based target names (`chimaera::MOD_NAME_client`, etc.)
- Creating and using a MOD_NAME container from an external application

## Building and Running

### Prerequisites

1. Install Chimaera libraries first:
   ```bash
   # From the main project directory
   cmake --preset debug
   cmake --build build
   cmake --install build --prefix /usr/local  # or your preferred install path
   ```

2. Ensure the installed libraries are in your system's library path:
   ```bash
   export CMAKE_PREFIX_PATH=/usr/local:$CMAKE_PREFIX_PATH
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
   ```

### Building the External Test

```bash
# Navigate to this directory
cd test/unit/external

# Configure and build
mkdir build
cd build
cmake ..
make

# Run the test
./external_mod_name_test
```

### Expected Behavior

- **Without Runtime**: The test will demonstrate successful linking but container operations will fail gracefully
- **With Runtime**: Start `chimaera_start_runtime` in another terminal for full functionality

## CMake Integration

External projects can use this pattern:

```cmake
find_package(chimaera REQUIRED)

target_link_libraries(your_target
  chimaera::MOD_NAME_client
  chimaera::admin_client
)
```

## Library Export Structure

The Chimaera build system creates export sets named after the namespace (from `chimaera_repo.yaml`):

- Export set: `chimaeraTargets`
- Config: `chimaeraConfig.cmake`
- Targets: `chimaera::MOD_NAME_client`, `chimaera::MOD_NAME_runtime`, `chimaera::admin_client`, etc.

This allows external applications to cleanly link against ChiMod libraries without knowing internal target names.