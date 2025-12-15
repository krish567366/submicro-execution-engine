#!/bin/bash

# CSV Data Demo - Runs HFT system on synthetic_ticks.csv
# Production code remains untouched

set -e

echo "════════════════════════════════════════════════════════"
echo "  HFT System - CSV Data Demo"
echo "════════════════════════════════════════════════════════"
echo ""

# Check CSV file exists
if [[ ! -f "synthetic_ticks.csv" ]]; then
    echo "❌ synthetic_ticks.csv not found!"
    exit 1
fi

# Count rows
ROWS=$(wc -l < synthetic_ticks.csv)
echo "✅ Found synthetic_ticks.csv with $ROWS rows"
echo ""

# Create temporary CSV reader demo
cat > /tmp/csv_demo.cpp << 'EOF'
#include "common_types.hpp"
#include "hawkes_engine.hpp"
#include "fpga_inference.hpp"
#include "avellaneda_stoikov.hpp"
#include "risk_control.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <map>

using namespace hft;

// Simple order book for demo
struct SimpleOrderBook {
    std::map<double, uint64_t, std::greater<double>> bids;  // Sorted descending
    std::map<double, uint64_t> asks;  // Sorted ascending
    
    void update_bid(double price, uint64_t size) {
        if (size == 0) bids.erase(price);
        else bids[price] = size;
    }
    
    void update_ask(double price, uint64_t size) {
        if (size == 0) asks.erase(price);
        else asks[price] = size;
    }
    
    double best_bid() const {
        return bids.empty() ? 0.0 : bids.begin()->first;
    }
    
    double best_ask() const {
        return asks.empty() ? 0.0 : asks.begin()->first;
    }
};

// Parse CSV row
struct CSVEvent {
    uint64_t ts_us;
    std::string event_type;
    char side;  // 'B' or 'S'
    double price;
    uint64_t size;
    uint64_t order_id;
    int level;
};

CSVEvent parse_csv_line(const std::string& line) {
    CSVEvent event = {};
    std::stringstream ss(line);
    std::string token;
    
    // ts_us
    std::getline(ss, token, ',');
    event.ts_us = std::stoull(token);
    
    // event_type
    std::getline(ss, token, ',');
    event.event_type = token;
    
    // side
    std::getline(ss, token, ',');
    event.side = token.empty() ? ' ' : token[0];
    
    // price
    std::getline(ss, token, ',');
    event.price = token.empty() ? 0.0 : std::stod(token);
    
    // size
    std::getline(ss, token, ',');
    event.size = token.empty() ? 0 : std::stoull(token);
    
    // order_id
    std::getline(ss, token, ',');
    event.order_id = token.empty() ? 0 : std::stoull(token);
    
    // level
    std::getline(ss, token, ',');
    event.level = token.empty() ? 0 : std::stoi(token);
    
    return event;
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  HFT System - CSV Market Data Replay                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    // Initialize components
    HawkesIntensityEngine hawkes(0.5, 0.5, 0.8, 0.3, 1e-6, 1.5);
    DynamicMMStrategy strategy(0.01, 0.15, 300.0, 10.0, 0.01, 850);
    RiskControl risk(1000, 10000.0, 100000.0);
    FPGA_DNN_Inference fpga_inference(12, 16);
    SimpleOrderBook lob;
    
    // Trading state
    int64_t position = 0;
    double realized_pnl = 0.0;
    uint64_t total_events = 0;
    uint64_t total_trades = 0;
    
    // Performance tracking
    std::vector<int64_t> latencies_ns;
    latencies_ns.reserve(1000000);
    
    // Open CSV file
    std::ifstream file("synthetic_ticks.csv");
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open synthetic_ticks.csv\n";
        return 1;
    }
    
    // Skip header
    std::string line;
    std::getline(file, line);
    
    std::cout << "Processing market data from CSV...\n\n";
    
    // Track order book state
    double best_bid = 0.0;
    double best_ask = 0.0;
    uint64_t last_print_event = 0;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Parse CSV event
        CSVEvent event = parse_csv_line(line);
        total_events++;
        
        // Update order book based on event type
        if (event.event_type == "add" || event.event_type == "modify") {
            if (event.side == 'B') {
                lob.update_bid(event.price, event.size);
            } else if (event.side == 'S') {
                lob.update_ask(event.price, event.size);
            }
        } else if (event.event_type == "cancel") {
            if (event.side == 'B') {
                lob.update_bid(event.price, 0);
            } else if (event.side == 'S') {
                lob.update_ask(event.price, 0);
            }
        } else if (event.event_type == "snapshot") {
            // Initialize order book
            lob.update_bid(99.95, 1000);
            lob.update_ask(100.05, 1000);
        }
        
        // Get best bid/ask from order book
        best_bid = lob.best_bid();
        best_ask = lob.best_ask();
        
        // Only process if we have valid book
        if (best_bid > 0.0 && best_ask > 0.0 && best_bid < best_ask) {
            double mid_price = (best_bid + best_ask) / 2.0;
            
            // Update Hawkes process
            TradingEvent te;
            te.arrival_time = now();
            te.event_type = (event.side == 'B') ? Side::BUY : Side::SELL;
            te.asset_id = 1;
            hawkes.update(te);
            
            // Create market tick
            MarketTick tick;
            tick.timestamp = now();
            tick.bid_price = best_bid;
            tick.ask_price = best_ask;
            tick.mid_price = mid_price;
            tick.bid_size = 100;
            tick.ask_size = 100;
            tick.depth_levels = 10;
            
            // Extract features
            MarketTick prev_tick = tick;
            MarketTick ref_tick = tick;
            
            auto features = FPGA_DNN_Inference::extract_features(
                tick, prev_tick, ref_tick,
                hawkes.get_buy_intensity(),
                hawkes.get_sell_intensity()
            );
            
            // FPGA inference
            auto prediction = fpga_inference.predict(features);
            
            // Strategy calculation
            auto quotes = strategy.calculate_quotes(mid_price, position, 300.0, 0.0001);
            
            // Risk checks
            Order test_order;
            test_order.price = quotes.bid_price;
            test_order.quantity = quotes.bid_size;
            test_order.side = Side::BUY;
            
            bool risk_passed = risk.check_pre_trade_limits(test_order, position);
            
            // Measure latency
            auto end_time = std::chrono::high_resolution_clock::now();
            int64_t latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time - start_time).count();
            latencies_ns.push_back(latency_ns);
            
            // Print progress every 10000 events
            if (total_events - last_print_event >= 10000) {
                std::cout << "\r" << std::setw(10) << total_events << " events | "
                          << "Mid: $" << std::fixed << std::setprecision(2) << mid_price << " | "
                          << "Spread: " << std::setprecision(4) << (best_ask - best_bid) << " | "
                          << "Hawkes: B=" << std::setprecision(0) << hawkes.get_buy_intensity() 
                          << " S=" << hawkes.get_sell_intensity() << " | "
                          << "Latency: " << std::setprecision(2) << (latency_ns / 1000.0) << " µs  "
                          << std::flush;
                last_print_event = total_events;
            }
        }
    }
    
    std::cout << "\n\n";
    file.close();
    
    // Calculate statistics
    if (!latencies_ns.empty()) {
        std::sort(latencies_ns.begin(), latencies_ns.end());
        
        int64_t min_ns = latencies_ns.front();
        int64_t max_ns = latencies_ns.back();
        int64_t sum_ns = 0;
        for (auto l : latencies_ns) sum_ns += l;
        double avg_ns = static_cast<double>(sum_ns) / latencies_ns.size();
        
        size_t p50_idx = latencies_ns.size() * 50 / 100;
        size_t p90_idx = latencies_ns.size() * 90 / 100;
        size_t p99_idx = latencies_ns.size() * 99 / 100;
        size_t p999_idx = latencies_ns.size() * 999 / 1000;
        
        std::cout << "╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Performance Statistics (CSV Replay)                   ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
        
        std::cout << "Total Events:  " << total_events << "\n";
        std::cout << "Total Samples: " << latencies_ns.size() << "\n\n";
        
        std::cout << "Latency Statistics:\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "min      = " << std::setw(10) << min_ns << " ns  (" 
                  << (min_ns / 1000.0) << " µs)\n";
        std::cout << "mean     = " << std::setw(10) << avg_ns << " ns  (" 
                  << (avg_ns / 1000.0) << " µs)\n";
        std::cout << "p50      = " << std::setw(10) << latencies_ns[p50_idx] << " ns  (" 
                  << (latencies_ns[p50_idx] / 1000.0) << " µs)\n";
        std::cout << "p90      = " << std::setw(10) << latencies_ns[p90_idx] << " ns  (" 
                  << (latencies_ns[p90_idx] / 1000.0) << " µs)\n";
        std::cout << "p99      = " << std::setw(10) << latencies_ns[p99_idx] << " ns  (" 
                  << (latencies_ns[p99_idx] / 1000.0) << " µs)\n";
        std::cout << "p999     = " << std::setw(10) << latencies_ns[p999_idx] << " ns  (" 
                  << (latencies_ns[p999_idx] / 1000.0) << " µs)\n";
        std::cout << "max      = " << std::setw(10) << max_ns << " ns  (" 
                  << (max_ns / 1000.0) << " µs)\n";
        std::cout << "jitter   = " << std::setw(10) << (max_ns - min_ns) << " ns  (" 
                  << ((max_ns - min_ns) / 1000.0) << " µs)\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
        
        std::cout << "✅ CSV replay complete!\n\n";
    }
    
    return 0;
}
EOF

echo "Compiling CSV demo..."

# Use clang++ on macOS
CXX="clang++"
if command -v g++-13 &> /dev/null; then
    CXX="g++-13"
elif command -v g++ &> /dev/null; then
    CXX="g++"
fi

$CXX -std=c++17 -O2 -march=native -pthread -Wall -Wextra \
    -I./include \
    -I/opt/homebrew/include \
    -I/usr/local/include \
    /tmp/csv_demo.cpp -o build/csv_demo

if [[ $? -eq 0 ]]; then
    echo "✅ Build successful!"
    echo ""
    echo "Running CSV replay..."
    echo "════════════════════════════════════════════════════════"
    echo ""
    
    ./build/csv_demo
    
    echo "════════════════════════════════════════════════════════"
    echo "✅ CSV demo complete!"
    echo ""
    echo "Your production code in src/main.cpp is unchanged."
    echo ""
else
    echo "❌ Build failed!"
    exit 1
fi
