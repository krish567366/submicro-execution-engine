#!/usr/bin/env python3
"""
Analyze trading performance and calculate potential profit
"""

import pandas as pd
import numpy as np

# Load the metrics
df = pd.read_csv('trading_metrics.csv', names=[
    'timestamp', 'mid_price', 'spread_bps', 'position', 'realized_pnl',
    'hawkes_buy', 'hawkes_sell', 'latency_us', 'trades', 'fills', 'regime', 'queue_util'
])

# Convert timestamp to numeric
df['timestamp'] = pd.to_numeric(df['timestamp'], errors='coerce')

print("=" * 70)
print("  HFT TRADING SYSTEM - PERFORMANCE ANALYSIS")
print("=" * 70)
print()

# Basic stats
print(f"📊 Dataset Statistics:")
print(f"   Total Ticks Processed:  {len(df):,}")
print(f"   Time Period:            {(df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]) / 1e6:.2f} seconds")
print(f"   Tick Rate:              {len(df) / ((df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]) / 1e6):.0f} ticks/second")
print()

# Price statistics
print(f"💰 Market Data:")
print(f"   Average Mid Price:      ${df['mid_price'].mean():.2f}")
print(f"   Price Range:            ${df['mid_price'].min():.2f} - ${df['mid_price'].max():.2f}")
print(f"   Price Volatility:       {df['mid_price'].std():.4f} ({df['mid_price'].std() / df['mid_price'].mean() * 100:.2f}%)")
print(f"   Average Spread:         {df['spread_bps'].mean():.2f} bps")
print()

# Trading performance
total_trades = df['trades'].sum()
total_fills = df['fills'].sum()
total_pnl = df['realized_pnl'].iloc[-1] - df['realized_pnl'].iloc[0]

print(f"📈 Trading Performance:")
print(f"   Total Trades:           {int(total_trades)}")
print(f"   Total Fills:            {int(total_fills)}")
print(f"   Final Position:         {int(df['position'].iloc[-1])}")
print(f"   Realized P&L:           ${total_pnl:.2f}")
print()

# Latency statistics
print(f"⚡ System Performance:")
print(f"   Average Latency:        {df['latency_us'].mean():.2f} μs")
print(f"   Min Latency:            {df['latency_us'].min():.2f} μs")
print(f"   Max Latency:            {df['latency_us'].max():.2f} μs")
print(f"   p50 Latency:            {df['latency_us'].quantile(0.50):.2f} μs")
print(f"   p99 Latency:            {df['latency_us'].quantile(0.99):.2f} μs")
print()

# Hawkes intensity analysis
print(f"📊 Order Flow Analysis (Hawkes Process):")
print(f"   Avg Buy Intensity:      {df['hawkes_buy'].mean():.1f}")
print(f"   Avg Sell Intensity:     {df['hawkes_sell'].mean():.1f}")
df['hawkes_imbalance'] = (df['hawkes_buy'] - df['hawkes_sell']) / (df['hawkes_buy'] + df['hawkes_sell'])
print(f"   Avg Imbalance:          {df['hawkes_imbalance'].mean():.3f}")
print()

# Regime analysis
regime_counts = df['regime'].value_counts()
print(f"🛡️  Risk Regime Distribution:")
for regime, count in regime_counts.items():
    regime_name = ['NORMAL', 'ELEVATED', 'STRESSED', 'HALTED'][int(regime)]
    pct = count / len(df) * 100
    print(f"   {regime_name:12s}        {count:6d} ({pct:5.1f}%)")
print()

# SIMULATE potential profit (if trading was enabled)
print("=" * 70)
print("  💡 POTENTIAL PROFIT SIMULATION (If Trading Enabled)")
print("=" * 70)
print()

# Simple market making simulation
# Assume we capture half the spread on each trade
avg_spread_dollars = df['mid_price'].mean() * df['spread_bps'].mean() / 10000
profit_per_trade = avg_spread_dollars / 2  # Half spread capture

# Estimate trading opportunities (when Hawkes imbalance is strong)
strong_signal = df[abs(df['hawkes_imbalance']) > 0.15]
trading_opportunities = len(strong_signal)

# Assume 70% fill rate
estimated_trades = int(trading_opportunities * 0.7)
estimated_profit = estimated_trades * profit_per_trade

print(f"📊 Market Making Simulation:")
print(f"   Average Spread:              ${avg_spread_dollars:.4f} per trade")
print(f"   Half-Spread Capture:         ${profit_per_trade:.4f} per fill")
print(f"   Strong Signal Opportunities: {trading_opportunities:,}")
print(f"   Estimated Trades (70% fill): {estimated_trades:,}")
print(f"   Estimated Profit:            ${estimated_profit:.2f}")
print()

# Calculate profit rate
time_seconds = (df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]) / 1e6
if time_seconds > 0:
    profit_per_second = estimated_profit / time_seconds
    profit_per_day = profit_per_second * 86400
    
    print(f"💰 Projected Returns:")
    print(f"   Profit Rate:                 ${profit_per_second:.2f}/second")
    print(f"   Daily Projection:            ${profit_per_day:,.2f}/day")
    print(f"   Monthly Projection:          ${profit_per_day * 30:,.2f}/month")
    print(f"   Annual Projection:           ${profit_per_day * 252:,.2f}/year (252 trading days)")
    print()

# Risk metrics
print(f"⚠️  Risk Analysis:")
print(f"   Price Volatility (daily):    {df['mid_price'].std() * np.sqrt(252):.2f}%")
print(f"   Max Hawkes Imbalance:        {df['hawkes_imbalance'].abs().max():.3f}")
print(f"   System is in HALTED mode:    {(df['regime'] == 3).sum() / len(df) * 100:.1f}% of time")
print()

print("=" * 70)
print("📝 NOTES:")
print("   - Current system: 0 trades due to HALTED regime (risk protection)")
print("   - Simulation assumes market making with 70% fill rate")
print("   - Actual performance depends on market conditions and risk settings")
print("   - To enable trading: adjust risk thresholds in risk_control.hpp")
print("=" * 70)
