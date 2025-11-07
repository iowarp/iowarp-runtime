# Development Dockerfile for IOWarp Runtime
# Builds the Chimaera runtime in release mode
FROM iowarp/cte-hermes-shm-build:latest

# Copy source code
COPY . /workspace
WORKDIR /workspace

# Configure with release preset and build
# Install to both /usr/local and /iowarp-runtime for flexibility
RUN sudo chown -R $(whoami):$(whoami) /workspace && \
    mkdir -p build && \
    cmake --preset release && \
    cmake --build build -j$(nproc) && \
    cmake --install build --prefix /usr/local && \
    cmake --install build --prefix /iowarp-runtime && \
    rm -rf /workspace

# Add iowarp-runtime to Spack configuration
RUN echo "  iowarp-runtime:" >> ~/.spack/packages.yaml && \
    echo "    externals:" >> ~/.spack/packages.yaml && \
    echo "    - spec: iowarp-runtime@main" >> ~/.spack/packages.yaml && \
    echo "      prefix: /usr/local" >> ~/.spack/packages.yaml && \
    echo "    buildable: false" >> ~/.spack/packages.yaml