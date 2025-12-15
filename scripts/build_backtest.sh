#!/bin/bash

# Build script for backtesting engine

echo "════════════════════════════════════════════════════════════════"
echo "  Building HFT Backtesting Engine"
echo "════════════════════════════════════════════════════════════════"
echo ""

# Compiler settings
CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O3 -march=native -pthread -Wall -Wextra"
INCLUDES="-I./include"
OUTPUT="backtest_demo"

# Source files
SOURCES="src/backtest_demo.cpp"

echo "Compiler:  $CXX"
echo "Flags:     $CXXFLAGS"
echo "Output:    $OUTPUT"
echo ""

# Compile
echo "Compiling..."
$CXX $CXXFLAGS $INCLUDES $SOURCES -o $OUTPUT

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo ""
    echo "Run with: ./$OUTPUT"
    echo ""
else
    echo ""
    echo "✗ Build failed!"
    exit 1
fi
