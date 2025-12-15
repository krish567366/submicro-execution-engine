#!/bin/bash

# HFT Benchmark Build Script for Linux Production Servers
# Run on Ubuntu 20.04+ / RHEL 8+ / Amazon Linux 2

set -e  # Exit on error

echo "════════════════════════════════════════════════════════"
echo "  HFT Benchmark Suite - Linux Build Script"
echo "════════════════════════════════════════════════════════"
echo ""

# Check if running on Linux
if [[ "$(uname)" != "Linux" ]]; then
    echo "❌ Error: This script must run on Linux"
    echo "   Current OS: $(uname)"
    exit 1
fi

echo "✅ Running on Linux: $(uname -r)"
echo ""

# Check compiler
if ! command -v g++ &> /dev/null; then
    echo "❌ Error: g++ not found"
    echo "   Install: sudo apt-get install g++ (Ubuntu) or sudo yum install gcc-c++ (RHEL)"
    exit 1
fi

GCC_VERSION=$(g++ --version | head -n1)
echo "✅ Compiler: $GCC_VERSION"
echo ""

# Check for required kernel features
echo "Checking kernel configuration..."

# Check if isolcpus is configured
if grep -q "isolcpus" /proc/cmdline 2>/dev/null; then
    ISOLATED_CPUS=$(grep -o 'isolcpus=[^ ]*' /proc/cmdline | cut -d= -f2)
    echo "✅ CPU isolation enabled: isolcpus=$ISOLATED_CPUS"
else
    echo "⚠️  Warning: No CPU isolation configured"
    echo "   Recommended: Add 'isolcpus=2-7 nohz_full=2-7' to /etc/default/grub"
fi

# Check huge pages
HUGEPAGES=$(cat /proc/sys/vm/nr_hugepages 2>/dev/null || echo "0")
if [[ "$HUGEPAGES" -gt 0 ]]; then
    echo "✅ Huge pages configured: $HUGEPAGES pages"
else
    echo "⚠️  Warning: No huge pages configured"
    echo "   Run: echo 1024 | sudo tee /proc/sys/vm/nr_hugepages"
fi

echo ""

# Build configuration
BUILD_TYPE="${1:-Release}"
OUTPUT_DIR="build"
BINARY_NAME="hft_benchmark"

echo "Build configuration:"
echo "  Type: $BUILD_TYPE"
echo "  Output: $OUTPUT_DIR/$BINARY_NAME"
echo ""

# Create build directory
mkdir -p "$OUTPUT_DIR"

# Compiler flags
CXXFLAGS=(
    "-std=c++17"
    "-O3"
    "-march=native"
    "-mtune=native"
    "-pthread"
    "-Wall"
    "-Wextra"
)

# Additional flags for Release build
if [[ "$BUILD_TYPE" == "Release" ]]; then
    CXXFLAGS+=(
        "-DNDEBUG"
        "-flto"                    # Link-time optimization
        "-ffast-math"              # Fast math operations
        "-funroll-loops"           # Aggressive loop unrolling
        "-finline-functions"       # Aggressive inlining
        "-fomit-frame-pointer"     # Remove frame pointer
        "-fno-exceptions"          # Disable exceptions
        "-fno-rtti"                # Disable RTTI
    )
fi

# Include directories
INCLUDES=(
    "-I./include"
)

# Source files
SOURCES=(
    "benchmark_main.cpp"
)

# Build command
echo "Compiling..."
echo "g++ ${CXXFLAGS[*]} ${INCLUDES[*]} ${SOURCES[*]} -o $OUTPUT_DIR/$BINARY_NAME"
echo ""

g++ "${CXXFLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" -o "$OUTPUT_DIR/$BINARY_NAME"

if [[ $? -eq 0 ]]; then
    echo "✅ Build successful!"
    echo ""
    
    # Set capabilities for real-time priority (without sudo)
    if command -v setcap &> /dev/null; then
        echo "Setting capabilities for real-time execution..."
        sudo setcap cap_sys_nice,cap_ipc_lock,cap_sys_rawio=+ep "$OUTPUT_DIR/$BINARY_NAME"
        echo "✅ Capabilities set"
    else
        echo "⚠️  setcap not found - will need sudo to run benchmark"
    fi
    
    echo ""
    echo "Binary: $OUTPUT_DIR/$BINARY_NAME"
    echo "Size: $(du -h $OUTPUT_DIR/$BINARY_NAME | cut -f1)"
    echo ""
    
    # Print next steps
    echo "════════════════════════════════════════════════════════"
    echo "  Next Steps:"
    echo "════════════════════════════════════════════════════════"
    echo ""
    echo "1. Quick test (1M samples):"
    echo "   sudo ./$OUTPUT_DIR/$BINARY_NAME --samples 1000000"
    echo ""
    echo "2. Production benchmark (100M samples):"
    echo "   sudo ./$OUTPUT_DIR/$BINARY_NAME --samples 100000000 --output prod_results"
    echo ""
    echo "3. Component benchmarks only:"
    echo "   sudo ./$OUTPUT_DIR/$BINARY_NAME --components"
    echo ""
    echo "4. System requirements check:"
    echo "   ./check_system_config.sh"
    echo ""
    
else
    echo "❌ Build failed!"
    exit 1
fi
