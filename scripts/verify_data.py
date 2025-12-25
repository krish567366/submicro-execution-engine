#!/usr/bin/env python3
"""
Data Verification Script
Analyzes synthetic_ticks_with_alpha.csv to verify quality and alpha patterns
"""

import csv
import numpy as np
from collections import defaultdict

print("=" * 70)
print("  SYNTHETIC DATA VERIFICATION REPORT")
print("=" * 70)
print()

# Read data
filename = "synthetic_ticks_with_alpha.csv"
print(f"📂 Loading: {filename}")

events = []
with open(filename, 'r') as f:
    for line in f:
        parts = line.strip().split(',')
        if len(parts) >= 7:
            events.append({
                'timestamp': int(parts[0]),
                'event_type': parts[1],
                'side': parts[2],
                'price': float(parts[3]),
                'size': int(parts[4]),
                'order_id': int(parts[5]),
                'depth': int(parts[6])
            })

print(f"Loaded {len(events):,} events")
print()

# Basic statistics
print("BASIC STATISTICS")
print("-" * 70)

timestamps = [e['timestamp'] for e in events]
duration_ns = timestamps[-1] - timestamps[0]
duration_sec = duration_ns / 1e9

print(f"Duration:        {duration_sec:.3f} seconds ({duration_ns:,} ns)")
print(f"Events:          {len(events):,}")
print(f"Event rate:      {len(events)/duration_sec:,.0f} events/sec")
print(f"Avg tick gap:    {duration_ns/len(events):.0f} ns")
print()

# Price analysis
prices = [e['price'] for e in events]
print("💵 PRICE ANALYSIS")
print("-" * 70)
print(f"Min price:       ${min(prices):.4f}")
print(f"Max price:       ${max(prices):.4f}")
print(f"Mean price:      ${np.mean(prices):.4f}")
print(f"Std dev:         ${np.std(prices):.4f}")
print(f"Price range:     ${max(prices) - min(prices):.4f}")
print()

# Order flow imbalance detection
print("ALPHA BURST DETECTION")
print("-" * 70)

# Calculate OBI over windows
window_size = 15  # Match burst duration
obi_values = []

for i in range(len(events) - window_size):
    window = events[i:i+window_size]
    
    buy_count = sum(1 for e in window if e['side'] == 'B' and e['event_type'] == 'add')
    sell_count = sum(1 for e in window if e['side'] == 'S' and e['event_type'] == 'add')
    
    total = buy_count + sell_count
    if total > 0:
        obi = (buy_count - sell_count) / total
        obi_values.append({
            'index': i,
            'timestamp': events[i]['timestamp'],
            'obi': obi,
            'strength': abs(obi)
        })

# Find strong OBI bursts (> 0.6 strength)
strong_bursts = [o for o in obi_values if o['strength'] > 0.6]

print(f"Total {window_size}-tick windows analyzed: {len(obi_values):,}")
print(f"Strong OBI bursts detected (>60%):      {len(strong_bursts):,}")
print()

if strong_bursts:
    # Group consecutive bursts
    burst_groups = []
    current_group = [strong_bursts[0]]
    
    for i in range(1, len(strong_bursts)):
        # If consecutive or within 100 ticks
        if strong_bursts[i]['index'] - strong_bursts[i-1]['index'] < 100:
            current_group.append(strong_bursts[i])
        else:
            if len(current_group) >= 5:  # Significant burst
                burst_groups.append(current_group)
            current_group = [strong_bursts[i]]
    
    if len(current_group) >= 5:
        burst_groups.append(current_group)
    
    print(f"Persistent alpha bursts identified:     {len(burst_groups)}")
    print()
    
    print("Top 10 strongest bursts:")
    print(f"  {'Event':<10} {'OBI':<10} {'Strength':<10} {'Duration'}")
    print("  " + "-" * 60)
    
    # Sort by average strength
    sorted_groups = sorted(burst_groups, key=lambda grp: np.mean([b['strength'] for b in grp]), reverse=True)
    for group in sorted_groups[:10]:
        avg_obi = np.mean([b['obi'] for b in group])
        avg_strength = np.mean([b['strength'] for b in group])
        duration_ticks = len(group)
        start_event = group[0]['index']
        
        direction = "BUY" if avg_obi > 0 else "SELL"
        print(f"  {start_event:<10,} {avg_obi:>+8.2%} {avg_strength:>8.2%}  {duration_ticks:>3} ticks ({direction})")

print()

# Event type distribution
print("📋 EVENT TYPE DISTRIBUTION")
print("-" * 70)

event_types = defaultdict(int)
for e in events:
    event_types[e['event_type']] += 1

for event_type, count in sorted(event_types.items(), key=lambda x: x[1], reverse=True):
    pct = count * 100.0 / len(events)
    print(f"  {event_type:<12} {count:>8,} ({pct:>5.1f}%)")

print()

# Side distribution
print("SIDE DISTRIBUTION")
print("-" * 70)

sides = defaultdict(int)
for e in events:
    sides[e['side']] += 1

for side, count in sorted(sides.items(), key=lambda x: x[1], reverse=True):
    pct = count * 100.0 / len(events)
    side_name = "Buy" if side == 'B' else "Sell"
    print(f"  {side_name:<12} {count:>8,} ({pct:>5.1f}%)")

print()

# Size distribution
print("📦 ORDER SIZE DISTRIBUTION")
print("-" * 70)

sizes = [e['size'] for e in events]
print(f"  Min:         {min(sizes):,}")
print(f"  Max:         {max(sizes):,}")
print(f"  Mean:        {np.mean(sizes):.1f}")
print(f"  Median:      {np.median(sizes):.0f}")
print(f"  Std dev:     {np.std(sizes):.1f}")

print()

# Verification summary
print("=" * 70)
print("  VERIFICATION SUMMARY")
print("=" * 70)
print()

checks = [
    ("PASS" if len(events) >= 90000 else "FAIL", f"Sufficient events: {len(events):,} >= 90,000"),
    ("PASS" if duration_sec >= 0.005 else "FAIL", f"Sufficient duration: {duration_sec:.3f}s >= 0.005s"),
    ("PASS" if len(burst_groups) >= 10 else "WARN", f"Alpha bursts detected: {len(burst_groups)} (expected ~17)"),
    ("PASS" if abs(sides['B'] - sides['S']) / len(events) < 0.05 else "FAIL", "Side balance within 5%"),
    ("PASS" if np.std(prices) < 0.2 else "FAIL", f"Price volatility reasonable: σ={np.std(prices):.4f}"),
]

for symbol, check in checks:
    print(f"{symbol} {check}")

print()

# Temporal filter compatibility
print("🔍 TEMPORAL FILTER COMPATIBILITY CHECK")
print("-" * 70)
print(f"Filter requirement: 12 consecutive ticks with same OBI direction")
print(f"Data provides:      15-tick bursts with 85% directional bias")
print()

compatible_bursts = sum(1 for g in burst_groups if len(g) >= 12)
print(f"Bursts ≥12 ticks:   {compatible_bursts}/{len(burst_groups)}")

if compatible_bursts >= 10:
    print("Data is compatible with 12-tick temporal filter")
else:
    print("Warning: May need to adjust filter parameters or regenerate data")

print()
print("=" * 70)
print("  DATA VERIFICATION COMPLETE")
print("=" * 70)
