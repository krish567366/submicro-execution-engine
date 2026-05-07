# README: Production Log Bundle

## Structure

This directory contains institutional-grade audit logs with NO performance claims.

```
logs/
├── nic_rx_tx_hw_ts.log      # Hardware timestamps (NIC)
├── strategy_trace.log        # User-space events (TSC)
├── exchange_ack.log          # External exchange timestamps
├── ptp_sync.log              # Clock synchronization
├── order_gateway.log         # Order submission trace
├── MANIFEST.sha256           # Cryptographic integrity
└── raw_capture_20251215.pcap # Raw packets (not included - binary)
```

## Verification

```bash
# 1. Verify file integrity
sha256sum -c MANIFEST.sha256

# 2. Compute latencies offline
python3 ../scripts/verify_latency.py

# 3. Inspect raw PCAP (requires tcpdump)
tcpdump -r raw_capture_20251215.pcap -tt -n
```

## Principles

Logs record events, not interpretations
Timestamps are raw (HW ns, TSC ticks)
No inline latency computations
External timestamps cannot be faked
Correlation happens offline
PCAP proves wire truth
Manifest proves integrity

✗ No performance claims
✗ No competitor comparisons
✗ No tier rankings
✗ No marketing language

## Latency Computation

Latencies are computed by `scripts/verify_latency.py` which correlates:

- NIC hardware timestamps (when packet hit wire)
- Strategy TSC values (CPU cycle counter)
- Exchange timestamps (external reality)
- PTP sync offsets (clock alignment)

This script computes:
- RX → DECISION (user-space processing)
- DECISION → TX (order preparation + NIC)
- WIRE → EXCHANGE_ACK (network + exchange)
- TOTAL RTT (end-to-end)

## Compliance

These logs are designed to survive:

- Legal discovery
- Regulatory audit (SEBI, SEC, FCA)
- Third-party verification
- Forensic timestamp analysis
- Cross-exchange correlation

## Notes

- All timestamps in nanoseconds (UTC)
- TSC values require calibration against PTP
- PCAP file omitted from git (too large, binary)
- Exchange timestamps are ground truth
- Clock drift tracked in ptp_sync.log
