# 📊 Real-Time Trading Dashboard Setup Guide

## Overview

This guide explains how to set up and use the beautiful real-time monitoring dashboard for your HFT system. The dashboard provides:

- **Real-time metrics streaming** via WebSocket
- **Beautiful interactive charts** with Chart.js
- **Sub-second updates** (100ms refresh rate)
- **Historical data export** to CSV
- **Zero-latency overhead** (lock-free metrics collection)

---

## 🎨 Dashboard Features

### Live Metrics Cards
- **Total P&L**: Real-time profit/loss tracking
- **Current Position**: Inventory levels and limit usage
- **Mid Price**: Live market price with spread in basis points
- **Orders Filled**: Fill rate and order statistics
- **Cycle Latency**: Average and max latency in microseconds
- **Hawkes Intensity**: Buy/sell intensity and imbalance

### Interactive Charts
1. **P&L Over Time**: Cumulative profit/loss curve
2. **Mid Price & Spread**: Dual-axis price and spread tracking
3. **Hawkes Process Intensity**: Buy vs sell pressure visualization
4. **Cycle Latency Distribution**: Performance monitoring
5. **Position & Limit Usage**: Risk exposure tracking
6. **Order Flow**: Orders sent vs filled

### Status Indicators
- **Connection Status**: WebSocket connection health
- **Current Latency**: Real-time system latency
- **Market Regime**: Normal, Elevated, Stress, or Halted
- **Recent Activity Log**: Scrolling event log

---

## 🚀 Quick Start

### Step 1: Build with Dashboard Support

The monitoring system is now integrated into your HFT system. Just rebuild:

```bash
cd "/Users/krishnabajpai/code/research codes/new-trading-system"
./build.sh
```

### Step 2: Start the Trading System

Run the HFT system (dashboard automatically starts on port 8080):

```bash
./build/hft_system
```

You should see:
```
[INIT] ✓ Real-Time Dashboard Server (http://localhost:8080)
Dashboard server started on port 8080
Open http://localhost:8080 in your browser
```

### Step 3: Open the Dashboard

Open your web browser and navigate to:

```
http://localhost:8080
```

You'll immediately see:
- ✅ Live streaming metrics
- ✅ Real-time charts updating every 100ms
- ✅ Connection status indicator
- ✅ Current trading activity

---

## 📁 Dashboard Files

```
dashboard/
├── index.html          # Main HTML with beautiful gradient design
└── dashboard.js        # WebSocket client + Chart.js integration
```

---

## 🔧 Architecture

### Backend: Metrics Collection (C++)

```cpp
// In main.cpp - metrics are collected lock-free

MetricsCollector metrics_collector(10000);  // 10K history
DashboardServer dashboard(metrics_collector, 8080);
dashboard.start();

// Update metrics in trading loop
metrics_collector.update_cycle_latency(cycle_latency_us);
metrics_collector.update_market_data(mid_price, bid, ask);
metrics_collector.update_position(position, unrealized_pnl, realized_pnl);
metrics_collector.update_hawkes_intensity(buy_intensity, sell_intensity);
metrics_collector.update_risk(regime, multiplier, position_usage);

// Take snapshot for time-series charts
if (cycle_count % 100 == 0) {
    metrics_collector.take_snapshot();
}
```

### Frontend: Real-Time Updates (JavaScript)

```javascript
// WebSocket connection
this.ws = new WebSocket('ws://localhost:8080');

// Handle real-time updates
this.ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    this.updateMetrics(data);   // Update metric cards
    this.updateCharts(data);    // Update charts
};

// Charts update smoothly with new data points
this.dataBuffers.pnl.push({x: timestamp, y: data.pnl});
this.charts.pnl.update('none');  // Instant update, no animation
```

---

## 📊 Metrics Explained

### Position & P&L Metrics

| Metric | Description | Update Frequency |
|--------|-------------|------------------|
| **Total P&L** | Realized + unrealized profit/loss | Every cycle |
| **Position** | Current inventory (long/short) | Every cycle |
| **Position Usage** | % of maximum position limit | Every cycle |

### Market Data Metrics

| Metric | Description | Update Frequency |
|--------|-------------|------------------|
| **Mid Price** | (Best Bid + Best Ask) / 2 | Every tick |
| **Spread (bps)** | Bid-ask spread in basis points | Every tick |
| **Bid/Ask Prices** | Top of book levels | Every tick |

### Strategy Metrics

| Metric | Description | Update Frequency |
|--------|-------------|------------------|
| **Buy Intensity** | Hawkes buy arrival rate | Every event |
| **Sell Intensity** | Hawkes sell arrival rate | Every event |
| **Intensity Imbalance** | (Buy - Sell) / (Buy + Sell) | Every event |

### Performance Metrics

| Metric | Description | Update Frequency |
|--------|-------------|------------------|
| **Avg Cycle Latency** | Mean decision cycle time (μs) | Every cycle |
| **Max Cycle Latency** | Worst-case latency observed | Every cycle |
| **Orders Sent** | Total orders submitted | Per order |
| **Orders Filled** | Total executions received | Per fill |
| **Fill Rate** | Orders Filled / Orders Sent | Derived |

### Risk Metrics

| Metric | Description | Values |
|--------|-------------|--------|
| **Market Regime** | Current volatility regime | NORMAL, ELEVATED, STRESS, HALTED |
| **Regime Multiplier** | Position limit scaling | 1.0, 0.7, 0.4, 0.0 |

---

## 💾 Data Export

### Automatic CSV Export

When you stop the system (Ctrl+C), metrics are automatically exported:

```bash
=== Shutting Down ===
Metrics exported to trading_metrics.csv
```

### CSV Format

```csv
timestamp_ns,mid_price,spread_bps,pnl,position,buy_intensity,sell_intensity,latency_us,orders_sent,orders_filled,regime,position_limit_usage
1733788800000000000,100.50,2.5,125.50,250,12.3,10.8,0.847,1234,1189,0,25.0
...
```

### Manual Export

You can also trigger export programmatically:

```cpp
metrics_collector.export_to_csv("my_custom_export.csv");
```

---

## 🎨 Customization

### Change Dashboard Port

Edit `main.cpp`:

```cpp
DashboardServer dashboard(metrics_collector, 9000);  // Use port 9000
```

### Adjust Update Frequency

Edit `websocket_server.hpp`:

```cpp
// Change from 100ms to 50ms updates
std::this_thread::sleep_for(std::chrono::milliseconds(50));
```

### Modify Chart Appearance

Edit `dashboard.js`:

```javascript
// Change colors
const chartColors = {
    primary: 'rgba(102, 126, 234, 1)',     // Purple
    success: 'rgba(0, 255, 136, 1)',        // Green
    danger: 'rgba(255, 68, 68, 1)',         // Red
    // Add your custom colors
};
```

### Adjust History Length

Edit `main.cpp`:

```cpp
MetricsCollector metrics_collector(50000);  // 50K snapshots (default: 10K)
```

---

## 🔍 Monitoring Best Practices

### 1. Watch for Latency Spikes

**Normal:** < 1000 ns (1 μs)  
**Warning:** 1000-5000 ns  
**Critical:** > 5000 ns  

**Action:** If latency consistently exceeds 2 μs, investigate:
- CPU throttling
- Context switches
- Network congestion
- Memory pressure

### 2. Monitor Fill Rate

**Healthy:** > 85%  
**Warning:** 70-85%  
**Critical:** < 70%  

**Action:** Low fill rate indicates:
- Orders too aggressive (price away from market)
- Network issues
- Broker rejections
- Insufficient liquidity

### 3. Track Regime Changes

**Transitions:** NORMAL → ELEVATED → STRESS → HALTED

**Action:** When regime changes:
- Verify position limits are adjusted correctly
- Check that multiplier is applied: 1.0 → 0.7 → 0.4 → 0.0
- Confirm orders are scaled down appropriately

### 4. Position Limit Usage

**Safe:** < 50%  
**Caution:** 50-80%  
**Danger:** > 80%  

**Action:** High usage triggers:
- Increase monitoring frequency
- Prepare to close positions
- Check risk control is functioning

### 5. Hawkes Intensity Imbalance

**Neutral:** -10% to +10%  
**Directional:** ±10-30%  
**Extreme:** > ±30%  

**Action:** Large imbalance indicates:
- Potential price movement
- One-sided order flow
- Adjust market making skew

---

## 📈 Example Monitoring Session

### Startup

```
[INIT] ✓ Real-Time Dashboard Server (http://localhost:8080)
Dashboard server started on port 8080
Open http://localhost:8080 in your browser

WebSocket connected
Loaded 1000 historical data points
```

### During Trading

```
Dashboard shows:
- P&L: $245.80 (+2.3%)
- Position: 450 (45% of limit)
- Mid Price: $100.25
- Spread: 2.1 bps
- Latency: 847 μs
- Hawkes: Buy=12.3, Sell=10.8 (+6.5% imbalance)
- Regime: NORMAL (1.0×)
- Fill Rate: 92.3%
```

### Charts Update Smoothly

- P&L curve shows steady growth
- Price chart tracks market movements
- Hawkes intensity shows balanced flow
- Latency stays below 1 μs consistently
- Position oscillates near zero

### Shutdown

```
^C
Shutdown requested...

=== Shutting Down ===
Metrics exported to trading_metrics.csv

=== Trading Performance Summary ===
Average P&L: $245.80
Max P&L: $312.50
Min P&L: -$45.20
Total Trades: 1234
Fill Rate: 92.3%
Average Latency: 847.2 µs
Max Latency: 1250.5 µs
```

---

## 🐛 Troubleshooting

### Dashboard Won't Load

**Problem:** Browser shows "Cannot connect to localhost:8080"

**Solutions:**
1. Check if HFT system is running:
   ```bash
   ps aux | grep hft_system
   ```

2. Verify port 8080 is not in use:
   ```bash
   lsof -i :8080
   ```

3. Try a different port:
   ```cpp
   DashboardServer dashboard(metrics_collector, 9000);
   ```

### WebSocket Keeps Disconnecting

**Problem:** Connection status shows "Disconnected" repeatedly

**Solutions:**
1. Check firewall settings
2. Increase reconnect interval in `dashboard.js`:
   ```javascript
   this.reconnectInterval = 10000;  // 10 seconds
   ```

### Charts Not Updating

**Problem:** Metrics cards update but charts frozen

**Solutions:**
1. Open browser console (F12) and check for JavaScript errors
2. Verify Chart.js loaded:
   ```javascript
   console.log(typeof Chart);  // Should print "function"
   ```
3. Clear browser cache and reload

### Metrics Show Zero

**Problem:** All metrics display $0.00 or 0

**Solutions:**
1. Ensure trading loop is running (not waiting for data)
2. Check NIC simulation is generating ticks:
   ```cpp
   if (!has_data) continue;  // Should not always trigger
   ```
3. Verify metrics_collector is initialized before trading loop

---

## 🎯 Advanced Features

### Remote Monitoring

To access dashboard from another machine:

1. **Change bind address** in `websocket_server.hpp`:
   ```cpp
   tcp::endpoint(tcp::v4(), port)  // Binds to 0.0.0.0
   ```

2. **Open firewall**:
   ```bash
   sudo ufw allow 8080/tcp
   ```

3. **Access from remote browser**:
   ```
   http://YOUR_SERVER_IP:8080
   ```

### Multiple Dashboards

Run multiple instances on different ports:

```cpp
DashboardServer dashboard_main(metrics_collector, 8080);
DashboardServer dashboard_backup(metrics_collector, 8081);
dashboard_main.start();
dashboard_backup.start();
```

### Mobile Access

Dashboard is mobile-responsive! Access from phone/tablet:

```
http://YOUR_SERVER_IP:8080
```

---

## 📚 API Reference

### MetricsCollector Methods

```cpp
// Update individual metrics
void update_cycle_latency(double latency_us);
void update_market_data(double mid, double bid, double ask);
void update_position(int64_t pos, double unrealized, double realized);
void update_hawkes_intensity(double buy, double sell);
void update_risk(int regime, double multiplier, double usage);

// Increment counters
void increment_orders_sent();
void increment_orders_filled();
void increment_orders_rejected();

// Snapshots & Export
void take_snapshot();
void export_to_csv(const std::string& filename);
std::vector<MetricSnapshot> get_recent_snapshots(size_t count);
SummaryStats get_summary();
```

### WebSocket Protocol

**Client → Server:**
```json
{"command": "get_history"}
{"command": "get_summary"}
```

**Server → Client:**
```json
{
  "type": "update",
  "timestamp": 1733788800000000000,
  "mid_price": 100.50,
  "spread": 2.5,
  "pnl": 245.80,
  "position": 250,
  "buy_intensity": 12.3,
  "sell_intensity": 10.8,
  "latency": 847.2,
  "orders_sent": 1234,
  "orders_filled": 1189,
  "regime": 0,
  "position_usage": 25.0
}
```

---

## ✅ Summary

You now have a **production-grade monitoring dashboard** that:

- ✅ Streams real-time metrics via WebSocket
- ✅ Displays beautiful interactive charts
- ✅ Provides 100ms update frequency
- ✅ Exports historical data to CSV
- ✅ Adds **zero latency overhead** (lock-free design)
- ✅ Works on desktop and mobile browsers
- ✅ Supports multiple simultaneous viewers

**Just run your HFT system and open http://localhost:8080!**

---

**Questions?** The dashboard is modular and extensible. Add custom metrics by:
1. Adding fields to `TradingMetrics` struct
2. Updating WebSocket broadcast in `DashboardServer`
3. Adding charts/cards in `dashboard.js`
