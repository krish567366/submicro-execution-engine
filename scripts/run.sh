#!/bin/bash

# Ultra-Low-Latency HFT System - Production Run Script
# Configures system for optimal performance before execution

set -e

echo "========================================"
echo "HFT System Production Runner"
echo "========================================"
echo ""

# Check if running as root (needed for some optimizations)
if [ "$EUID" -ne 0 ]; then 
    echo "⚠️  Not running as root. Some optimizations will be skipped."
    echo "   Run with sudo for best performance."
    echo ""
    SUDO=""
else
    SUDO=""
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Step 1: Checking if binary exists..."
if [ ! -f "./build/hft_system" ]; then
    echo -e "${RED}Error: hft_system not found. Build first with ./build.sh${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Binary found${NC}"
echo ""

echo "Step 2: Configuring system for low latency..."

# Disable CPU frequency scaling
echo "  - Setting CPU governor to 'performance'"
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        echo performance > "$cpu" 2>/dev/null || true
    done
    echo -e "${GREEN}  ✓ CPU governor set${NC}"
else
    echo -e "${YELLOW}  ⊘ Skipped (needs root)${NC}"
fi

# Configure huge pages
echo "  - Configuring huge pages (2MB)"
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    echo 512 > /proc/sys/vm/nr_hugepages 2>/dev/null || true
    HUGEPAGES=$(cat /proc/sys/vm/nr_hugepages)
    echo -e "${GREEN}  ✓ Huge pages: $HUGEPAGES${NC}"
else
    echo -e "${YELLOW}  ⊘ Skipped (needs root)${NC}"
fi

# Disable transparent huge pages (THP) - creates latency spikes
echo "  - Disabling transparent huge pages"
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    echo never > /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null || true
    echo -e "${GREEN}  ✓ THP disabled${NC}"
else
    echo -e "${YELLOW}  ⊘ Skipped (needs root)${NC}"
fi

# Disable swap
echo "  - Disabling swap"
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    swapoff -a 2>/dev/null || true
    echo -e "${GREEN}  ✓ Swap disabled${NC}"
else
    echo -e "${YELLOW}  ⊘ Skipped (needs root)${NC}"
fi

# Set real-time priority
echo "  - Configuring real-time scheduling"
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    ulimit -r unlimited 2>/dev/null || true
    echo -e "${GREEN}  ✓ RT priority limit raised${NC}"
else
    echo -e "${YELLOW}  ⊘ Skipped (needs root)${NC}"
fi

# Increase locked memory limit
echo "  - Increasing locked memory limit"
ulimit -l unlimited 2>/dev/null || true
echo -e "${GREEN}  ✓ Memory lock limit raised${NC}"

echo ""
echo "Step 3: System information..."
echo "  CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
echo "  Cores: $(nproc)"
echo "  Memory: $(free -h | awk '/^Mem:/ {print $2}')"
echo "  Governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'unknown')"
echo ""

echo "========================================"
echo "Starting HFT System..."
echo "========================================"
echo ""

# Pin to CPU core 0 and set real-time priority
if [ -n "$SUDO" ] || [ "$EUID" -eq 0 ]; then
    echo "Running with:"
    echo "  - CPU affinity: Core 0"
    echo "  - RT priority: 99 (SCHED_FIFO)"
    echo "  - Memory: Locked in RAM"
    echo ""
    
    # Use chrt for real-time scheduling and taskset for CPU affinity
    chrt -f 99 taskset -c 0 ./build/hft_system
else
    echo "Running with:"
    echo "  - CPU affinity: Core 0"
    echo "  - Standard priority (use sudo for RT)"
    echo ""
    
    # Just CPU affinity
    taskset -c 0 ./build/hft_system
fi

echo ""
echo "========================================"
echo "System Shutdown Complete"
echo "========================================"
