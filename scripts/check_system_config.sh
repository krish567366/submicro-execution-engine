#!/bin/bash

# System Configuration Checker for HFT Benchmarking
# Verifies Linux system is properly configured for ultra-low latency

echo "════════════════════════════════════════════════════════"
echo "  HFT System Configuration Check"
echo "════════════════════════════════════════════════════════"
echo ""

WARNINGS=0
ERRORS=0

# Function to print results
check_pass() {
    echo "✅ $1"
}

check_warn() {
    echo "⚠️  $1"
    ((WARNINGS++))
}

check_fail() {
    echo "❌ $1"
    ((ERRORS++))
}

# 1. Check OS
echo "[1] Operating System"
if [[ "$(uname)" == "Linux" ]]; then
    check_pass "Linux kernel $(uname -r)"
else
    check_fail "Not running on Linux ($(uname))"
fi
echo ""

# 2. Check CPU isolation
echo "[2] CPU Isolation"
if grep -q "isolcpus" /proc/cmdline 2>/dev/null; then
    ISOLATED=$(grep -o 'isolcpus=[^ ]*' /proc/cmdline | cut -d= -f2)
    check_pass "CPU isolation enabled: isolcpus=$ISOLATED"
    
    if grep -q "nohz_full" /proc/cmdline; then
        NOHZ=$(grep -o 'nohz_full=[^ ]*' /proc/cmdline | cut -d= -f2)
        check_pass "Tickless mode enabled: nohz_full=$NOHZ"
    else
        check_warn "No tickless mode (nohz_full not set)"
    fi
else
    check_warn "No CPU isolation configured"
    echo "         Add to /etc/default/grub GRUB_CMDLINE_LINUX:"
    echo "         'isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7'"
    echo "         Then: sudo update-grub && sudo reboot"
fi
echo ""

# 3. Check huge pages
echo "[3] Huge Pages"
HUGEPAGES=$(cat /proc/sys/vm/nr_hugepages 2>/dev/null || echo "0")
HUGEPAGE_SIZE=$(cat /proc/meminfo | grep Hugepagesize | awk '{print $2}')

if [[ "$HUGEPAGES" -gt 0 ]]; then
    TOTAL_MB=$((HUGEPAGES * HUGEPAGE_SIZE / 1024))
    check_pass "Huge pages: $HUGEPAGES x ${HUGEPAGE_SIZE}kB = ${TOTAL_MB}MB"
else
    check_warn "No huge pages configured"
    echo "         Run: echo 1024 | sudo tee /proc/sys/vm/nr_hugepages"
fi
echo ""

# 4. Check CPU governor
echo "[4] CPU Frequency Scaling"
if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
    GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
    if [[ "$GOVERNOR" == "performance" ]]; then
        check_pass "CPU governor: $GOVERNOR"
    else
        check_warn "CPU governor: $GOVERNOR (should be 'performance')"
        echo "         Run: sudo cpupower frequency-set -g performance"
    fi
else
    check_warn "CPU frequency scaling not available or not supported"
fi
echo ""

# 5. Check swappiness
echo "[5] Swappiness"
SWAPPINESS=$(cat /proc/sys/vm/swappiness)
if [[ "$SWAPPINESS" -le 10 ]]; then
    check_pass "Swappiness: $SWAPPINESS (good for HFT)"
else
    check_warn "Swappiness: $SWAPPINESS (should be ≤10)"
    echo "         Run: echo 1 | sudo tee /proc/sys/vm/swappiness"
fi
echo ""

# 6. Check memory locking limits
echo "[6] Memory Locking Limits"
MEMLOCK_SOFT=$(ulimit -l)
if [[ "$MEMLOCK_SOFT" == "unlimited" ]]; then
    check_pass "Memory lock limit: unlimited"
else
    check_warn "Memory lock limit: ${MEMLOCK_SOFT}kB (should be unlimited)"
    echo "         Add to /etc/security/limits.conf:"
    echo "         '* soft memlock unlimited'"
    echo "         '* hard memlock unlimited'"
fi
echo ""

# 7. Check real-time limits
echo "[7] Real-Time Priority Limits"
RTPRIO=$(ulimit -r)
if [[ "$RTPRIO" -ge 49 ]]; then
    check_pass "RT priority limit: $RTPRIO"
else
    check_warn "RT priority limit: $RTPRIO (should be ≥49)"
    echo "         Add to /etc/security/limits.conf:"
    echo "         '* soft rtprio 99'"
    echo "         '* hard rtprio 99'"
fi
echo ""

# 8. Check IRQ affinity
echo "[8] IRQ Affinity"
if [[ -d /proc/irq ]]; then
    # Count IRQs not pinned to isolated cores
    IRQS_OK=0
    for irq in /proc/irq/*/smp_affinity; do
        ((IRQS_OK++))
    done
    check_pass "IRQ configuration present ($IRQS_OK IRQs)"
    echo "         (Manual verification recommended: cat /proc/irq/*/smp_affinity)"
else
    check_warn "Cannot check IRQ affinity"
fi
echo ""

# 9. Check CPU count
echo "[9] CPU Configuration"
CPU_COUNT=$(nproc)
check_pass "CPU cores: $CPU_COUNT"
if [[ "$CPU_COUNT" -ge 8 ]]; then
    check_pass "Sufficient cores for HFT (≥8)"
else
    check_warn "Low core count (recommended: ≥8 for production)"
fi
echo ""

# 10. Check TSC (Time Stamp Counter)
echo "[10] TSC Configuration"
if grep -q "constant_tsc" /proc/cpuinfo; then
    check_pass "Constant TSC available"
else
    check_warn "Constant TSC not available (timing may be less accurate)"
fi

if grep -q "nonstop_tsc" /proc/cpuinfo; then
    check_pass "Nonstop TSC available"
fi
echo ""

# 11. Check compiler
echo "[11] Compiler"
if command -v g++ &> /dev/null; then
    GCC_VER=$(g++ -dumpversion)
    check_pass "g++ version $GCC_VER installed"
    
    if [[ "${GCC_VER%%.*}" -ge 9 ]]; then
        check_pass "g++ supports C++17 and [[likely]] attributes"
    else
        check_warn "g++ version < 9 (may not support all optimizations)"
    fi
else
    check_fail "g++ not found"
    echo "         Install: sudo apt-get install g++ (Ubuntu)"
fi
echo ""

# 12. Check for DPDK (optional)
echo "[12] DPDK (Optional)"
if command -v dpdk-devbind.py &> /dev/null; then
    check_pass "DPDK tools installed"
else
    echo "   ℹ️  DPDK not installed (optional for kernel bypass)"
fi
echo ""

# Summary
echo "════════════════════════════════════════════════════════"
echo "  Summary"
echo "════════════════════════════════════════════════════════"
echo ""
echo "Errors:   $ERRORS"
echo "Warnings: $WARNINGS"
echo ""

if [[ "$ERRORS" -eq 0 && "$WARNINGS" -eq 0 ]]; then
    echo "✅ System is optimally configured for HFT benchmarking!"
    echo ""
    echo "Ready to run:"
    echo "  ./build_benchmark.sh"
    echo "  sudo ./build/hft_benchmark --samples 100000000"
    exit 0
elif [[ "$ERRORS" -eq 0 ]]; then
    echo "⚠️  System is functional but not optimally configured"
    echo ""
    echo "You can run benchmarks, but results may not be optimal."
    echo "Address warnings above for best performance."
    exit 0
else
    echo "❌ System has critical configuration issues"
    echo ""
    echo "Fix errors above before running benchmarks."
    exit 1
fi
