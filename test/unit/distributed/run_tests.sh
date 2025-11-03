#!/bin/bash
# Run IOWarp Distributed Unit Test
#
# This script manages the distributed test environment, including:
# - Docker cluster setup
# - Hostfile generation
# - Test execution
# - Cleanup

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Configuration
NUM_NODES=${NUM_NODES:-4}
TEST_CASE=${TEST_CASE:-all}  # all, direct, range, broadcast
RUNTIME_MODE=${RUNTIME_MODE:-docker}  # docker or local

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Generate hostfile
generate_hostfile() {
    log_info "Generating hostfile..."
    cd "$SCRIPT_DIR"
    ./generate_hostfile.sh
    log_success "Hostfile generated"
}

# Start Docker cluster
start_docker_cluster() {
    log_info "Starting Docker cluster with $NUM_NODES nodes..."
    cd "$SCRIPT_DIR"

    # Generate hostfile first
    generate_hostfile

    # Start containers in detached mode
    docker compose up -d

    # Wait for containers to be ready
    log_info "Waiting for containers to initialize..."
    sleep 10

    # Check container status
    docker compose ps

    log_success "Docker cluster started"
    log_info "View live logs with: docker compose logs -f"
}

# Stop Docker cluster
stop_docker_cluster() {
    log_info "Stopping Docker cluster..."
    cd "$SCRIPT_DIR"
    docker compose down
    log_success "Docker cluster stopped"
}

# Build test in Docker
build_test_docker() {
    log_info "Building distributed test in Docker containers..."
    cd "$SCRIPT_DIR"

    # Wait for build to complete (node1 builds on startup)
    log_info "Waiting for build to complete..."
    local max_wait=300  # 5 minutes max
    local elapsed=0

    while [ $elapsed -lt $max_wait ]; do
        if docker exec iowarp-distributed-node1 test -f /iowarp-runtime/build-docker/bin/chimaera_distributed_bdev_tests 2>/dev/null; then
            log_success "Test built successfully in Docker"
            docker exec iowarp-distributed-node1 ls -lh /iowarp-runtime/build-docker/bin/chimaera_distributed_bdev_tests
            return 0
        fi
        sleep 5
        elapsed=$((elapsed + 5))
    done

    log_error "Build timed out after ${max_wait} seconds"
    log_error "Build logs:"
    docker compose logs iowarp-node1
    return 1
}

# Build test locally
build_test_local() {
    log_info "Building distributed test locally..."
    cd "$REPO_ROOT"

    # Build the test
    cmake --preset debug
    cmake --build build --target chimaera_distributed_bdev_tests -j8

    log_success "Test built successfully"
}

# Run test in Docker using Jarvis
run_test_docker() {
    log_info "Running distributed test using Jarvis pipeline: $TEST_CASE"
    cd "$SCRIPT_DIR"

    # Execute test on node1 using Jarvis pipeline
    docker exec iowarp-distributed-node1 bash -c "
        cd /iowarp-runtime/test/jarvis_wrp_runtime &&
        jarvis ppl load yaml pipelines/distributed_test_container.yaml &&
        jarvis pkg conf distributed_test num_nodes=$NUM_NODES test_case=$TEST_CASE &&
        jarvis ppl run
    "

    log_success "Test completed"
}

# Run test locally using Jarvis
run_test_local() {
    log_info "Running distributed test using Jarvis pipeline: $TEST_CASE"
    cd "$REPO_ROOT/test/jarvis_wrp_runtime"

    # Load pipeline and configure
    jarvis ppl load yaml pipelines/distributed_test_local.yaml
    jarvis pkg conf distributed_test num_nodes=$NUM_NODES test_case=$TEST_CASE
    jarvis ppl run

    log_success "Test completed"
}

# Run test directly (without Jarvis) in Docker
run_test_docker_direct() {
    log_info "Running distributed test directly: $TEST_CASE"
    cd "$SCRIPT_DIR"

    # Wait for all runtimes to be ready (give them time to initialize)
    log_info "Waiting for runtimes to initialize across all nodes..."
    sleep 5

    # Execute test on node1
    docker exec iowarp-distributed-node1 bash -c "
        cd /iowarp-runtime/build-docker/bin &&
        ./chimaera_distributed_bdev_tests --num-nodes $NUM_NODES --test-case $TEST_CASE
    "

    log_success "Test completed"
}

# Run test directly (without Jarvis) locally
run_test_local_direct() {
    log_info "Running distributed test directly: $TEST_CASE"
    cd "$REPO_ROOT/build/bin"

    ./chimaera_distributed_bdev_tests --num-nodes $NUM_NODES --test-case $TEST_CASE

    log_success "Test completed"
}

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS] COMMAND

Commands:
    setup       Set up the distributed test environment
    build       Build the distributed test
    run         Run the distributed test (using Jarvis pipeline)
    run_direct  Run the distributed test (direct execution without Jarvis)
    clean       Clean up the test environment
    all         Setup, build, and run (default)

Options:
    -n, --num-nodes NUM     Number of nodes (default: 4)
    -t, --test-case CASE    Test case: all, direct, range, broadcast (default: all)
    -m, --mode MODE         Runtime mode: docker or local (default: docker)
    -h, --help              Show this help message

Environment Variables:
    NUM_NODES       Number of nodes
    TEST_CASE       Test case to run
    RUNTIME_MODE    Runtime mode (docker or local)

Examples:
    # Run all tests with Docker using Jarvis (default 4 nodes)
    $0 all

    # Run specific test case with Jarvis
    $0 -t direct run

    # Run directly without Jarvis
    $0 -t direct run_direct

    # Use 8 nodes
    $0 -n 8 all

    # Local mode with Jarvis
    $0 -m local all

    # Just setup and build
    $0 setup
    $0 build

Execution Modes:
    - 'run' uses Jarvis pipelines (distributed_test_container.yaml or distributed_test_local.yaml)
    - 'run_direct' executes the test binary directly without Jarvis
EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-nodes)
            NUM_NODES="$2"
            shift 2
            ;;
        -t|--test-case)
            TEST_CASE="$2"
            shift 2
            ;;
        -m|--mode)
            RUNTIME_MODE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        setup|build|run|run_direct|clean|all)
            COMMAND="$1"
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Default command
COMMAND=${COMMAND:-all}

# Main execution
log_info "IOWarp Distributed Test Runner"
log_info "Configuration:"
log_info "  Nodes: $NUM_NODES"
log_info "  Test case: $TEST_CASE"
log_info "  Mode: $RUNTIME_MODE"
log_info ""

case $COMMAND in
    setup)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            start_docker_cluster
        else
            generate_hostfile
        fi
        ;;

    build)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            build_test_docker
        else
            build_test_local
        fi
        ;;

    run)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            run_test_docker
        else
            run_test_local
        fi
        ;;

    run_direct)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            run_test_docker_direct
        else
            run_test_local_direct
        fi
        ;;

    clean)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            stop_docker_cluster
        fi
        log_success "Cleanup complete"
        ;;

    all)
        if [ "$RUNTIME_MODE" = "docker" ]; then
            start_docker_cluster
            build_test_docker
            run_test_docker_direct
            stop_docker_cluster
        else
            generate_hostfile
            build_test_local
            run_test_local
        fi
        log_success "All operations completed successfully"
        ;;

    *)
        log_error "Unknown command: $COMMAND"
        usage
        exit 1
        ;;
esac
