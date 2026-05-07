#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "backtesting_engine.hpp"
#include "alpha_yield_engine.hpp"

using namespace hft;

struct BenchStrategy {
    void on_tick(const MarketTick&) {}
    void on_fill(uint64_t, double, uint64_t) {}
};

void run_test_case(const std::string& data_path, uint32_t msg_limit, double jitter_ns, bool fpga) {
    mm::AlphaYieldEngine::Params ay_params;
    ay_params.risk_aversion = 0.5;
    ay_params.rebate_value = 0.002;
    ay_params.intensity_k = 1.0;
    ay_params.ofi_k = 0.5;
    ay_params.inventory_cap = 10000;

    backtest::BacktestEngine<BenchStrategy> engine(data_path, ay_params);
    BenchStrategy strategy;
    
    engine.set_message_rate_limit(msg_limit);
    engine.set_jitter(jitter_ns);
    engine.set_silent(true);
    if (fpga) engine.enable_fpga_mode(true);

    auto r = engine.run(strategy);

    std::cout << "| " << std::setw(8) << msg_limit 
              << " | " << std::setw(6) << (int)jitter_ns 
              << " | " << (fpga ? "FPGA" : "CPU ") << " | "
              << std::setw(7) << std::fixed << std::setprecision(1) << r.pnl << " | "
              << std::setw(7) << r.fills << " | "
              << std::setw(8) << r.aggressive_fills << " | "
              << std::setw(6) << std::fixed << std::setprecision(2) << r.avg_chaos << " | "
              << std::setw(6) << r.governed << " |\n";
}

int main() {
    std::string data_path = "apex_stress.bin";
    std::ofstream ofs(data_path, std::ios::binary);
    double mid = 100.0;
    uint64_t start_ns = 1706263521000000000ULL; // Baseline
    
    for (int i=0; i<100000; ++i) {
        MarketTick t;
        if (i % 100 == 50) {
            mid += 0.005 * std::cos(i * 0.015); 
        }
        
        t.mid_price = mid; t.bid_price = mid-0.01; t.ask_price = mid+0.01;
        t.bid_size = 1000; t.ask_size = 1000;
        
        for(int j=0; j<10; j++) {
            t.bid_prices[j] = t.bid_price - 0.01*j;
            t.ask_prices[j] = t.ask_price + 0.01*j;
            t.bid_sizes[j] = 2000;
            t.ask_sizes[j] = 2000;
        }

        // PROFITABLE FREQUENCY
        t.trade_volume = (i % 10 == 0) ? 2000 : 0; 
        t.trade_side = (rand() % 2 == 0) ? Side::BUY : Side::SELL;
        
        uint64_t ns_val = start_ns + (uint64_t)i * 1000000ULL; 
        auto dur = std::chrono::nanoseconds(ns_val);
        t.timestamp = std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(dur));
        
        ofs.write(reinterpret_cast<const char*>(&t), sizeof(MarketTick));
    }
    ofs.close();

    std::cout << "\n=== APEX 6.5 MULTI-VENUE FLOW (Lead-Lag + SOR) ===\n";
    std::cout << "| Msg/Sec  | Jitter | Mode |  PnL   |  Fills  | AggFills | Chaos  |  Gov   |\n";
    std::cout << "|----------|--------|------|--------|---------|----------|--------|--------|\n";

    // 1. Research Hybrid Baseline
    run_test_case(data_path, 5000, 0, false);
    
    // 2. High Jitter Resilience
    std::cout << "\n=== ARBITRAGE EFFICIENCY (Bad Cable) ===\n";
    run_test_case(data_path, 5000, 500, false); // CPU
    run_test_case(data_path, 5000, 500, true);  // FPGA (Hybrid)

    return 0;
}
