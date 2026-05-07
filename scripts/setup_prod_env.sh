#!/bin/bash

# ============================================================================
# PRODUCTION ENVIRONMENT SETUP SCRIPT
# ============================================================================
# This script configures the Linux OS for sub-microsecond determinism.
# MUST BE RUN AS ROOT.
# ============================================================================

set -e

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
fi

echo "--- Optimizing System for Low Latency ---"

# 1. CPU Power Management
# Set governor to performance to prevent down-clocking
echo "performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Disable Intel Turbo Boost (prevents thermal jitter)
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo "1" > /sys/devices/system/cpu/intel_pstate/no_turbo
fi

# 2. Hugepage Allocation (2MB pages)
# Needed for large lock-free buffers and LOB reconstruction
echo "1024" > /proc/sys/vm/nr_hugepages
echo "Pinned 1024 x 2MB Hugepages"

# 3. Network Interrupt Affinity
# Move interrupts away from our trading core (assuming core 1 is for trading)
# This is a simplified version, requires mapping interface IRQs
# for irq in $(grep eth0 /proc/interrupts | cut -d: -f1); do
#    echo "1" > /proc/irq/$irq/smp_affinity_list # Move to core 0
# done

# 4. Kernel Network Tuning
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.core.netdev_max_backlog=10000

# 5. Disable Swap (Avoid page faults)
swapoff -a

echo "--- Setup Complete ---"
echo "RECOMMENDED: Add 'isolcpus=1' to your GRUB_CMDLINE_LINUX_DEFAULT"
echo "to truly isolate the trading core from the Linux scheduler."
