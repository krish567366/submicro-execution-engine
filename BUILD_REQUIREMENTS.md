# Build Requirements for Production HFT System

## Platform Requirements

⚠️ **CRITICAL**: This system is designed for **Intel/AMD x86-64 architecture only**.

### Recommended Hardware

**CPU Requirements:**
- **Intel**: Xeon Cascade Lake, Ice Lake, or Sapphire Rapids
- **AMD**: EPYC Zen 2, Zen 3, or Zen 4
- Minimum 8 cores (16 with hyperthreading disabled)
- **Required CPU Features:**
  - AVX2 (minimum)
  - AVX-512 (recommended for superposition logic)
  - TSX (Transactional Synchronization Extensions)
  - RDTSC/RDPMC (nanosecond timing)

**📊 Performance Impact**: See [LATENCY_ANALYSIS.md](docs/LATENCY_ANALYSIS.md) for detailed breakdown
- **Total latency savings**: 8,775 - 18,228 ns per trade (13-33x faster)
- **Sub-microsecond execution**: 580-720 ns end-to-end (vs 9.5-19 μs standard)

**Memory:**
- 32GB+ DDR4-3200 or faster
- ECC recommended for production

**Network:**
- 10GbE or faster NIC with kernel bypass support (DPDK/Solarflare ef_vi)
- Intel X710/XXV710 or Mellanox ConnectX-5/6

### Operating System

**Linux (Required for Production):**
```bash
Ubuntu 22.04 LTS Server (PREEMPT_RT kernel)
OR
RHEL 8.x / Rocky Linux 8.x with real-time kernel
```

**Kernel Configuration:**
```bash
# Required kernel parameters
isolcpus=2-15          # Isolate CPUs for trading threads
nohz_full=2-15         # Tickless kernel on isolated CPUs
rcu_nocbs=2-15         # RCU callbacks off isolated CPUs
intel_pstate=disable   # Disable frequency scaling
processor.max_cstate=1 # Prevent deep sleep
intel_idle.max_cstate=0
```

## Build Instructions

### On Linux x86-64 (Production)

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    g++-11 \
    libboost-all-dev \
    libnlohmann-json3-dev \
    libssl-dev

# 2. Configure build
cd /path/to/new-trading-system
mkdir -p build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++-11 \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

# 3. Build
make -j$(nproc) hft_system

# 4. Verify build
./hft_system --version
./bin/test_multi_kernel  # Run unit tests
```

### Build Flags Explained

The CMakeLists.txt automatically enables:
- `-O3`: Maximum optimization
- `-march=native`: CPU-specific optimizations (AVX-512, etc.)
- `-mtune=native`: Tune for current CPU microarchitecture
- `-flto`: Link-time optimization
- `-ffast-math`: Aggressive floating-point optimizations
- `-funroll-loops`: Loop unrolling for hot paths

### Performance Tuning

**CPU Pinning:**
```bash
# Pin trading thread to isolated CPU core
taskset -c 2 ./hft_system
```

**Memory Locking:**
```bash
# Increase locked memory limit
ulimit -l unlimited
```

**Network Tuning:**
```bash
# Disable interrupt coalescing
ethtool -C eth0 rx-usecs 0 tx-usecs 0

# Increase ring buffer
ethtool -G eth0 rx 4096 tx 4096
```

## Current Platform (ARM64 / Apple Silicon)

You are currently on **ARM64 architecture** (Apple Silicon M1/M2/M3).

### Why ARM64 is Not Supported

The codebase uses x86-specific features:

1. **AVX-512 SIMD**: Parallel strategy evaluation (64 strategies in 1 cycle)
2. **Intel TSX**: Hardware transactional memory for lock-free operations
3. **RDTSC/RDPMC**: Sub-nanosecond timing (ARM has CNTVCT but different semantics)
4. **Direct PCIe MMIO**: Custom NIC driver for 20-50ns packet RX

### Development Testing Only

If you need to test **non-performance-critical** parts on ARM64:

```bash
cmake .. -DALLOW_NON_OPTIMAL_BUILD=ON
make hft_system
```

⚠️ **WARNING**: This build will be:
- **10-100x slower** than x86-64
- Missing critical features (SIMD, TSX, hardware timing)
- **NOT suitable for production trading**

## Verification

After building on x86-64, verify all features:

```bash
# 1. Check CPU features
lscpu | grep -E "avx2|avx512|tsx"

# 2. Run benchmarks
./benchmarks/backtest_demo

# 3. Check latency
./bin/test_multi_kernel --latency-test

# Expected Results:
# - Decision latency: < 1000ns (1 microsecond)
# - NIC RX to decision: < 500ns
# - Order submission: < 2000ns
```

## Production Deployment Checklist

- [ ] x86-64 CPU with AVX-512
- [ ] PREEMPT_RT kernel installed
- [ ] CPU isolation configured (isolcpus, nohz_full)
- [ ] Memory locked (ulimit -l unlimited)
- [ ] NIC bypass driver configured (DPDK/ef_vi)
- [ ] Hyperthreading disabled in BIOS
- [ ] Turbo Boost disabled (consistent timing)
- [ ] All unnecessary services stopped
- [ ] Trading thread pinned to isolated core
- [ ] Benchmarks showing < 1μs latency

## Troubleshooting

**Error: "PRODUCTION HFT BUILD REQUIRES x86-64 ARCHITECTURE"**
- You're on ARM64. Build on Linux x86-64 server.

**Error: "immintrin.h not found"**
- Your compiler doesn't support x86 intrinsics. Use g++-11 or newer.

**Latency > 5μs:**
- Check CPU frequency scaling is disabled
- Verify thread is pinned to isolated core
- Run with SCHED_FIFO priority

## Contact

For production deployment assistance:
- Review PRODUCTION_READINESS.md
- Check docs/LATENCY_BUDGET.md
- Run institutional verification: docs/INSTITUTIONAL_LOGGING_COMPARISON.md
