#!/bin/bash
# Script to run unit tests in Docker container

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== IoWarp Runtime Unit Tests in Docker ===${NC}"

# Parse command line arguments
BUILD_IMAGE=false
REBUILD=false
SPECIFIC_TEST=""
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --build)
            BUILD_IMAGE=true
            shift
            ;;
        --rebuild)
            REBUILD=true
            BUILD_IMAGE=true
            shift
            ;;
        --test)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build          Build Docker image before running tests"
            echo "  --rebuild        Rebuild Docker image from scratch (no cache)"
            echo "  --test <name>    Run specific test by name"
            echo "  --verbose, -v    Enable verbose test output"
            echo "  --help, -h       Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                           # Run all tests using existing image"
            echo "  $0 --build                   # Build image and run all tests"
            echo "  $0 --test test_comutex       # Run specific test"
            echo "  $0 --rebuild --verbose       # Rebuild and run with verbose output"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Build Docker image if requested
if [ "$BUILD_IMAGE" = true ]; then
    echo -e "${YELLOW}Building Docker image...${NC}"
    if [ "$REBUILD" = true ]; then
        docker-compose -f docker-compose.test.yml build --no-cache
    else
        docker-compose -f docker-compose.test.yml build
    fi
fi

# Construct test command
TEST_CMD="cd /workspace/build && "

if [ -n "$SPECIFIC_TEST" ]; then
    echo -e "${YELLOW}Running specific test: ${SPECIFIC_TEST}${NC}"
    TEST_CMD+="ctest -R ${SPECIFIC_TEST}"
else
    echo -e "${YELLOW}Running all tests...${NC}"
    TEST_CMD+="ctest"
fi

if [ "$VERBOSE" = true ]; then
    TEST_CMD+=" --verbose --output-on-failure"
else
    TEST_CMD+=" --output-on-failure"
fi

# Run tests in Docker
echo -e "${GREEN}Starting test execution...${NC}"
docker-compose -f docker-compose.test.yml run --rm test bash -c "$TEST_CMD"

# Check exit code
if [ $? -eq 0 ]; then
    echo -e "${GREEN}=== All tests passed! ===${NC}"
else
    echo -e "${RED}=== Some tests failed ===${NC}"
    exit 1
fi
