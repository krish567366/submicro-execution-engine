---
name: Performance Issue
description: Report performance degradation or optimization opportunities
title: "[PERF] "
labels: ["performance", "triage"]
assignees: []
---

## Performance Issue Description

Describe the performance issue you're experiencing.

## Benchmark Results

Please include benchmark results showing the performance degradation:

### Before (Expected Performance)
```
Component                 Latency    Throughput
─────────────────────────────────────────────
[Component Name]         XXX ns     XXX ops/sec
...
TOTAL                   XXX ns      XXX ops/sec
```

### After (Actual Performance)
```
Component                 Latency    Throughput
─────────────────────────────────────────────
[Component Name]         XXX ns     XXX ops/sec
...
TOTAL                   XXX ns      XXX ops/sec
```

## Environment

- **OS**: [e.g., Ubuntu 20.04, CentOS 8]
- **CPU**: [e.g., Intel Xeon Platinum 8280 @ 2.7GHz]
- **Memory**: [e.g., 256GB DDR4-2933]
- **NIC**: [e.g., Intel X710, Mellanox ConnectX-5]
- **Kernel**: [e.g., Linux 5.4.0 with RT patches]
- **Compiler**: [e.g., GCC 9.4.0 with -O3 -march=native]
- **Commit Hash**: [e.g., a1b2c3d]

## Measurement Methodology

How did you measure the performance? Include:

- **Benchmark command**: Exact command used
- **Measurement tool**: TSC, perf, custom profiler, etc.
- **Sample size**: Number of measurements
- **Statistical analysis**: Mean, median, p99, standard deviation

## Steps to Reproduce

1. Build with specific flags: `...`
2. Run benchmark: `...`
3. Observe performance degradation

## Root Cause Analysis

If you've investigated, what do you suspect is causing the issue?

- [ ] Compiler optimization regression
- [ ] Memory allocation changes
- [ ] Lock contention
- [ ] Cache misses
- [ ] Branch misprediction
- [ ] Other (please specify)

## Proposed Solution

If you have suggestions for fixing the performance issue:

## Additional Context

- **Regression**: When did this performance degradation start?
- **Impact**: How critical is this for your use case?
- **Workarounds**: Any temporary workarounds you've found?

## Checklist

- [ ] Performance measurements are reproducible
- [ ] Baseline performance is documented
- [ ] Environment details are complete
- [ ] Measurement methodology is sound