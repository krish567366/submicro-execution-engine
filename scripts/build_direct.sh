#!/bin/bash

# Direct build script without CMake
# Compiles backtesting demo with institutional logging

set -e

echo "========================================================================"
echo "  Building Backtesting Demo (Direct Compilation)"
echo "========================================================================"
echo ""

# Check for data file
if [ ! -f "synthetic_ticks_with_alpha.csv" ]; then
    echo "⚠️  Generating synthetic data..."
    python3 generate_alpha_data.py
    echo ""
fi

# Create logs directory
mkdir -p logs
mkdir -p build

echo "Compiling with g++..."
echo "  • Optimization: -O3 -march=native"
echo "  • Standard: C++17"
echo "  • Libraries: pthread, ssl, crypto"
echo ""

# Detect OpenSSL path (macOS Homebrew)
OPENSSL_PREFIX=$(brew --prefix openssl 2>/dev/null || echo "/usr/local/opt/openssl")

# Compile
g++ -std=c++17 -O3 -march=native \
    -I./include \
    -I"$OPENSSL_PREFIX/include" \
    -L"$OPENSSL_PREFIX/lib" \
    -o build/backtest_demo \
    src/backtest_demo.cpp \
    -pthread -lssl -lcrypto \
    -DNDEBUG

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Running demo..."
    echo "========================================================================"
    echo ""
    
    ./build/backtest_demo
    
    echo ""
    echo "========================================================================"
    echo "  LOGS GENERATED"
    echo "========================================================================"
    echo ""
    
    if [ -f "logs/backtest_replay.log" ]; then
        echo "✓ Event Replay Log: logs/backtest_replay.log"
        echo "  Lines: $(wc -l < logs/backtest_replay.log)"
    fi
    
    if [ -f "logs/risk_breaches.log" ]; then
        echo "✓ Risk Breach Log: logs/risk_breaches.log"
        echo "  Lines: $(wc -l < logs/risk_breaches.log)"
    fi
    
    if [ -f "logs/system_verification.log" ]; then
        echo "✓ System Verification: logs/system_verification.log"
    fi
    
    echo ""
else
    echo "❌ Build failed!"
    exit 1
fi
