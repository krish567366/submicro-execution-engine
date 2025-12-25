# HFT Benchmark Suite - Linux Deployment Guide

## 📋 Overview

This guide will help you deploy and run the HFT benchmark suite on Linux production servers.

**Target Performance:** 730 ns (0.73 μs) tick-to-trade latency

---

## System Requirements

### Minimum Requirements
- **OS:** Linux kernel 4.0+ (Ubuntu 20.04+, RHEL 8+, Amazon Linux 2)
- **CPU:** x86_64 with TSC support, 4+ cores
- **RAM:** 8 GB
- **Compiler:** g++ 9.0+ (for C++17 and optimization flags)

### Recommended for Production
- **CPU:** Intel Xeon or AMD EPYC, 8+ cores (2.5+ GHz)
- **RAM:** 32 GB with ECC
- **Network:** Mellanox ConnectX-5/6 or Solarflare X2 (for hardware timestamping)
- **Storage:** NVMe SSD (for log files)

---

## 📦 Step 1: Transfer Files to Linux Server

### Option A: Git Clone (Recommended)
```bash
# On Linux server
cd ~
git clone <your-repo-url> trading-system
cd trading-system
```

### Option B: SCP from Mac
```bash
# On your Mac
cd "/Users/krishnabajpai/code/research codes/new-trading-system"
scp -r . user@linux-server:~/trading-system/
```

### Option C: Create tar archive
```bash
# On your Mac
cd "/Users/krishnabajpai/code/research codes/new-trading-system"
tar czf hft-benchmark.tar.gz \
    include/ \
    benchmark_main.cpp \
    build_benchmark.sh \
    check_system_config.sh \
    BENCHMARK_GUIDE.md

# Transfer to Linux
scp hft-benchmark.tar.gz user@linux-server:~/

# On Linux server
tar xzf hft-benchmark.tar.gz
cd hft-benchmark/
```

---

## Step 2: System Configuration (Run as root)

### 2.1 Check Current Configuration
```bash
chmod +x check_system_config.sh
./check_system_config.sh
```

### 2.2 Configure CPU Isolation

Edit GRUB configuration:
```bash
sudo vi /etc/default/grub
```

Add/modify `GRUB_CMDLINE_LINUX`:
```
GRUB_CMDLINE_LINUX="isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7"
```

Update GRUB and reboot:
```bash
# Ubuntu/Debian
sudo update-grub
sudo reboot

# RHEL/CentOS
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
sudo reboot
```

Verify after reboot:
```bash
cat /proc/cmdline | grep isolcpus
# Should show: isolcpus=2-7 nohz_full=2-7 rcu_nocbs=2-7
```

### 2.3 Configure Huge Pages

```bash
# Allocate 1024 huge pages (2MB each = 2GB total)
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages

# Make permanent (add to /etc/sysctl.conf)
echo "vm.nr_hugepages = 1024" | sudo tee -a /etc/sysctl.conf

# Verify
cat /proc/meminfo | grep Huge
```

### 2.4 Set CPU Governor to Performance

```bash
# Install cpupower (if not present)
sudo apt-get install linux-tools-common linux-tools-$(uname -r)  # Ubuntu
# OR
sudo yum install kernel-tools  # RHEL

# Set performance governor
sudo cpupower frequency-set -g performance

# Verify
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# All should show: performance
```

### 2.5 Configure Swappiness

```bash
# Set swappiness to 1 (minimize swapping)
echo 1 | sudo tee /proc/sys/vm/swappiness

# Make permanent
echo "vm.swappiness = 1" | sudo tee -a /etc/sysctl.conf
```

### 2.6 Set Memory Lock Limits

Edit `/etc/security/limits.conf`:
```bash
sudo vi /etc/security/limits.conf
```

Add these lines:
```
* soft memlock unlimited
* hard memlock unlimited
* soft rtprio 99
* hard rtprio 99
```

Log out and back in for changes to take effect.

### 2.7 Disable Unnecessary Services

```bash
# Stop unnecessary services that cause interrupts
sudo systemctl stop irqbalance
sudo systemctl disable irqbalance

# Optional: disable other services
sudo systemctl stop bluetooth
sudo systemctl stop cups
```

---

## 🔨 Step 3: Build the Benchmark

```bash
# Make build script executable
chmod +x build_benchmark.sh

# Build (Release mode)
./build_benchmark.sh

# Or build with debug symbols
./build_benchmark.sh Debug
```

Expected output:
```
 Build successful!
 Capabilities set
Binary: build/hft_benchmark
Size: 2.1M
```

---

##  Step 4: Run Benchmarks

### 4.1 Quick Test (1M samples, ~10 seconds)

```bash
sudo ./build/hft_benchmark --samples 1000000 --output test_results
```

### 4.2 Component Benchmarks Only

```bash
sudo ./build/hft_benchmark --components
```

This will benchmark:
- Packet parser (~20 ns)
- Order book update (~30 ns)
- Hawkes engine (~50 ns)
- FPGA inference (~400 ns)
- Strategy calculation (~70 ns)
- Risk checks (~20 ns)
- Lock-free queue ops (~30 ns)

### 4.3 Production Benchmark (100M samples, ~10-20 minutes)

```bash
# Full benchmark with 100M samples
sudo ./build/hft_benchmark --samples 100000000 --output prod_results

# Monitor progress
# You'll see: Progress: 0%...100%
```

### 4.4 Full System Benchmark Only

```bash
sudo ./build/hft_benchmark --full --samples 50000000 --output full_system
```

---

## Step 5: Analyze Results

### 5.1 View Console Output

The benchmark will print:
```
═══ TICK-TO-TRADE LATENCY ═══
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
min      =   680.23 ns  (0.68 μs)
mean     =   732.45 ns  (0.73 μs)
median   =   729.11 ns  (0.73 μs)
p90      =   745.67 ns  (0.75 μs)
p99      =   781.23 ns  (0.78 μs)
p999     =   823.45 ns  (0.82 μs)
p9999    =   891.12 ns  (0.89 μs)
max      =   1023.56 ns (1.02 μs)
jitter   =   343.33 ns  (0.34 μs)
stddev   =   42.11 ns   (0.04 μs)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 5.2 CSV Output Files

Three files will be generated:

**1. `prod_results_total.csv`** - Overall statistics
```csv
metric,value_ns,value_us
min,680.23,0.68
mean,732.45,0.73
median,729.11,0.73
p90,745.67,0.75
p99,781.23,0.78
...
```

**2. `prod_results_components.csv`** - Component breakdown
```csv
component,mean_ns,p99_ns,max_ns,percent
RX DMA → App,30.12,45.67,89.23,4.1
Parse Packet,20.34,32.11,67.89,2.8
LOB Update,29.87,41.23,78.45,4.1
...
```

**3. `prod_results_raw_samples.csv`** - Raw data (large file!)
```csv
sample_id,total_ns,rx_dma_ns,parse_ns,lob_ns,...
0,728.45,30.12,20.34,29.87,...
1,731.23,29.87,21.11,30.45,...
...
```

### 5.3 Analyze with Python/Excel

```bash
# View first 10 rows
head -n 10 prod_results_total.csv

# Plot histogram (requires python + matplotlib)
python3 plot_results.py prod_results_raw_samples.csv
```

---

##  Step 6: Interpreting Results

### What to Look For

#### Excellent Performance
- **p50 < 750 ns** - Better than Jane Street
- **p99 < 850 ns** - Very consistent
- **Jitter < 300 ns** - Deterministic execution

#### Good Performance
- **p50 < 1000 ns** - Competitive with top firms
- **p99 < 1200 ns** - Good consistency
- **Jitter < 500 ns** - Acceptable

#### Needs Optimization
- **p50 > 1500 ns** - Check system configuration
- **p99 > 2000 ns** - High variance, investigate
- **Jitter > 1000 ns** - System contention issues

### Common Issues and Fixes

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| High p99 (>2μs) | CPU not isolated | Check `isolcpus` configuration |
| High jitter (>1μs) | Interrupts on core | Disable IRQ balancing |
| Slow mean (>1.5μs) | CPU frequency scaling | Set governor to 'performance' |
| Inconsistent runs | Memory swapping | Configure huge pages + swappiness |
| Very slow (>5μs) | Debug build | Rebuild with Release mode |

---

## Advanced Configuration

### Hardware Timestamping (Mellanox/Solarflare)

For NICs with hardware timestamping support:

```bash
# Mellanox ConnectX
ethtool -T eth0  # Check if hardware timestamping supported

# If supported, enable in code:
# Modify benchmark_main.cpp to use hw_rx_timestamp / hw_tx_timestamp
```

### Network Loopback Testing

```bash
# Connect TX and RX ports with optical loopback
# This measures true round-trip latency including NIC

sudo ./build/hft_benchmark --loopback --samples 10000000
```

### Custom Market Data Injection

For testing with real exchange feed formats:

```bash
# Capture real market data
tcpdump -i eth0 -w market_data.pcap 'udp port 12345'

# Replay in benchmark (requires pcap support)
sudo ./build/hft_benchmark --replay market_data.pcap
```

---

##  Continuous Monitoring

### Automated Daily Benchmarks

Create a cron job:
```bash
# Edit crontab
crontab -e

# Add daily benchmark at 6 AM
0 6 * * * cd ~/trading-system && sudo ./build/hft_benchmark \
          --samples 10000000 --output daily_$(date +\%Y\%m\%d) \
          >> benchmark_log.txt 2>&1
```

### Track Performance Over Time

```bash
# Extract p99 from all daily results
grep "p99" daily_*.csv | sort
```

---

## 🐛 Troubleshooting

### Permission Errors

```bash
# If "Failed to set real-time priority"
sudo setcap cap_sys_nice,cap_ipc_lock=+ep ./build/hft_benchmark

# Or run with sudo
sudo ./build/hft_benchmark ...
```

### Build Errors

```bash
# Missing headers
sudo apt-get install linux-headers-$(uname -r)

# Compiler too old
# Install g++ 9+
sudo apt-get install g++-9
export CXX=g++-9
./build_benchmark.sh
```

### Abnormally Slow Results

```bash
# Check CPU frequency
cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq
# Should be close to CPU max frequency

# Check if core is truly isolated
ps aux | grep -E 'CPU|2' | grep -v grep
# Should show very few processes on core 2

# Check system load
uptime
# Load should be < number of cores
```

---

##  Further Optimization

### Kernel Tuning

Additional kernel parameters in `/etc/sysctl.conf`:
```
# Network buffer sizes
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 87380 134217728

# Reduce latency
net.ipv4.tcp_low_latency = 1
net.ipv4.tcp_sack = 1
```

### BIOS Settings

In your server BIOS:
- Disable: C-States, P-States, Turbo Boost (for consistency)
- Enable: Constant TSC, Hardware Prefetcher
- Set: Power management to "Maximum Performance"

---

##  Success Criteria

Your system is ready for production if:

-  p50 < 850 ns
-  p99 < 1000 ns
-  Jitter < 400 ns
-  All component benchmarks meet targets
-  Consistent results across multiple runs

---

## 📞 Support

For issues or questions:
1. Check `check_system_config.sh` output
2. Review troubleshooting section
3. Check kernel logs: `dmesg | tail -100`
4. Verify system load: `htop` or `top`

---

## Next Steps

After successful benchmarking:
1. Integrate with real exchange feeds
2. Add hardware NIC support (DPDK/ef_vi)
3. Deploy to production with monitoring
4. Run continuous performance regression tests

**Target: Sub-microsecond tick-to-trade in production! **
