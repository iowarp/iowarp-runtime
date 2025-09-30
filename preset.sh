#!/bin/bash

# Get the target prefix path
IOWARP_PREFIX="$HOME/.ppi-jarvis-mods/packages/iowarp-runtime"

# Copy the template CMakePresets.json to the root directory
cp .vscode/CMakePresets.json CMakePresets.json

# Replace IOWARP_PREFIX placeholder with the actual path
sed -i "s|IOWARP_PREFIX|${IOWARP_PREFIX}|g" CMakePresets.json

echo "Created CMakePresets.json with IOWARP_PREFIX set to: ${IOWARP_PREFIX}"
