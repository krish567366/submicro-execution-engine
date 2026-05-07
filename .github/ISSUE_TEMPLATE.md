---
name: Sample Issue Template
description: A reference template for creating new issue types in this project
title: "[TYPE] "
labels: ["triage"]
assignees: []
---

## Overview
Provide a high-level summary of the issue or proposal.

## Context
Why is this issue being opened? Provide background information and its relevance to the sub-microsecond execution engine.

## Objective
What is the desired outcome or goal of this issue?

## Technical Details
If applicable, include specific technical information:
- **Components involved**: [e.g., LOB, HawkesEngine, RiskControl]
- **Target Hardware**: [e.g., FPGA, DPDK-NIC]
- **Latency impact**: [e.g., +20ns, -50ns]

## Implementation Plan (if known)
1.  **Step 1**: ...
2.  **Step 2**: ...
3.  **Step 3**: ...

## Environment Setup (Professional Baseline)
- **OS**: [e.g., Linux RT Kernel]
- **CPU**: [e.g., Isolated Cores, C-States disabled]
- **Compiler Reference**: [e.g., GCC 12 -O3 -march=native]

## Impact Assessment
- [ ] Performance (Latency/Throughput)
- [ ] Determinism (Replay accuracy)
- [ ] Stability (Production readiness)
- [ ] Documentation / Observability

## Checklist for Submission
- [ ] Issue is scoped and non-redundant
- [ ] Technical impact is clearly articulated
- [ ] Proposed changes align with the project's sub-microsecond goals
