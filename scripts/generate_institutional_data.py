#!/usr/bin/env python3
"""
Institutional-Grade Market Data Generator
Creates perfect synthetic data with all verification requirements
"""

import numpy as np
import pandas as pd
import hashlib
import json
from datetime import datetime
import sys

class InstitutionalDataGenerator:
    def __init__(self):
        self.seed = 42  # Deterministic seed for reproducibility
        np.random.seed(self.seed)
        
        # Market parameters
        self.base_price = 100.0
        self.tick_size = 0.01
        self.spread_ticks = 5
        
        # Time parameters (nanosecond precision)
        self.start_time_ns = 1765351552000000000  # Dec 14, 2025 specific timestamp
        self.duration_seconds = 10.0
        self.events_per_second = 10000
        self.total_events = int(self.duration_seconds * self.events_per_second)
        
        # Alpha burst parameters
        self.burst_probability = 0.02
        self.burst_duration = 15  # ticks
        self.burst_strength = 0.85
        
        # Generated data
        self.market_data = []
        self.alpha_bursts = []
        
    def generate_market_data(self):
        """Generate synthetic market data with embedded alpha"""
        print("=" * 70)
        print("  INSTITUTIONAL-GRADE MARKET DATA GENERATOR")
        print("=" * 70)
        print()
        print("Configuration:")
        print(f"  Deterministic Seed: {self.seed}")
        print(f"  Start Time:         {self.start_time_ns} ns")
        print(f"  Duration:           {self.duration_seconds} seconds")
        print(f"  Total Events:       {self.total_events:,}")
        print(f"  Base Price:         ${self.base_price}")
        print()
        
        # Initialize market state
        current_price = self.base_price
        current_time_ns = self.start_time_ns
        time_increment_ns = int((self.duration_seconds * 1e9) / self.total_events)
        
        # Track alpha burst state
        in_burst = False
        burst_remaining = 0
        burst_direction = 0  # 1=buy, -1=sell
        burst_count = 0
        
        order_id = 1
        
        print("Generating market events...")
        
        for i in range(self.total_events):
            # Check if we should start a new alpha burst
            if not in_burst and np.random.random() < self.burst_probability / 100:
                in_burst = True
                burst_remaining = self.burst_duration
                burst_direction = np.random.choice([1, -1])
                burst_count += 1
                self.alpha_bursts.append({
                    'start_event': i,
                    'direction': 'BUY' if burst_direction > 0 else 'SELL',
                    'duration': self.burst_duration
                })
                
                if burst_count % 5 == 0:
                    print(f"  Generated {burst_count} alpha bursts (at event {i:,})")
            
            # Determine event characteristics
            if in_burst:
                # During alpha burst: strong directional bias
                if np.random.random() < self.burst_strength:
                    side = 'B' if burst_direction > 0 else 'S'
                else:
                    side = 'S' if burst_direction > 0 else 'B'
                
                burst_remaining -= 1
                if burst_remaining <= 0:
                    in_burst = False
            else:
                # Normal market: balanced
                side = np.random.choice(['B', 'S'])
            
            # Event type distribution
            event_type = np.random.choice(['add', 'modify', 'cancel'], p=[0.40, 0.40, 0.20])
            
            # Price with small random walk
            current_price += np.random.normal(0, 0.0001)
            current_price = np.clip(current_price, 99.8, 100.2)
            
            # Round to tick size
            price = round(current_price / self.tick_size) * self.tick_size
            
            # Order size
            size = np.random.randint(1, 51)
            
            # Depth level
            level = np.random.randint(0, 7)
            
            # Create event
            event = {
                'timestamp_ns': current_time_ns,
                'event_type': event_type,
                'side': side,
                'price': price,
                'size': size,
                'order_id': order_id,
                'level': level
            }
            
            self.market_data.append(event)
            
            # Increment time (with small jitter for realism)
            jitter = np.random.randint(-10, 11)  # ±10ns jitter
            current_time_ns += time_increment_ns + jitter
            order_id += 1
        
        print(f"\nAlpha bursts generated: {burst_count}")
        print(f"Expected profitable signals: ~{burst_count} (each lasting {self.burst_duration} ticks)")
        print()
        
    def save_market_data(self, filename="synthetic_ticks_with_alpha.csv"):
        """Save market data to CSV with proper format"""
        print(f"Saving market data to {filename}...")
        
        # Convert to DataFrame
        df = pd.DataFrame(self.market_data)
        
        # Save without header for C++ parser compatibility
        with open(filename, 'w') as f:
            for _, row in df.iterrows():
                f.write(f"{row['timestamp_ns']},{row['event_type']},{row['side']},"
                       f"{row['price']:.4f},{row['size']},{row['order_id']},{row['level']}\n")
        
        print(f"✓ Saved {len(self.market_data):,} events to {filename}")
        
        # Calculate SHA256 checksum
        sha256_hash = hashlib.sha256()
        with open(filename, 'rb') as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        
        checksum = sha256_hash.hexdigest()
        print(f"✓ SHA256 Checksum: {checksum}")
        
        # Save metadata
        metadata = {
            'filename': filename,
            'sha256': checksum,
            'seed': self.seed,
            'total_events': len(self.market_data),
            'start_time_ns': self.start_time_ns,
            'duration_seconds': self.duration_seconds,
            'alpha_bursts': len(self.alpha_bursts),
            'burst_details': self.alpha_bursts,
            'generation_timestamp': datetime.now().isoformat()
        }
        
        with open('market_data_metadata.json', 'w') as f:
            json.dump(metadata, f, indent=2)
        
        print(f"✓ Metadata saved to market_data_metadata.json")
        print()
        
        return checksum
    
    def print_statistics(self):
        """Print data quality statistics"""
        print("=" * 70)
        print("  DATA QUALITY VERIFICATION")
        print("=" * 70)
        print()
        
        df = pd.DataFrame(self.market_data)
        
        # Time analysis
        timestamps = df['timestamp_ns'].values
        duration_ns = timestamps[-1] - timestamps[0]
        print("Time Statistics:")
        print(f"  Start time:     {timestamps[0]:,} ns")
        print(f"  End time:       {timestamps[-1]:,} ns")
        print(f"  Duration:       {duration_ns / 1e9:.6f} seconds")
        print(f"  Avg tick gap:   {np.mean(np.diff(timestamps)):.0f} ns")
        print()
        
        # Event type distribution
        print("Event Type Distribution:")
        for event_type, count in df['event_type'].value_counts().items():
            pct = count * 100.0 / len(df)
            print(f"  {event_type:<12} {count:>8,} ({pct:>5.1f}%)")
        print()
        
        # Side distribution
        print("Side Distribution:")
        for side, count in df['side'].value_counts().items():
            pct = count * 100.0 / len(df)
            side_name = "Buy" if side == 'B' else "Sell"
            print(f"  {side_name:<12} {count:>8,} ({pct:>5.1f}%)")
        print()
        
        # Price statistics
        prices = df['price'].values
        print("Price Statistics:")
        print(f"  Min:            ${prices.min():.4f}")
        print(f"  Max:            ${prices.max():.4f}")
        print(f"  Mean:           ${prices.mean():.4f}")
        print(f"  Std Dev:        ${prices.std():.4f}")
        print()
        
        # Size statistics
        sizes = df['size'].values
        print("Order Size Statistics:")
        print(f"  Min:            {sizes.min():,}")
        print(f"  Max:            {sizes.max():,}")
        print(f"  Mean:           {sizes.mean():.1f}")
        print(f"  Median:         {int(np.median(sizes)):,}")
        print()
        
        # Alpha burst summary
        print("Alpha Burst Summary:")
        print(f"  Total bursts:   {len(self.alpha_bursts)}")
        print(f"  Burst duration: {self.burst_duration} ticks")
        print(f"  Burst strength: {self.burst_strength*100:.0f}% directional bias")
        print()
        
        # Verification checks
        print("Verification Checks:")
        checks = [
            ("✓" if len(df) >= 90000 else "✗", f"Event count: {len(df):,} >= 90,000"),
            ("✓" if duration_ns/1e9 >= 9.0 else "✗", f"Duration: {duration_ns/1e9:.3f}s >= 9.0s"),
            ("✓" if len(self.alpha_bursts) >= 10 else "✗", f"Alpha bursts: {len(self.alpha_bursts)} >= 10"),
            ("✓" if abs(df[df['side']=='B'].shape[0] - df[df['side']=='S'].shape[0]) / len(df) < 0.05 else "✗", "Side balance within 5%"),
            ("✓" if prices.std() < 0.2 else "✗", f"Price volatility: σ={prices.std():.4f} < 0.2"),
        ]
        
        for symbol, check in checks:
            print(f"  {symbol} {check}")
        
        print()

def main():
    """Generate institutional-grade market data"""
    generator = InstitutionalDataGenerator()
    
    # Generate data
    generator.generate_market_data()
    
    # Save data and get checksum
    checksum = generator.save_market_data()
    
    # Print statistics
    generator.print_statistics()
    
    print("=" * 70)
    print("  DATA GENERATION COMPLETE")
    print("=" * 70)
    print()
    print("Generated Files:")
    print("  • synthetic_ticks_with_alpha.csv  (Market data)")
    print("  • market_data_metadata.json       (SHA256 + metadata)")
    print()
    print("Next Steps:")
    print("  1. Verify SHA256 checksum matches in replay logs")
    print("  2. Run backtesting engine with this data")
    print("  3. Generate institutional verification reports")
    print()

if __name__ == "__main__":
    main()
