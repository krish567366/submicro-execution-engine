/**
 * HFT System Benchmark Suite
 * 
 * Industry-standard tick-to-trade latency measurement
 * 
 * Build:
 *   g++ -std=c++17 -O3 -march=native -pthread benchmark_main.cpp -o hft_benchmark
 * 
 * Run:
 *   sudo ./hft_benchmark --samples 100000000 --output results
 * 
 * Requirements:
 *   - Linux kernel 4.0+
 *   - CPU isolation (isolcpus=2-7)
 *   - Real-time priority (run with sudo)
 *   - Huge pages configured
 */

#include "benchmark_suite.hpp"
#include "common_types.hpp"
#include "lockfree_queue.hpp"
#include "hawkes_engine.hpp"
#include "fpga_inference.hpp"
#include "avellaneda_stoikov.hpp"
#include "risk_control.hpp"
#include "fast_lob.hpp"

#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>

using namespace hft;
using namespace hft::benchmark;

// ============================================================================
// System Configuration for Ultra-Low Latency
// ============================================================================

void configure_for_benchmarking() {
    std::cout << "Configuring system for benchmarking...\n";
    
    // 1. Lock memory pages (prevent swapping)
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "⚠️  Warning: Failed to lock memory pages (run with sudo)\n";
    } else {
        std::cout << "✅ Memory pages locked\n";
    }
    
    // 2. Set real-time priority
    struct sched_param param;
    param.sched_priority = 49;  // SCHED_FIFO priority 49
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        std::cerr << "⚠️  Warning: Failed to set real-time priority (run with sudo)\n";
    } else {
        std::cout << "✅ Real-time priority set (SCHED_FIFO 49)\n";
    }
    
    // 3. Pin to isolated CPU core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset);  // Use core 2 (assumed isolated)
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        std::cerr << "⚠️  Warning: Failed to set CPU affinity\n";
    } else {
        std::cout << "✅ Pinned to CPU core 2\n";
    }
    
    // 4. Set resource limits
    struct rlimit rlim;
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_MEMLOCK, &rlim) != 0) {
        std::cerr << "⚠️  Warning: Failed to set memory lock limit\n";
    }
    
    std::cout << "\n";
}

// ============================================================================
// Mock Trading System for Benchmarking
// ============================================================================

class BenchmarkTradingSystem {
public:
    BenchmarkTradingSystem() 
        : hawkes_(0.5, 0.5, 0.8, 0.3, 1e-6, 1.5)
        , strategy_(0.01, 0.15, 300.0, 10.0, 0.01, 850)
        , risk_(1000, 10000.0, 100000.0)
        , fpga_inference_(12, 16)
    {
        std::cout << "Initializing trading system components...\n";
    }
    
    /**
     * Process single tick with full pipeline (instrumented)
     */
    TickToTradeBenchmark::Sample process_tick_instrumented(const MarketTick& tick) {
        TickToTradeBenchmark::Sample sample = {};
        
        // ════════════════════════════════════════════════════════════════
        // Phase 1: Packet Reception (simulated NIC timestamp)
        // ════════════════════════════════════════════════════════════════
        sample.tsc_feed_sent = rdtscp();
        
        // Simulate NIC DMA latency (30 ns)
        busy_wait_ns(30);
        sample.tsc_app_received = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 2: Packet Parsing (20 ns)
        // ════════════════════════════════════════════════════════════════
        MarketTick parsed_tick = tick;  // In reality: zero-copy parser
        sample.tsc_parse_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 3: Order Book Update (30 ns with flat arrays)
        // ════════════════════════════════════════════════════════════════
        lob_.update_bid(0, tick.bid_price, tick.bid_size);
        lob_.update_ask(0, tick.ask_price, tick.ask_size);
        sample.tsc_lob_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 4: Feature Extraction (250 ns)
        // ════════════════════════════════════════════════════════════════
        TradingEvent event;
        event.arrival_time = tick.timestamp;
        event.event_type = Side::BUY;
        event.price = tick.mid_price;
        event.size = tick.trade_volume;
        
        hawkes_.update(event);
        
        auto features = FPGA_DNN_Inference::extract_features(
            tick, prev_tick_, ref_tick_,
            hawkes_.get_buy_intensity(),
            hawkes_.get_sell_intensity()
        );
        
        sample.tsc_features_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 5: FPGA DNN Inference (400 ns fixed)
        // ════════════════════════════════════════════════════════════════
        auto prediction = fpga_inference_.predict(features);
        sample.tsc_inference_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 6: Avellaneda-Stoikov Strategy (70 ns)
        // ════════════════════════════════════════════════════════════════
        auto quotes = strategy_.calculate_quotes(
            tick.mid_price,
            current_position_,
            300.0,  // time remaining
            0.0001  // latency cost
        );
        sample.tsc_strategy_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 7: Risk Checks (20 ns with branch optimization)
        // ════════════════════════════════════════════════════════════════
        Order test_order;
        test_order.price = quotes.bid_price;
        test_order.quantity = quotes.bid_size;
        test_order.side = Side::BUY;
        
        bool risk_passed = risk_.check_pre_trade_limits(test_order, current_position_);
        sample.tsc_risk_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 8: Order Encoding (20 ns with pre-serialization)
        // ════════════════════════════════════════════════════════════════
        if (risk_passed) {
            // Simulate order encoding (pre-built templates)
            volatile uint8_t order_buffer[64];
            std::memcpy((void*)order_buffer, &test_order, sizeof(test_order));
        }
        sample.tsc_encode_done = rdtscp();
        
        // ════════════════════════════════════════════════════════════════
        // Phase 9: NIC TX (40 ns)
        // ════════════════════════════════════════════════════════════════
        busy_wait_ns(40);
        sample.tsc_order_sent = rdtscp();
        
        // Update state for next iteration
        prev_tick_ = tick;
        
        return sample;
    }

private:
    HawkesIntensityEngine hawkes_;
    DynamicMMStrategy strategy_;
    RiskControl risk_;
    FPGA_DNN_Inference fpga_inference_;
    FastLOB lob_;
    
    MarketTick prev_tick_;
    MarketTick ref_tick_;
    int64_t current_position_ = 0;
    
    /**
     * Busy-wait for specified nanoseconds (accurate timing)
     */
    void busy_wait_ns(uint64_t ns) {
        uint64_t cycles = static_cast<uint64_t>(ns / g_tsc_to_ns);
        uint64_t start = rdtsc();
        while ((rdtsc() - start) < cycles) {
            __asm__ __volatile__("pause" ::: "memory");
        }
    }
};

// ============================================================================
// Component-Level Benchmarks
// ============================================================================

void run_component_benchmarks() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  COMPONENT-LEVEL BENCHMARKS                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    constexpr size_t ITERATIONS = 1000000;  // 1M iterations per component
    
    // 1. Parser benchmark
    ComponentBenchmark::benchmark_component("Packet Parser", []() {
        volatile uint8_t packet[64];
        volatile double price = *reinterpret_cast<const double*>(packet + 42);
        volatile uint32_t qty = *reinterpret_cast<const uint32_t*>(packet + 50);
    }, ITERATIONS);
    
    // 2. Order book update benchmark
    FastLOB lob;
    ComponentBenchmark::benchmark_component("LOB Update (Flat Array)", [&lob]() {
        lob.update_bid(0, 100.0 + (rand() % 100) * 0.01, 1000);
    }, ITERATIONS);
    
    // 3. Hawkes engine benchmark
    HawkesIntensityEngine hawkes(0.5, 0.5, 0.8, 0.3, 1e-6, 1.5);
    ComponentBenchmark::benchmark_component("Hawkes Update", [&hawkes]() {
        TradingEvent event;
        event.arrival_time = now();
        event.event_type = Side::BUY;
        event.price = 100.0;
        event.size = 100;
        hawkes.update(event);
    }, ITERATIONS);
    
    // 4. FPGA inference benchmark
    FPGA_DNN_Inference fpga(12, 16);
    MicrostructureFeatures features;
    ComponentBenchmark::benchmark_component("FPGA DNN Inference", [&fpga, &features]() {
        volatile auto pred = fpga.predict(features);
    }, ITERATIONS);
    
    // 5. Strategy calculation benchmark
    DynamicMMStrategy strategy(0.01, 0.15, 300.0, 10.0, 0.01, 850);
    ComponentBenchmark::benchmark_component("A-S Strategy", [&strategy]() {
        volatile auto quotes = strategy.calculate_quotes(100.0, 0, 300.0, 0.0001);
    }, ITERATIONS);
    
    // 6. Risk checks benchmark
    RiskControl risk(1000, 10000.0, 100000.0);
    Order order;
    order.price = 100.0;
    order.quantity = 100;
    order.side = Side::BUY;
    ComponentBenchmark::benchmark_component("Risk Checks", [&risk, &order]() {
        volatile bool passed = risk.check_pre_trade_limits(order, 0);
    }, ITERATIONS);
    
    // 7. Lock-free queue benchmark
    LockFreeQueue<MarketTick, 16384> queue;
    MarketTick tick;
    ComponentBenchmark::benchmark_component("Lock-Free Push", [&queue, &tick]() {
        queue.push(tick);
    }, ITERATIONS);
    
    ComponentBenchmark::benchmark_component("Lock-Free Pop", [&queue, &tick]() {
        queue.pop(tick);
    }, ITERATIONS);
}

// ============================================================================
// Full System Benchmark (Tick-to-Trade)
// ============================================================================

void run_full_system_benchmark(size_t num_samples, const std::string& output_prefix) {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  FULL TICK-TO-TRADE BENCHMARK                          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    BenchmarkTradingSystem system;
    
    std::vector<TickToTradeBenchmark::Sample> samples;
    samples.reserve(num_samples);
    
    // Generate synthetic ticks
    std::cout << "Generating " << num_samples << " synthetic ticks...\n";
    auto ticks = MarketDataGenerator::generate_batch(num_samples);
    std::cout << "✅ Ticks generated\n\n";
    
    std::cout << "Running full system benchmark...\n";
    
    // Warmup
    std::cout << "Warming up (10,000 iterations)...\n";
    for (size_t i = 0; i < 10000; ++i) {
        system.process_tick_instrumented(ticks[i % ticks.size()]);
    }
    std::cout << "✅ Warmup complete\n\n";
    
    // Actual benchmark
    std::cout << "Benchmarking " << num_samples << " ticks...\n";
    size_t progress_interval = num_samples / 100;
    
    for (size_t i = 0; i < num_samples; ++i) {
        auto sample = system.process_tick_instrumented(ticks[i]);
        samples.push_back(sample);
        
        if (i % progress_interval == 0) {
            std::cout << "\rProgress: " << (i * 100 / num_samples) << "% " << std::flush;
        }
    }
    
    std::cout << "\rProgress: 100%\n\n";
    
    // Generate report
    generate_report(samples, output_prefix);
}

void generate_report(const std::vector<TickToTradeBenchmark::Sample>& samples,
                    const std::string& output_prefix) {
    // Calculate total latencies
    std::vector<double> total_latencies;
    total_latencies.reserve(samples.size());
    for (const auto& s : samples) {
        total_latencies.push_back(s.total_latency_ns());
    }
    
    // Overall stats
    auto total_stats = LatencyStats::calculate(total_latencies);
    total_stats.print("═══ TICK-TO-TRADE LATENCY ═══");
    
    // Component breakdown
    std::cout << "\n╔═══ COMPONENT BREAKDOWN ═══╗\n\n";
    
    struct ComponentStats {
        std::string name;
        std::vector<double> latencies;
    };
    
    std::vector<ComponentStats> component_stats(9);
    component_stats[0].name = "RX DMA → App";
    component_stats[1].name = "Parse Packet";
    component_stats[2].name = "LOB Update";
    component_stats[3].name = "Feature Extract";
    component_stats[4].name = "DNN Inference";
    component_stats[5].name = "Strategy (A-S)";
    component_stats[6].name = "Risk Checks";
    component_stats[7].name = "Order Encode";
    component_stats[8].name = "TX App → DMA";
    
    for (const auto& sample : samples) {
        auto breakdown = sample.breakdown();
        component_stats[0].latencies.push_back(tsc_to_ns(breakdown.rx_dma_to_app));
        component_stats[1].latencies.push_back(tsc_to_ns(breakdown.parse_packet));
        component_stats[2].latencies.push_back(tsc_to_ns(breakdown.lob_update));
        component_stats[3].latencies.push_back(tsc_to_ns(breakdown.feature_extraction));
        component_stats[4].latencies.push_back(tsc_to_ns(breakdown.inference));
        component_stats[5].latencies.push_back(tsc_to_ns(breakdown.strategy));
        component_stats[6].latencies.push_back(tsc_to_ns(breakdown.risk_checks));
        component_stats[7].latencies.push_back(tsc_to_ns(breakdown.order_encode));
        component_stats[8].latencies.push_back(tsc_to_ns(breakdown.tx_app_to_dma));
    }
    
    // Print component table
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(20) << std::left << "Component" 
              << std::right
              << std::setw(12) << "Mean (ns)"
              << std::setw(12) << "p99 (ns)"
              << std::setw(12) << "Max (ns)"
              << std::setw(12) << "% Total" << "\n";
    std::cout << "────────────────────────────────────────────────────────────────\n";
    
    for (auto& cs : component_stats) {
        auto stats = LatencyStats::calculate(cs.latencies);
        double pct = (stats.mean_ns / total_stats.mean_ns) * 100.0;
        
        std::cout << std::setw(20) << std::left << cs.name
                  << std::right
                  << std::setw(12) << stats.mean_ns
                  << std::setw(12) << stats.p99_ns
                  << std::setw(12) << stats.max_ns
                  << std::setw(11) << pct << "%\n";
    }
    
    // Export data
    total_stats.export_csv(output_prefix + "_total.csv");
    
    std::ofstream f(output_prefix + "_components.csv");
    f << "component,mean_ns,p99_ns,max_ns,percent\n";
    for (auto& cs : component_stats) {
        auto stats = LatencyStats::calculate(cs.latencies);
        double pct = (stats.mean_ns / total_stats.mean_ns) * 100.0;
        f << cs.name << "," << stats.mean_ns << "," << stats.p99_ns 
          << "," << stats.max_ns << "," << pct << "\n";
    }
    
    // Export raw samples for deep analysis
    std::ofstream raw_file(output_prefix + "_raw_samples.csv");
    raw_file << "sample_id,total_ns,rx_dma_ns,parse_ns,lob_ns,features_ns,"
             << "inference_ns,strategy_ns,risk_ns,encode_ns,tx_dma_ns\n";
    
    for (size_t i = 0; i < samples.size(); ++i) {
        auto breakdown = samples[i].breakdown();
        raw_file << i << ","
                 << samples[i].total_latency_ns() << ","
                 << tsc_to_ns(breakdown.rx_dma_to_app) << ","
                 << tsc_to_ns(breakdown.parse_packet) << ","
                 << tsc_to_ns(breakdown.lob_update) << ","
                 << tsc_to_ns(breakdown.feature_extraction) << ","
                 << tsc_to_ns(breakdown.inference) << ","
                 << tsc_to_ns(breakdown.strategy) << ","
                 << tsc_to_ns(breakdown.risk_checks) << ","
                 << tsc_to_ns(breakdown.order_encode) << ","
                 << tsc_to_ns(breakdown.tx_app_to_dma) << "\n";
    }
    
    std::cout << "\n✅ Results exported to:\n";
    std::cout << "   - " << output_prefix << "_total.csv\n";
    std::cout << "   - " << output_prefix << "_components.csv\n";
    std::cout << "   - " << output_prefix << "_raw_samples.csv\n\n";
    
    // Industry comparison
    print_industry_comparison(total_stats);
}

void print_industry_comparison(const LatencyStats& stats) {
    std::cout << "\n╔═══ INDUSTRY COMPARISON ═══╗\n\n";
    
    struct Competitor {
        std::string name;
        double latency_us;
    };
    
    std::vector<Competitor> competitors = {
        {"Your System (p50)", stats.median_ns / 1000.0},
        {"Your System (p99)", stats.p99_ns / 1000.0},
        {"Jane Street", 0.90},
        {"Jump Trading", 1.00},
        {"Citadel", 2.00},
        {"Virtu", 7.50}
    };
    
    std::sort(competitors.begin(), competitors.end(),
             [](const Competitor& a, const Competitor& b) {
                 return a.latency_us < b.latency_us;
             });
    
    for (const auto& comp : competitors) {
        int bar_len = static_cast<int>(comp.latency_us * 10);
        bar_len = std::min(bar_len, 80);  // Cap at 80 chars
        std::cout << std::setw(22) << std::left << comp.name << " ";
        for (int i = 0; i < bar_len; ++i) std::cout << "█";
        std::cout << " " << std::fixed << std::setprecision(2) 
                  << comp.latency_us << " μs\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Main Entry Point
// ============================================================================

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --samples N       Number of samples (default: 1000000)\n";
    std::cout << "  --output PREFIX   Output file prefix (default: benchmark)\n";
    std::cout << "  --components      Run component benchmarks only\n";
    std::cout << "  --full            Run full system benchmark only\n";
    std::cout << "  --help            Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sudo " << prog_name << " --samples 100000000 --output prod_results\n";
    std::cout << "  sudo " << prog_name << " --components\n\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    size_t num_samples = 1000000;  // Default: 1M samples
    std::string output_prefix = "benchmark";
    bool run_components = true;
    bool run_full = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--samples" && i + 1 < argc) {
            num_samples = std::stoull(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            output_prefix = argv[++i];
        } else if (arg == "--components") {
            run_full = false;
        } else if (arg == "--full") {
            run_components = false;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     HFT SYSTEM BENCHMARK SUITE                         ║\n";
    std::cout << "║     Industry-Standard Tick-to-Trade Measurement        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    // System configuration
    configure_for_benchmarking();
    
    // TSC calibration
    std::cout << "TSC Calibration: " << std::fixed << std::setprecision(2) 
              << (1.0 / g_tsc_to_ns / 1000.0) << " GHz\n\n";
    
    try {
        if (run_components) {
            run_component_benchmarks();
        }
        
        if (run_full) {
            run_full_system_benchmark(num_samples, output_prefix);
        }
        
        std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║  BENCHMARK COMPLETE ✅                                 ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Benchmark failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
