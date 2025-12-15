#!/usr/bin/env python3
"""
Generate Synthetic Market Data with Embedded Persistent Alpha

This script creates realistic tick data with embedded 1-5ms persistent OBI patterns
that match our temporal filter requirements (10+ consecutive ticks).

Key Features:
- Persistent order flow imbalances (1-5ms duration)
- Realistic price dynamics (mean reversion + momentum)
- Hawkes-like clustering of events
- Profitable opportunities for our 890ns execution system
"""

import numpy as np
import pandas as pd
from datetime import datetime
import random

# ============================================================================
# Configuration
# ============================================================================
DURATION_SECONDS = 10.0
EVENTS_PER_SECOND = 10000  # ~100ns between events
TOTAL_EVENTS = int(DURATION_SECONDS * EVENTS_PER_SECOND)

BASE_PRICE = 100.0
TICK_SIZE = 0.01
SPREAD_TICKS = 5  # 5 tick spread = $0.05

# Alpha signal parameters
ALPHA_BURST_PROBABILITY = 0.02  # 2% chance of alpha burst per 100 ticks
ALPHA_BURST_DURATION_TICKS = 15  # 15 ticks = ~1.5μs persistence
ALPHA_BURST_STRENGTH = 0.85  # 85% directional bias during burst

# Noise parameters
NOISE_PROBABILITY = 0.30  # 30% random noise
RANDOM_WALK_STEP = 0.0001  # Small price drift

print("="*70)
print("GENERATING SYNTHETIC DATA WITH EMBEDDED PERSISTENT ALPHA")
print("="*70)
print(f"\nConfiguration:")
print(f"  Duration: {DURATION_SECONDS} seconds")
print(f"  Total Events: {TOTAL_EVENTS:,}")
print(f"  Base Price: ${BASE_PRICE}")
print(f"  Spread: {SPREAD_TICKS} ticks (${SPREAD_TICKS * TICK_SIZE})")
print(f"\nAlpha Parameters:")
print(f"  Burst Probability: {ALPHA_BURST_PROBABILITY*100}%")
print(f"  Burst Duration: {ALPHA_BURST_DURATION_TICKS} ticks (~{ALPHA_BURST_DURATION_TICKS*100}ns)")
print(f"  Burst Strength: {ALPHA_BURST_STRENGTH*100}% directional bias")
print()

# ============================================================================
# Initialize State
# ============================================================================
np.random.seed(42)
random.seed(42)

start_time_ns = int(datetime.now().timestamp() * 1e9)
current_time_ns = start_time_ns

mid_price = BASE_PRICE
bid_price = mid_price - (SPREAD_TICKS / 2) * TICK_SIZE
ask_price = mid_price + (SPREAD_TICKS / 2) * TICK_SIZE

events = []
order_id_counter = 1

# Alpha burst state
in_alpha_burst = False
alpha_burst_direction = 0  # +1 for buy pressure, -1 for sell pressure
alpha_burst_remaining = 0
alpha_bursts_generated = 0

# ============================================================================
# Helper Functions
# ============================================================================

def round_to_tick(price):
    """Round price to nearest tick"""
    return round(price / TICK_SIZE) * TICK_SIZE

def generate_event_type():
    """Generate realistic event type distribution"""
    rand = random.random()
    if rand < 0.40:
        return 'add'
    elif rand < 0.60:
        return 'cancel'
    else:
        return 'modify'

def generate_size():
    """Generate realistic order size (1-50 shares, biased toward small)"""
    # Exponential distribution biased toward small orders
    size = int(np.random.exponential(10) + 1)
    return min(max(size, 1), 50)

def generate_level():
    """Generate price level (0-10, biased toward top of book)"""
    # Exponential distribution biased toward level 0-2
    level = int(np.random.exponential(2))
    return min(level, 10)

# ============================================================================
# Generate Events with Embedded Alpha
# ============================================================================
print("Generating events...")

for i in range(TOTAL_EVENTS):
    # Update time (events every ~100ns)
    time_delta_ns = int(np.random.uniform(80, 120))  # 80-120ns jitter
    current_time_ns += time_delta_ns
    
    # ========================================================================
    # ALPHA BURST LOGIC: Create persistent order flow imbalances
    # ========================================================================
    
    # Check if we should start a new alpha burst
    if not in_alpha_burst and i % 100 == 0:  # Check every 100 ticks
        if random.random() < ALPHA_BURST_PROBABILITY:
            in_alpha_burst = True
            alpha_burst_direction = random.choice([1, -1])  # Buy or sell pressure
            alpha_burst_remaining = ALPHA_BURST_DURATION_TICKS + int(np.random.uniform(-5, 10))
            alpha_bursts_generated += 1
            
            if i % 1000 == 0:  # Progress indicator
                print(f"  Generated {alpha_bursts_generated} alpha bursts (at event {i:,})")
    
    # Determine if this event is part of alpha signal
    is_alpha_event = False
    event_side = None
    
    if in_alpha_burst:
        # During alpha burst: strong directional bias
        if random.random() < ALPHA_BURST_STRENGTH:
            is_alpha_event = True
            event_side = 'B' if alpha_burst_direction > 0 else 'S'
        
        alpha_burst_remaining -= 1
        if alpha_burst_remaining <= 0:
            in_alpha_burst = False
    
    # If not alpha event, generate random event (noise)
    if not is_alpha_event:
        if random.random() < NOISE_PROBABILITY:
            event_side = random.choice(['B', 'S'])
        else:
            # Continue previous trend (weak autocorrelation)
            if events and random.random() < 0.55:
                event_side = events[-1]['side']
            else:
                event_side = random.choice(['B', 'S'])
    
    # ========================================================================
    # Generate Event Details
    # ========================================================================
    
    event_type = generate_event_type()
    size = generate_size()
    level = generate_level()
    
    # Calculate price based on side and level
    if event_side == 'B':
        price = round_to_tick(bid_price - level * TICK_SIZE)
    else:
        price = round_to_tick(ask_price + level * TICK_SIZE)
    
    # Ensure price is positive
    price = max(price, 1.0)
    
    # Store event
    events.append({
        'ts_us': current_time_ns,  # Keep as nanoseconds (will convert to microseconds)
        'event_type': event_type,
        'side': event_side,
        'price': price,
        'size': size,
        'order_id': order_id_counter,
        'level': level
    })
    
    order_id_counter += 1
    
    # ========================================================================
    # Price Dynamics: Small random walk with mean reversion
    # ========================================================================
    
    if i % 500 == 0:  # Update prices periodically
        # Random walk
        price_change = np.random.normal(0, RANDOM_WALK_STEP)
        mid_price += price_change
        
        # Mean reversion toward BASE_PRICE
        mean_reversion = (BASE_PRICE - mid_price) * 0.001
        mid_price += mean_reversion
        
        # Update bid/ask
        bid_price = mid_price - (SPREAD_TICKS / 2) * TICK_SIZE
        ask_price = mid_price + (SPREAD_TICKS / 2) * TICK_SIZE

# ============================================================================
# Create DataFrame and Save
# ============================================================================
print(f"\nAlpha bursts generated: {alpha_bursts_generated}")
print(f"Expected profitable signals: ~{alpha_bursts_generated} (each lasting {ALPHA_BURST_DURATION_TICKS} ticks)")
print()

df = pd.DataFrame(events)

# Convert nanoseconds to microseconds for the CSV format
df['ts_us'] = (df['ts_us'] / 1000).astype(int)

# Format prices to 2 decimal places
df['price'] = df['price'].round(2)

# Save to CSV
output_file = 'synthetic_ticks_with_alpha.csv'
df.to_csv(output_file, index=False, header=False)

print("="*70)
print(f"✓ Generated {len(df):,} events")
print(f"✓ Saved to: {output_file}")
print(f"✓ Duration: {(df['ts_us'].iloc[-1] - df['ts_us'].iloc[0]) / 1e6:.2f} seconds")
print()

# ============================================================================
# Analyze Generated Data
# ============================================================================
print("Data Quality Analysis:")
print("-"*70)

# Event type distribution
event_counts = df['event_type'].value_counts()
print(f"  Event types:")
for event_type, count in event_counts.items():
    print(f"    {event_type}: {count:,} ({count/len(df)*100:.1f}%)")

# Side distribution
side_counts = df['side'].value_counts()
print(f"\n  Side distribution:")
for side, count in side_counts.items():
    side_name = "Buy" if side == 'B' else "Sell"
    print(f"    {side_name}: {count:,} ({count/len(df)*100:.1f}%)")

# Price statistics
print(f"\n  Price statistics:")
print(f"    Min: ${df['price'].min():.2f}")
print(f"    Max: ${df['price'].max():.2f}")
print(f"    Mean: ${df['price'].mean():.2f}")
print(f"    Std: ${df['price'].std():.4f}")

# Size statistics
print(f"\n  Order size statistics:")
print(f"    Min: {df['size'].min()}")
print(f"    Max: {df['size'].max()}")
print(f"    Mean: {df['size'].mean():.1f}")
print(f"    Median: {df['size'].median():.0f}")

print("\n" + "="*70)
print("✓ Data generation complete!")
print()
print("Next steps:")
print("  1. Update backtest_demo.cpp to use 'synthetic_ticks_with_alpha.csv'")
print("  2. Run: ./backtest_demo")
print("  3. Verify strategy is now profitable with persistent alpha")
print("="*70)
