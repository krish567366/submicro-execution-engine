#!/bin/bash

# Quick Build and Run Script for macOS
# Demonstrates the HFT system with simulated market data

set -e

echo "════════════════════════════════════════════════════════"
echo "  HFT System - Quick Demo Build (macOS)"
echo "════════════════════════════════════════════════════════"
echo ""

# Check if on macOS
if [[ "$(uname)" != "Darwin" ]]; then
    echo "⚠️  This script is for macOS. Use build_benchmark.sh on Linux."
    echo ""
fi

# Check for compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "❌ No C++ compiler found"
    echo "   Install Xcode Command Line Tools:"
    echo "   xcode-select --install"
    exit 1
fi

# Use clang++ on macOS (comes with Xcode)
CXX="clang++"
if command -v g++-13 &> /dev/null; then
    CXX="g++-13"  # Homebrew g++
elif command -v g++ &> /dev/null; then
    CXX="g++"
fi

echo "Using compiler: $CXX"
echo ""

# Build directory
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"

# Compiler flags (macOS compatible)
CXXFLAGS=(
    "-std=c++17"
    "-O2"                    # O2 instead of O3 for faster compile
    "-march=native"
    "-pthread"
    "-Wall"
    "-Wextra"
    "-I./include"
    "-I/opt/homebrew/include"  # Boost headers (Apple Silicon)
    "-I/usr/local/include"     # Boost headers (Intel Mac)
)

echo "Building trading system demo..."
echo "$CXX ${CXXFLAGS[*]} src/main.cpp -o $BUILD_DIR/trading_demo"
echo ""

$CXX "${CXXFLAGS[@]}" src/main.cpp -o "$BUILD_DIR/trading_demo"

if [[ $? -eq 0 ]]; then
    echo "✅ Build successful!"
    echo ""
    echo "Running demo..."
    echo "════════════════════════════════════════════════════════"
    echo ""
    
    # Run with simulated market data
    ./"$BUILD_DIR/trading_demo"
    
    echo ""
    echo "════════════════════════════════════════════════════════"
    echo "✅ Demo complete!"
    echo ""
    echo "To run again: ./$BUILD_DIR/trading_demo"
    echo ""
else
    echo "❌ Build failed!"
    exit 1
fi
