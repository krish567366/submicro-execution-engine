#!/usr/bin/env python3
"""
Simple backtest runner - loads CSV and shows how much profit the HFT system would make
"""

import pandas as pd
import numpy as np

print("=" * 80)
print("  🚀 HFT SYSTEM BACKTEST - PROFIT ANALYSIS")
print("=" * 80)
print()

# Load the synthetic tick data
print("Loading synthetic_ticks.csv...")
ticks = pd.read_csv('synthetic_ticks.csv')
print(f"✅ Loaded {len(ticks):,} ticks\n")

# Show data info
print(f"📊 Data Overview:")
print(f"   Time Range:     {ticks['ts_us'].iloc[0]} to {ticks['ts_us'].iloc[-1]}")
print(f"   Duration:       {(ticks['ts_us'].iloc[-1] - ticks['ts_us'].iloc[0]) / 1e6:.2f} seconds")
print(f"   Event Types:    {ticks['event_type'].unique()}")
print(f"   Price Range:    ${ticks['price'].min():.2f} - ${ticks['price'].max():.2f}")
print()

# Build order book snapshots
print("🔨 Building order book...")
bid_prices = []
ask_prices = []
bid_sizes = []
ask_sizes = []
spreads = []
timestamps = []

current_bids = {}  # level -> (price, size)
current_asks = {}  # level -> (price, size)

for idx, row in ticks.iterrows():
    if pd.isna(row['side']):
        continue
        
    level = int(row['level']) if not pd.isna(row['level']) else 0
    price = row['price']
    size = row['size']
    side = row['side']
    event = row['event_type']
    
    # Update order book
    if side == 'B':
        if event == 'add' or event == 'modify':
            current_bids[level] = (price, size)
        elif event == 'cancel':
            if level in current_bids:
                del current_bids[level]
    else:  # 'S'
        if event == 'add' or event == 'modify':
            current_asks[level] = (price, size)
        elif event == 'cancel':
            if level in current_asks:
                del current_asks[level]
    
    # Take snapshot every 100 events
    if idx % 100 == 0 and current_bids and current_asks:
        best_bid = max([p for p, s in current_bids.values()])
        best_ask = min([p for p, s in current_asks.values()])
        bid_size = sum([s for p, s in current_bids.values()])
        ask_size = sum([s for p, s in current_asks.values()])
        
        bid_prices.append(best_bid)
        ask_prices.append(best_ask)
        bid_sizes.append(bid_size)
        ask_sizes.append(ask_size)
        spreads.append(best_ask - best_bid)
        timestamps.append(row['ts_us'])

print(f"✅ Built {len(bid_prices)} order book snapshots\n")

# Create market data DataFrame
market_data = pd.DataFrame({
    'timestamp': timestamps,
    'bid': bid_prices,
    'ask': ask_prices,
    'mid': [(b + a) / 2 for b, a in zip(bid_prices, ask_prices)],
    'spread': spreads,
    'bid_size': bid_sizes,
    'ask_size': ask_sizes
})

print("=" * 80)
print("  💰 MARKET MAKING SIMULATION")
print("=" * 80)
print()

# Market making parameters
POSITION_LIMIT = 100
RISK_AVERSION = 0.01
initial_cash = 100000.0

# State
position = 0
cash = initial_cash
trades = []
pnl_history = []

print("Running market making strategy...")
print()

for idx in range(1, len(market_data)):
    row = market_data.iloc[idx]
    prev_row = market_data.iloc[idx - 1]
    
    mid = row['mid']
    spread = row['spread']
    
    # Calculate inventory penalty (Avellaneda-Stoikov style)
    inventory_penalty = position * RISK_AVERSION * 0.0001  # simplified
    
    # Calculate our quotes
    our_bid = mid - spread/2 - inventory_penalty
    our_ask = mid + spread/2 - inventory_penalty
    
    # Check if we can trade (simplified - assume we get filled if we're competitive)
    market_bid = row['bid']
    market_ask = row['ask']
    
    # If market crosses our bid (someone hits our bid), we sell
    if market_ask <= our_bid and position > -POSITION_LIMIT:
        trade_price = our_bid
        position -= 10  # Sell 10 shares
        cash += trade_price * 10
        trades.append({
            'time': row['timestamp'],
            'side': 'SELL',
            'price': trade_price,
            'size': 10,
            'position': position
        })
    
    # If market crosses our ask (someone lifts our ask), we buy
    elif market_bid >= our_ask and position < POSITION_LIMIT:
        trade_price = our_ask
        position += 10  # Buy 10 shares
        cash -= trade_price * 10
        trades.append({
            'time': row['timestamp'],
            'side': 'BUY',
            'price': trade_price,
            'size': 10,
            'position': position
        })
    
    # Calculate unrealized P&L
    unrealized_pnl = position * mid
    total_pnl = (cash - initial_cash) + unrealized_pnl
    pnl_history.append(total_pnl)

print(f"✅ Simulation complete\n")

# Calculate results
trades_df = pd.DataFrame(trades) if trades else pd.DataFrame()
final_pnl = pnl_history[-1] if pnl_history else 0
total_trades = len(trades)

print("=" * 80)
print("  📈 RESULTS")
print("=" * 80)
print()

print(f"💼 Trading Statistics:")
print(f"   Total Trades:           {total_trades:,}")
print(f"   Final Position:         {position}")
print(f"   Final Cash:             ${cash:,.2f}")
print()

print(f"💰 Profit & Loss:")
print(f"   Initial Capital:        ${initial_cash:,.2f}")
print(f"   Final Cash + Position:  ${cash + (position * market_data.iloc[-1]['mid']):,.2f}")
print(f"   Total P&L:              ${final_pnl:,.2f}")
print(f"   Return:                 {(final_pnl / initial_cash * 100):.2f}%")
print()

if total_trades > 0:
    duration_seconds = (timestamps[-1] - timestamps[0]) / 1e6
    trades_per_second = total_trades / duration_seconds
    pnl_per_trade = final_pnl / total_trades
    
    print(f"⚡ Performance:")
    print(f"   Duration:               {duration_seconds:.2f} seconds")
    print(f"   Trades/Second:          {trades_per_second:.2f}")
    print(f"   P&L per Trade:          ${pnl_per_trade:.4f}")
    print()
    
    # Extrapolate to full day
    seconds_per_day = 6.5 * 3600  # Trading hours
    daily_pnl = (final_pnl / duration_seconds) * seconds_per_day
    
    print(f"📊 Projections:")
    print(f"   Daily P&L (projected):  ${daily_pnl:,.2f}")
    print(f"   Monthly (21 days):      ${daily_pnl * 21:,.2f}")
    print(f"   Annual (252 days):      ${daily_pnl * 252:,.2f}")
    print()

print(f"📉 Market Statistics:")
print(f"   Avg Mid Price:          ${market_data['mid'].mean():.2f}")
print(f"   Avg Spread:             ${market_data['spread'].mean():.4f}")
print(f"   Spread (bps):           {(market_data['spread'].mean() / market_data['mid'].mean() * 10000):.2f} bps")
print()

print("=" * 80)
print("✅ Backtest Complete!")
print("=" * 80)
