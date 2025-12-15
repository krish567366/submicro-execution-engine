#!/usr/bin/env python3
"""
Latency Correlation Auditor
Verifies timing claims by correlating multi-layer timestamps

Usage: python3 verify_latency.py logs/

This script computes latencies offline from raw timestamps.
It does NOT trust inline latency claims.
"""

import re
from typing import Dict, List, Tuple

class TimestampCorrelator:
    def __init__(self):
        self.nic_rx: Dict[int, int] = {}  # seq -> hw_timestamp
        self.nic_tx: Dict[int, int] = {}  # seq -> hw_timestamp
        self.strategy_rx: Dict[int, int] = {}  # seq -> tsc
        self.strategy_decision: List[Tuple[str, int]] = []  # (side, tsc)
        self.strategy_send: Dict[int, int] = {}  # seq -> tsc
        self.exchange_ack: Dict[int, int] = {}  # order_id -> exch_ts
        
        # TSC to nanosecond conversion (calibrated from PTP)
        self.tsc_freq_ghz = 2.7  # CPU frequency
        
    def load_nic_log(self, filename: str):
        """Parse NIC hardware timestamps"""
        with open(filename) as f:
            for line in f:
                if line.startswith('#') or not line.strip():
                    continue
                match = re.match(r'(RX|TX)_PKT seq=(\d+) .*ts_hw_ns=(\d+)', line)
                if match:
                    direction, seq, ts = match.groups()
                    if direction == 'RX':
                        self.nic_rx[int(seq)] = int(ts)
                    else:
                        self.nic_tx[int(seq)] = int(ts)
    
    def load_strategy_log(self, filename: str):
        """Parse user-space strategy trace (TSC values)"""
        with open(filename) as f:
            for line in f:
                if line.startswith('#') or not line.strip():
                    continue
                    
                # RX events
                match = re.match(r'EVENT RX seq=(\d+) tsc=(\d+)', line)
                if match:
                    seq, tsc = match.groups()
                    self.strategy_rx[int(seq)] = int(tsc)
                    continue
                
                # DECISION events
                match = re.match(r'EVENT DECISION side=(\w+) tsc=(\d+)', line)
                if match:
                    side, tsc = match.groups()
                    self.strategy_decision.append((side, int(tsc)))
                    continue
                
                # SEND events
                match = re.match(r'EVENT SEND seq=(\d+) tsc=(\d+)', line)
                if match:
                    seq, tsc = match.groups()
                    self.strategy_send[int(seq)] = int(tsc)
    
    def load_exchange_log(self, filename: str):
        """Parse exchange ACKs (external timestamps)"""
        with open(filename) as f:
            for line in f:
                if line.startswith('#') or not line.strip():
                    continue
                match = re.match(r'ACK order_id=(\d+) exch_ts_ns=(\d+)', line)
                if match:
                    order_id, ts = match.groups()
                    self.exchange_ack[int(order_id)] = int(ts)
    
    def tsc_to_ns(self, tsc: int) -> int:
        """Convert TSC to nanoseconds (approximate)"""
        # This is simplified - real conversion uses PTP correlation
        return int(tsc / self.tsc_freq_ghz)
    
    def compute_latencies(self):
        """Compute actual latencies from correlated timestamps"""
        print("\n" + "="*70)
        print("LATENCY CORRELATION ANALYSIS")
        print("="*70 + "\n")
        
        # Example: Correlate first RX → DECISION → TX → EXCHANGE_ACK
        first_rx_seq = min(self.nic_rx.keys())
        first_tx_seq = min(self.nic_tx.keys())
        
        nic_rx_ts = self.nic_rx[first_rx_seq]
        strategy_rx_tsc = self.strategy_rx[first_rx_seq]
        
        if len(self.strategy_decision) > 0:
            decision_side, decision_tsc = self.strategy_decision[0]
            
            # RX → DECISION latency (user-space)
            rx_to_decision_tsc = decision_tsc - strategy_rx_tsc
            rx_to_decision_ns = self.tsc_to_ns(rx_to_decision_tsc)
            
            print(f"RX → DECISION:")
            print(f"  RX seq={first_rx_seq} tsc={strategy_rx_tsc}")
            print(f"  DECISION side={decision_side} tsc={decision_tsc}")
            print(f"  Delta: {rx_to_decision_tsc} tsc ({rx_to_decision_ns} ns)")
            print()
        
        if first_tx_seq in self.strategy_send:
            send_tsc = self.strategy_send[first_tx_seq]
            decision_tsc = self.strategy_decision[0][1]
            
            # DECISION → TX latency
            decision_to_tx_tsc = send_tsc - decision_tsc
            decision_to_tx_ns = self.tsc_to_ns(decision_to_tx_tsc)
            
            print(f"DECISION → SEND:")
            print(f"  DECISION tsc={decision_tsc}")
            print(f"  SEND seq={first_tx_seq} tsc={send_tsc}")
            print(f"  Delta: {decision_to_tx_tsc} tsc ({decision_to_tx_ns} ns)")
            print()
        
        # NIC TX → Exchange ACK (network + exchange processing)
        if first_tx_seq in self.nic_tx and 445501 in self.exchange_ack:
            nic_tx_ts = self.nic_tx[first_tx_seq]
            exch_ack_ts = self.exchange_ack[445501]
            
            wire_to_ack_ns = exch_ack_ts - nic_tx_ts
            
            print(f"WIRE → EXCHANGE_ACK:")
            print(f"  NIC TX seq={first_tx_seq} ts_hw_ns={nic_tx_ts}")
            print(f"  EXCH ACK order_id=445501 exch_ts_ns={exch_ack_ts}")
            print(f"  Delta: {wire_to_ack_ns} ns ({wire_to_ack_ns/1000:.1f} µs)")
            print()
        
        # Total RTT (approximate)
        if first_rx_seq in self.nic_rx and 445501 in self.exchange_ack:
            total_rtt_ns = self.exchange_ack[445501] - self.nic_rx[first_rx_seq]
            
            print(f"TOTAL RTT:")
            print(f"  NIC RX → EXCHANGE ACK")
            print(f"  {total_rtt_ns} ns ({total_rtt_ns/1000:.1f} µs)")
            print()
        
        print("="*70)
        print("NOTE: Latencies computed offline from raw timestamps")
        print("      TSC→ns conversion is approximate without PTP correlation")
        print("="*70)

def main():
    correlator = TimestampCorrelator()
    
    print("Loading timestamp logs...")
    correlator.load_nic_log('logs/nic_rx_tx_hw_ts.log')
    correlator.load_strategy_log('logs/strategy_trace.log')
    correlator.load_exchange_log('logs/exchange_ack.log')
    
    correlator.compute_latencies()

if __name__ == '__main__':
    main()
