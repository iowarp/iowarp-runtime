# Docker Testing for IoWarp Runtime

This directory contains Docker configuration for running unit tests in an isolated environment using pre-built modules from the host system.

## Prerequisites

- Docker installed and running
- Docker Compose installed (usually comes with Docker Desktop)
- Access to `iowarp/iowarp-deps:latest` image
- Pre-built modules in `${HOME}/.ppi-jarvis-mods/`:
  - `cte-hermes-shm` (HermesShm library)
  - `iowarp-runtime` (IoWarp runtime)

## Quick Start

### Run All Tests

```bash
./run-tests-docker.sh
```

### Build and Run Tests

```bash
./run-tests-docker.sh --build
```

### Rebuild from Scratch

```bash
./run-tests-docker.sh --rebuild
```

### Run Specific Test

```bash
./run-tests-docker.sh --test test_comutex
```

### Run with Verbose Output

```bash
./run-tests-docker.sh --verbose
```

## Files

- **Dockerfile.test** - Dockerfile for building the test environment
- **docker-compose.test.yml** - Docker Compose configuration for running tests
- **run-tests-docker.sh** - Helper script for easy test execution
- **.dockerignore** - Files to exclude from Docker build context

## Docker Image

The test container is based on `iowarp/iowarp-deps:latest` and uses pre-built modules from the host system:
- **Base image dependencies**: CMake, GCC/Clang compiler, basic libraries
- **Host-mounted modules**:
  - HermesShm from `${HOME}/.ppi-jarvis-mods/cte-hermes-shm`
  - IoWarp runtime from `${HOME}/.ppi-jarvis-mods/iowarp-runtime`
- **Environment configuration**: PATH and LD_LIBRARY_PATH automatically configured for mounted modules

## Configuration

### Shared Memory

The docker-compose configuration allocates 2GB of shared memory for IPC tests:

```yaml
shm_size: '2gb'
```

Adjust this if your tests require more shared memory.

### Module Mounting

The container mounts pre-built modules from the host system:

```yaml
volumes:
  - ${HOME}/.ppi-jarvis-mods:/ppi-jarvis-mods  # Pre-built modules
  - .:/workspace                                # Source and build directories
```

This approach:
- Uses pre-built binaries and libraries from the host
- Eliminates container build time
- Ensures consistency with host build environment
- Allows quick test iterations

### Environment Variables

The container automatically configures environment variables for mounted modules:

```yaml
environment:
  - HERMES_SHM_DIR=/ppi-jarvis-mods/cte-hermes-shm
  - IOWARP_DIR=/ppi-jarvis-mods/iowarp-runtime
  - CMAKE_PREFIX_PATH=/ppi-jarvis-mods/cte-hermes-shm:/ppi-jarvis-mods/iowarp-runtime:/usr/local
```

PATH and LD_LIBRARY_PATH are also configured in the Dockerfile to include bin and lib directories from both modules.

## Manual Docker Commands

If you prefer manual control:

### Build Image

```bash
docker build -f Dockerfile.test -t iowarp-runtime-test .
```

### Run Tests

```bash
docker run --rm --shm-size=2gb \
  -v ${HOME}/.ppi-jarvis-mods:/ppi-jarvis-mods \
  -v $(pwd):/workspace \
  iowarp-runtime-test
```

### Interactive Shell

```bash
docker run --rm -it --shm-size=2gb \
  -v ${HOME}/.ppi-jarvis-mods:/ppi-jarvis-mods \
  -v $(pwd):/workspace \
  iowarp-runtime-test bash
```

### Run Specific Test

```bash
docker run --rm --shm-size=2gb \
  -v ${HOME}/.ppi-jarvis-mods:/ppi-jarvis-mods \
  -v $(pwd):/workspace \
  iowarp-runtime-test bash -c "cd /workspace/build && ctest -R test_comutex --verbose"
```

## Troubleshooting

### Out of Memory

If tests fail with memory errors, increase shared memory:

```bash
docker run --rm --shm-size=4gb iowarp-runtime-test
```

### Missing Pre-built Modules

Ensure `${HOME}/.ppi-jarvis-mods/` contains pre-built modules:

```bash
ls ${HOME}/.ppi-jarvis-mods/
# Should show: cte-hermes-shm  iowarp-runtime
```

If modules are missing, build them on the host system before running Docker tests.

### Permission Issues

If you encounter permission issues with mounted volumes, ensure your user has proper Docker permissions:

```bash
sudo usermod -aG docker $USER
```

Then log out and back in.

### Network Issues

If tests require network access, enable host networking:

```yaml
network_mode: host
```

## CI/CD Integration

This Docker setup can be easily integrated into CI/CD pipelines:

### GitHub Actions Example

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run tests in Docker
        run: |
          docker-compose -f docker-compose.test.yml run --rm test
```

### GitLab CI Example

```yaml
test:
  image: docker:latest
  services:
    - docker:dind
  script:
    - docker-compose -f docker-compose.test.yml run --rm test
```

## Performance Tips

1. **Pre-build modules** - Build HermesShm and IoWarp runtime on host before running tests
2. **Mount volumes** - Mounted directories provide instant access to binaries
3. **No container builds** - Container uses pre-built code, eliminating build time
4. **Shared memory** - Configure shm_size based on test requirements

## Cleaning Up

Remove containers:

```bash
docker-compose -f docker-compose.test.yml down
```

Remove test image:

```bash
docker rmi iowarp-runtime-test
```

Remove all Docker build cache:

```bash
docker builder prune -a
```
