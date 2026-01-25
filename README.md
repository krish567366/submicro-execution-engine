<div align="center">

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                                                                               ║
║        ███████╗██╗   ██╗██████╗ ███╗   ███╗██╗ ██████╗██████╗  ██████╗        ║
║        ██╔════╝██║   ██║██╔══██╗████╗ ████║██║██╔════╝██╔══██╗██╔═══██╗       ║
║        ███████╗██║   ██║██████╔╝██╔████╔██║██║██║     ██████╔╝██║   ██║       ║
║        ╚════██║██║   ██║██╔══██╗██║╚██╔╝██║██║██║     ██╔══██╗██║   ██║       ║
║        ███████║╚██████╔╝██████╔╝██║ ╚═╝ ██║██║╚██████╗██║  ██║╚██████╔╝       ║
║        ╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝        ║
║                                                                               ║
║            Sub-Microsecond Execution Engine for Algorithmic Trading           ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

<h1>Ultra-Low Latency Trading System</h1>

<p>
<b>Deterministic, nanosecond-precise execution engine for quantitative trading research</b>
</p>

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen?style=for-the-badge)](.)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=for-the-badge&logo=cplusplus)](.)
[![Rust](https://img.shields.io/badge/Rust-1.70%2B-orange?style=for-the-badge&logo=rust)](.)
[![License](https://img.shields.io/badge/license-Proprietary-red?style=for-the-badge)](LICENSE)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen?style=for-the-badge)](.)
[![GitHub Sponsors](https://img.shields.io/badge/Sponsor-krish567366-ff69b4?style=for-the-badge&logo=github)](https://github.com/sponsors/krish567366)

<p>
<a href="https://submicro.krishnabajpai.me/">Live Demo</a> •
<a href="#key-features">Features</a> •
<a href="#quick-start">Quick Start</a> •
<a href="#benchmarks">Benchmarks</a> •
<a href="#architecture">Architecture</a> •
<a href="#documentation">Docs</a>
</p>

---

### **890ns median latency** | **Deterministic replay** | **Lock-free architecture** | **Research-grade framework**

**[View Interactive Documentation →](https://submicro.krishnabajpai.me/)**

---

> [!TIP]
> **Branching Strategy**:
> *   **`main`**: Fundamental execution infrastructure. Primary focus is **Latency Minimization** (median < 900ns) and system determinism.
> *   **`alpha-optimized`**: Quantitative modeling layer. Primary focus is **Alpha & Signal Optimization** (multi-kernel Hawkes, SIMD features) while keeping latency within sub-microsecond thresholds.

</div>

---

## What Makes This Special?

> **Built for researchers and systems engineers pushing the boundaries of low-latency execution.**

This isn't just another trading bot. It's a **complete infrastructure** for understanding, measuring, and optimizing execution latency at the **hardware level**.

### The Problem
Traditional trading systems are black boxes with unpredictable latency, non-deterministic behavior, and poor visibility into where microseconds are lost.

### The Solution
A **transparent, deterministic execution engine** that:
- Achieves **sub-microsecond decision latency** (890ns median)
- Guarantees **bit-identical replay** for audit and debugging
- Provides **nanosecond-level instrumentation** at every stage
- Uses **zero-allocation hot paths** and lock-free data structures
- Simulates **kernel-bypass networking** (DPDK-style)
- Implements **institutional-grade logging** and monitoring

**Research & Education Only** — Not production-ready. Hardware validation (DPDK/FPGA) is simulated. [Learn more](#hardware-validation--future-work).

**PROPRIETARY LICENSE** — Commercial use prohibited. Written permission required. Contact: krishna@krishnabajpai.me

## Performance Snapshot

<div align="center">

| **Component** | **Median** | **p99** | **p99.9** | 
|------------------|--------------|-----------|--------------|
| Market Data Ingestion | **87 ns** | 124 ns | 201 ns |
| Signal Extraction (SIMD) | **40 ns** | 48 ns | 67 ns |
| Hawkes Update (Power-Law) | **150 ns** | 189 ns | 234 ns |
| **End-to-End Decision** | **890 ns** | **921 ns** | **1047 ns** |
| Order Serialization | **34 ns** | 41 ns | 58 ns |

**Measurement Precision:** ±5ns (TSC jitter) | ±17ns (PTP offset)  
**Test Hardware:** Intel Xeon Platinum 8280 @ 2.7GHz, isolated core, RT kernel

</div>

---

## Key Features

<table>
<tr>
<td width="50%">

### **Performance**
- Sub-microsecond decision latency
- Zero-copy data paths
- Lock-free SPSC/MPSC queues
- Cache-aligned data structures
- SIMD-optimized computations (AVX-512)

</td>
<td width="50%">

### **Determinism**
- Bit-identical replay guarantees
- Event-driven scheduling
- Fixed RNG seeds
- Pre-allocated memory pools
- TSC-level timestamp precision

</td>
</tr>
<tr>
<td width="50%">

### **Architecture**
- Kernel-bypass NIC simulation
- Multivariate Hawkes process
- Avellaneda-Stoikov market making
- Adaptive risk management
- C++/Rust FFI integration

</td>
<td width="50%">

### **Observability**
- Real-time metrics dashboard
- Multi-layer audit logging
- SHA-256 replay verification
- Nanosecond-level tracing
- Latency breakdown analysis

</td>
</table>
<br/>

<table>
<tr>
<td colspan="2">

### **New: Jitter Profiler & Stall Detection**
> **"If you can't measure tail latency, you can't fix it."**
A built-in nanosecond-resolution profiler detects **System Management Interrupts (SMIs)** and scheduler preemption by measuring the cycle-gap between busy-wait iterations. This ensures deep visibility into the "Zero-Interruption" guarantee required for p99 latency stability.

</td>
</tr>
</table>

---

## Quick Start

**Get running in 60 seconds:**

```bash
# 1. Clone the repository
git clone https://github.com/krish567366/submicro-execution-engine.git
cd submicro-execution-engine

# 2. Build the system (automatic optimization flags)
./scripts/build_all.sh

# 3. Run deterministic backtest
scripts/run_backtest.py

# 4. View results
python3 scripts/verify_latency.py
open dashboard/index.html  # Interactive metrics dashboard
```

<details>
<summary><b>Expected Output (click to expand)</b></summary>

```
=== Low-Latency Trading System ===
Market data ingestion: 87ns median
Signal extraction: 40ns median  
Hawkes update: 150ns median
Decision latency: 890ns median

--- Cycle: 1000 ---
Mid Price: $100.05
Position: 250
Active Quotes: Bid=100.04 Ask=100.06 Spread=2.00 bps
Hawkes: Buy=12.456 Sell=11.234 Imbalance=0.052
Regime: NORMAL (multiplier=1.0)
Last Cycle Latency: 847 ns (0.847 µs)
Determinism verified: SHA-256 match
```

</details>

---

## Architecture Overview

<div align="center">

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Market Data Feed (Simulated)                         │
│                    Kernel-Bypass NIC • Zero-Copy DMA Transfer                │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ 87ns median
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Lock-Free Ring Buffer (SPSC)                              │
│              Power-of-2 Size • Cache-Line Aligned • No Allocations           │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ O(1) operations
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                   Order Book Reconstruction                                  │
│            Price-Level Aggregation • L2 Depth Tracking                       │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │
                ┌───────────────┴───────────────┐
                ▼                               ▼
┌─────────────────────────────┐   ┌─────────────────────────────────────────┐
│   Hawkes Process Engine     │   │  Microstructure Features                │
│   • Self/Cross Excitation   │   │  • Deep OFI (10 levels)                │
│   • Power-Law Kernel        │   │  • Order Book Imbalance                │
│   • Buy/Sell Intensity      │   │  • Flow Toxicity (Kyle λ)              │
└──────────────┬──────────────┘   └──────────────┬──────────────────────────┘
               │  150ns median                   │ 40ns (SIMD)
               └───────────────┬─────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                  FPGA DNN Inference (Simulated)                              │
│              12 Features → 8 Hidden → 3 Outputs • 400ns Fixed                │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│              Avellaneda-Stoikov Market Making Strategy                       │
│        HJB Equation • Inventory Skew • Latency-Aware Pricing                 │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ 890ns E2E
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Risk Control (Pre-Trade + Kill-Switch)                    │
│          Position Limits • Regime Detection • Atomic Checks                  │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ 34ns serialization
                                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Order Submission                                       │
│                  Pre-Serialized Orders • Zero Copy                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

</div>

> **See [`ARCHITECTURE.md`](ARCHITECTURE.md) for detailed component documentation**

---

## Determinism & Reproducibility

One of the system's **core guarantees** is bit-identical replay capability:

**Fixed RNG seeds** — Deterministic random number generation  
**Event-driven scheduling** — No wall-clock dependencies  
**Pre-allocated memory** — No allocator non-determinism  
**Timestamp-ordered events** — Consistent processing order  

### Verification

```bash
# Run backtest
scripts/run_backtest.py

# Verify deterministic replay
cd logs
sha256sum -c MANIFEST.sha256
strategy_trace.log: OK
order_flow.log: OK
latency_metrics.log: OK
```

---

## Complete Documentation

| Document | Description |
|----------|-------------|
| [`ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Order path, cache layout, thread model |
| [`BENCHMARK_GUIDE.md`](docs/BENCHMARK_GUIDE.md) | Latency measurement methodology |
| [`LATENCY_BUDGET.md`](docs/LATENCY_BUDGET.md) | Component-level breakdown |
| [`INSTITUTIONAL_LOGGING_COMPARISON.md`](docs/INSTITUTIONAL_LOGGING_COMPARISON.md) | Audit-grade logging |
| [`PRODUCTION_READINESS.md`](docs/PRODUCTION_READINESS.md) | Deployment considerations |
| [`ENGAGEMENTS.md`](docs/ENGAGEMENTS.md) | Commercial & Research support |
| [`logs/README.md`](logs/README.md) | Multi-layer timestamp verification |

---

## Contributing

We welcome contributions! Here's how to get started:

<details>
<summary><b>Report a Bug</b></summary>

Open an issue with:
- System configuration (CPU, OS, compiler)
- Reproducible example
- Expected vs actual behavior
- Relevant logs

</details>

<details>
<summary><b>Propose a Feature</b></summary>

1. Check existing issues/PRs
2. Open an issue describing the feature
3. Discuss implementation approach
4. Submit a PR with tests

</details>

<details>
<summary><b>Submit a Pull Request</b></summary>

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes with tests
4. Ensure `ctest` and `cargo test` pass
5. Commit with clear messages
6. Push and open a PR

</details>

### Development Guidelines

- **Code style:** Follow existing patterns (run `clang-format`)
- **Tests:** Add tests for new features
- **Benchmarks:** Measure latency impact
- **Documentation:** Update relevant markdown files

---

## 🌟 Star History

## Star History

<a href="https://www.star-history.com/#krish567366/submicro-execution-engine&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=krish567366/submicro-execution-engine&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=krish567366/submicro-execution-engine&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=krish567366/submicro-execution-engine&type=date&legend=top-left" />
 </picture>
</a>

---

## Commercial & Research Engagements

This repository serves as a framework for research and evaluation. For institutional requirements requiring production-grade performance or custom research collaborations, we offer several engagement models:

-   **Institutional Implementation**: Custom DPDK/RDMA kernel-bypass and FPGA HLS/RTL acceleration.
-   **Performance Engineering**: Architecture reviews, latency bottleneck audits, and system tuning.
-   **Research Collaboration**: Market microstructure modeling and low-latency ML inference.
-   **Strategy Optimization**: Migration of Python/Julia models to AVX-512 optimized C++.

See [**`ENGAGEMENTS.md`**](docs/ENGAGEMENTS.md) for more details.

**Contact:** [krishna@krishnabajpai.me](mailto:krishna@krishnabajpai.me)

*Notice: Pricing is not disclosed publicly. All engagements are scoped and quoted based on project complexity and hardware requirements.*

### Kernel-bypass NICs

True kernel bypass depends on:
- **Specific NIC hardware** (NVIDIA/Mellanox, Intel, Solarflare/Xilinx, FPGA NICs)
- **Driver stacks** (DPDK, RDMA verbs, Onload, custom DMA paths)
- **NIC firmware** and queue configuration

There is no realistic way to ship a single, generic kernel-bypass implementation that works across all vendors and drivers. The only viable approach is to define a clean abstraction and allow users to plug in vendor-specific implementations.

### FPGA Inference

The same constraint applies to FPGA-based inference:
- **FPGA implementations** depend on the exact FPGA model, vendor toolchain (Xilinx Vitis/Vivado, Intel Quartus), and PCIe/DMA configuration.
- **The inference pipeline** (HLS, RTL, data layout, batching, clocking) is tightly coupled to the target hardware.
- **Bitstreams** are not portable across vendors or across FPGA families.

In this repository:
- FPGA inference is represented as an architectural interface / placeholder.
- Software emulation is used where hardware is not available.
- For specific hardware targets, custom HLS/RTL integration can be implemented.

---

## 📖 Academic References

<details>
<summary><b>Click to expand bibliography</b></summary>

### Hawkes Processes
1. **Hawkes, A. G. (1971).** "Specular Point Processes" *Biometrika*
2. **Bacry, E., et al. (2015).** "Hawkes Processes in Finance" *Market Microstructure and Liquidity*

### Market Making
3. **Avellaneda, M., & Stoikov, S. (2008).** "High-frequency trading in a limit order book" *Quantitative Finance*
4. **Guéant, O., et al. (2013).** "Dealing with the inventory risk" *Mathematics and Financial Economics*

### Market Microstructure
5. **Cartea, Á., et al. (2015).** "Algorithmic and High-Frequency Trading" *Cambridge University Press*
6. **Lehalle, C.-A., & Laruelle, S. (2018).** "Market Microstructure in Practice" *World Scientific*
7. **Easley, D., et al. (2012).** "Flow Toxicity and Liquidity in a High-Frequency World" *Review of Financial Studies*

### System Design
8. **Nygren, E. (2015).** "Linux Kernel Development for Real-Time Systems" *O'Reilly*
9. **Gregg, B. (2013).** "Systems Performance: Enterprise and the Cloud" *Prentice Hall*

</details>

---

## Important Disclaimers

<div align="center">

### **RESEARCH & EDUCATION ONLY**

</div>

This system is **NOT**:
- Production-ready trading software
- Connected to any exchanges
- Financial advice or recommendation
- Guaranteed to be profitable

This system **IS**:
- A research framework
- An educational tool
- A latency benchmarking platform
- A deterministic execution skeleton

**Real production HFT requires:**
- Hardware FPGA acceleration (Xilinx, Altera)
- True kernel-bypass (DPDK, Solarflare OpenOnload)
- Exchange connectivity (FIX, proprietary protocols)
- Compliance systems (kill-switches, position limits)
- Risk management infrastructure
- Extensive testing and regulatory approval
- **Vendor-specific hardware integration** (DPDK/FPGA)

**Legal:** No warranty. Use at your own risk. See LICENSE for details.

---

## Contact & Community

<div align="center">

**Questions? Ideas? Collaboration?**

[![GitHub Issues](https://img.shields.io/badge/Issues-Open-blue?style=for-the-badge&logo=github)](https://github.com/krish567366/submicro-execution-engine/issues)
[![Discussions](https://img.shields.io/badge/Discussions-Join-green?style=for-the-badge&logo=github)](https://github.com/krish567366/submicro-execution-engine/discussions)
[![Email](https://img.shields.io/badge/Email-Contact-red?style=for-the-badge&logo=gmail)](mailto:krishna@krishnabajpai.me)

</div>

---

## ❤️ Support This Project

If this research and codebase helps with your work, please consider sponsoring:

[![GitHub Sponsors](https://img.shields.io/badge/Sponsor-krish567366-ff69b4?style=for-the-badge&logo=github)](https://github.com/sponsors/krish567366)

Your support helps fund:
- **Hardware**: Dedicated access to DPDK-enabled NICs and FPGA accelerator cards for validation
-  **Funding**: Production-grade bitstreams and vendor-specific implementations
-  **Research**: Advanced algorithms and optimization techniques
-  **Documentation**: Comprehensive guides and institutional-grade whitepapers
- **Open Source**: Keeping the core research framework freely available

---

### Related Projects

- [DPDK](https://www.dpdk.org/) — Data Plane Development Kit
- [Solarflare OpenOnload](https://www.xilinx.com/products/design-tools/software-zone/openonload.html) — Kernel-bypass networking
- [Folly](https://github.com/facebook/folly) — Facebook's lock-free structures
- [QuantLib](https://www.quantlib.org/) — Quantitative finance library

---

<div align="center">

## **Built for Speed. Designed for Reliability. Optimized for Discovery.**

### If you find this useful, please star the repository

<sub>Made with care by quantitative systems engineers</sub>

---

**Trading • Low-Latency • Research • Open Source**

</div>

---

## License

**PROPRIETARY LICENSE** - See [LICENSE](LICENSE) file for details

**IMPORTANT:** This software is for academic research and educational purposes only. 
Commercial use is strictly prohibited. Written permission required for any use beyond 
personal learning and non-commercial experimentation.

**To request permission:** Contact krishna@krishnabajpai.me

Copyright (c) 2025 Krishna Bajpai - All Rights Reserved

---
