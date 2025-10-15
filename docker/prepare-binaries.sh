#!/bin/bash
# Prepare Chimaera binaries for Docker build
# This script copies necessary binaries and libraries from the build directory
# to docker/bin/ for inclusion in the Docker image

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
BIN_DIR="${SCRIPT_DIR}/bin"

echo "=== Preparing Chimaera Binaries for Docker Build ==="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Target directory: $BIN_DIR"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory not found: $BUILD_DIR"
    echo "Please build the project first:"
    echo "  cmake --preset debug"
    echo "  cmake --build build"
    exit 1
fi

# Create bin directory
mkdir -p "$BIN_DIR"

# Function to copy file if it exists
copy_if_exists() {
    local src="$1"
    local dst="$2"

    if [ -f "$src" ]; then
        cp -v "$src" "$dst"
        return 0
    else
        echo "WARNING: File not found: $src"
        return 1
    fi
}

# Copy runtime executables
echo "Copying runtime executables..."
copy_if_exists "$BUILD_DIR/bin/chimaera_start_runtime" "$BIN_DIR/"
copy_if_exists "$BUILD_DIR/bin/chimaera_stop_runtime" "$BIN_DIR/"

# Copy core libraries
echo ""
echo "Copying core libraries..."
copy_if_exists "$BUILD_DIR/bin/libcxx.so" "$BIN_DIR/" || true
copy_if_exists "$BUILD_DIR/bin/libcxx.so.1" "$BIN_DIR/" || true
copy_if_exists "$BUILD_DIR/bin/libcxx.so.1.0.0" "$BIN_DIR/" || true

# Copy admin ChiMod libraries
echo ""
echo "Copying admin ChiMod libraries..."
copy_if_exists "$BUILD_DIR/bin/libchimaera_admin_runtime.so" "$BIN_DIR/" || true
copy_if_exists "$BUILD_DIR/bin/libchimaera_admin_client.so" "$BIN_DIR/" || true

# Copy BDev ChiMod libraries (optional)
echo ""
echo "Copying BDev ChiMod libraries (optional)..."
copy_if_exists "$BUILD_DIR/bin/libchimaera_bdev_runtime.so" "$BIN_DIR/" || true
copy_if_exists "$BUILD_DIR/bin/libchimaera_bdev_client.so" "$BIN_DIR/" || true

# List copied files
echo ""
echo "=== Copied Files ==="
ls -lh "$BIN_DIR/"

# Check if critical files exist
echo ""
echo "=== Verification ==="
MISSING_FILES=0

if [ ! -f "$BIN_DIR/chimaera_start_runtime" ]; then
    echo "ERROR: Missing chimaera_start_runtime"
    MISSING_FILES=$((MISSING_FILES + 1))
fi

if [ ! -f "$BIN_DIR/chimaera_stop_runtime" ]; then
    echo "ERROR: Missing chimaera_stop_runtime"
    MISSING_FILES=$((MISSING_FILES + 1))
fi

if [ ! -f "$BIN_DIR/libcxx.so.1.0.0" ] && [ ! -f "$BIN_DIR/libcxx.so" ]; then
    echo "ERROR: Missing libcxx.so"
    MISSING_FILES=$((MISSING_FILES + 1))
fi

if [ ! -f "$BIN_DIR/libchimaera_admin_runtime.so" ]; then
    echo "ERROR: Missing libchimaera_admin_runtime.so"
    MISSING_FILES=$((MISSING_FILES + 1))
fi

if [ $MISSING_FILES -gt 0 ]; then
    echo ""
    echo "ERROR: $MISSING_FILES critical file(s) missing"
    echo "Build may have failed or files are in unexpected locations"
    exit 1
fi

echo ""
echo "=== Success ==="
echo "All critical binaries copied successfully"
echo "Ready to build Docker image:"
echo "  cd $SCRIPT_DIR"
echo "  docker-compose build"
