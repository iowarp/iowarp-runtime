#!/bin/bash
#
# Build Debian package for chimaera-runtime
# Usage: ./build-deb.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Chimaera Runtime Debian Package Builder ===${NC}\n"

# Check if we're in the right directory
if [ ! -f "chimaera_repo.yaml" ] && [ ! -f "chimods/chimaera_repo.yaml" ]; then
    echo -e "${RED}Error: Must be run from chimaera-runtime root directory${NC}"
    exit 1
fi

# Check for required tools
REQUIRED_TOOLS="dpkg-buildpackage debhelper cmake g++"
for tool in $REQUIRED_TOOLS; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: Required tool '$tool' not found${NC}"
        echo "Install with: sudo apt-get install build-essential debhelper cmake"
        exit 1
    fi
done

# Check for required dependencies
echo -e "${YELLOW}Checking build dependencies...${NC}"
MISSING_DEPS=""
for dep in libboost-fiber-dev libboost-context-dev libzmq3-dev libyaml-cpp-dev libcereal-dev catch2; do
    if ! dpkg -l | grep -q "^ii  $dep"; then
        MISSING_DEPS="$MISSING_DEPS $dep"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo -e "${YELLOW}Missing dependencies:$MISSING_DEPS${NC}"
    echo "Install with: sudo apt-get install$MISSING_DEPS"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Clean previous builds
echo -e "${YELLOW}Cleaning previous build artifacts...${NC}"
rm -rf debian/chimaera-runtime debian/.debhelper debian/files debian/*.substvars
rm -f ../chimaera-runtime_*.deb ../chimaera-runtime_*.buildinfo ../chimaera-runtime_*.changes

# Build the package
echo -e "${GREEN}Building Debian package...${NC}"
dpkg-buildpackage -us -uc -b -j$(nproc)

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo -e "\n${GREEN}=== Build Successful! ===${NC}\n"
    echo "Package files created in parent directory:"
    ls -lh ../chimaera-runtime_*.deb
    echo ""
    echo -e "${GREEN}Install with:${NC}"
    echo "  sudo dpkg -i ../chimaera-runtime_*.deb"
    echo ""
    echo -e "${GREEN}Or install dependencies and package:${NC}"
    echo "  sudo apt-get install -f ../chimaera-runtime_*.deb"
else
    echo -e "\n${RED}=== Build Failed ===${NC}\n"
    exit 1
fi
