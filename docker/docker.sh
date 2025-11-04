#!/bin/bash

# Build iowarp-runtime Docker image

# Get the project root directory (parent of docker folder)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/.." && pwd )"

echo $PROJECT_ROOT
echo $SCRIPT_DIR
# Build the Docker image
docker build  --no-cache -t iowarp/iowarp-runtime:latest -f "${SCRIPT_DIR}/Dockerfile.build" "${PROJECT_ROOT}"
