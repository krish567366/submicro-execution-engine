#!/bin/bash

# Build and Run Script for Ultra-Low-Latency HFT System
# This script builds the system with aggressive optimizations

set -e  # Exit on error

echo "========================================"
echo "Ultra-Low-Latency HFT System Builder"
echo "========================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for required tools
echo "Checking prerequisites..."

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake not found. Please install CMake 3.15+${NC}"
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}ERROR: No C++ compiler found. Please install GCC or Clang${NC}"
    exit 1
fi

echo -e "${GREEN}Prerequisites satisfied${NC}"
echo ""

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

echo ""
echo "========================================"
echo "Configuring CMake..."
echo "========================================"

# Configure with Release mode (aggressive optimizations)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo ""
echo "========================================"
echo "Building with optimizations..."
echo "========================================"
echo "Flags: -O3 -march=native -flto -ffast-math"
echo ""

# Build with all available cores
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}========================================"
    echo "Build successful!"
    echo -e "========================================${NC}"
    echo ""
    echo "Executable: build/hft_system"
    echo ""
    echo "To run the system:"
    echo "  ./hft_system"
    echo ""
    echo "Performance Tips:"
    echo "  1. Disable frequency scaling:"
    echo "     sudo cpupower frequency-set -g performance"
    echo ""
    echo "  2. Pin to specific CPU core:"
    echo "     taskset -c 0 ./hft_system"
    echo ""
    echo "  3. Set real-time priority (requires sudo):"
    echo "     sudo chrt -f 99 ./hft_system"
    echo ""
    echo "  4. Use huge pages (requires sudo):"
    echo "     sudo sysctl -w vm.nr_hugepages=512"
    echo ""
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
