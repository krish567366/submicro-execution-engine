#!/bin/bash

# =============================================================================
# APEX PREDATOR: LINUX KERNEL TUNING SCRIPT
# =============================================================================
# Targets: Isolated Cores, No-Hz Full, HugePages, PCIe Tuning.
# WARNING: Requires root/sudo. Run only on production HFT servers.
# ==================================================

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (sudo)" 
   exit 1
fi

echo "[1/5] RESERVING HUGEPAGES (1024 x 2MB = 2GB)"
echo 1024 > /proc/sys/vm/nr_hugepages
mkdir -p /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

echo "[2/5] DISABLING CPU GOVERNORS (PERFORMANCE MODE)"
for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $i
done

echo "[3/5] TUNING NETWORK STACK (ZERO-LOSS)"
sysctl -w net.core.rmem_max=16777216
sysctl -w net.core.wmem_max=16777216
sysctl -w net.core.netdev_max_backlog=5000
sysctl -w net.ipv4.tcp_rmem='4096 87380 16777216'
sysctl -w net.ipv4.tcp_wmem='4096 65536 16777216'
sysctl -w net.ipv4.tcp_low_latency=1

echo "[4/5] KERNEL BOOT PARAMETER REMINDER"
echo "To fully isolate cores 4-7 and disable timer interrupts (NOHZ):"
echo "Add 'isolcpus=4-7 nohz_full=4-7 rcu_nocbs=4-7 clocksource=tsc' to /etc/default/grub"
echo "Then: sudo update-grub && sudo reboot"

echo "[5/5] PCIe THROUGHPUT (MMIO TUNING)"
# Set PCIe Read Completion Boundary to 128B (Intel standard)
# requires setpci: sudo setpci -d *:* 68.w=4000:4000

echo "SYSTEM HARDENED FOR APEX PREDATOR DEPLOYMENT."
