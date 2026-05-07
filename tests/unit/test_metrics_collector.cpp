#include "metrics_collector.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>

// Unit tests for Metrics Collector

void test_basic_metrics_updates() {
    std::cout << "Testing basic metrics updates..." << std::endl;

    MetricsCollector collector;

    // Test initial state
    auto& metrics = collector.get_metrics();
    assert(metrics.current_position.load() == 0);
    assert(metrics.total_pnl.load() == 0.0);
    assert(metrics.orders_sent.load() == 0);

    // Update market data
    collector.update_market_data(100.0, 99.5, 100.5);
    assert(metrics.mid_price.load() == 100.0);
    assert(metrics.bid_price.load() == 99.5);
    assert(metrics.ask_price.load() == 100.5);
    assert(std::abs(metrics.spread_bps.load() - 100.0) < 0.1);  // 1% spread = 100bps

    // Update position and P&L
    collector.update_position(100, 50.0, 25.0);
    assert(metrics.current_position.load() == 100);
    assert(metrics.unrealized_pnl.load() == 50.0);
    assert(metrics.realized_pnl.load() == 25.0);
    assert(metrics.total_pnl.load() == 75.0);

    // Update cycle latency
    collector.update_cycle_latency(15.5);
    assert(metrics.avg_cycle_latency_us.load() == 15.5);
    assert(metrics.max_cycle_latency_us.load() == 15.5);
    assert(metrics.min_cycle_latency_us.load() == 15.5);

    // Update again with different latency
    collector.update_cycle_latency(20.0);
    assert(metrics.avg_cycle_latency_us.load() == 20.0);
    assert(metrics.max_cycle_latency_us.load() == 20.0);
    assert(metrics.min_cycle_latency_us.load() == 15.5);

    std::cout << "Basic metrics updates passed" << std::endl;
}

void test_order_counters() {
    std::cout << "Testing order counters..." << std::endl;

    MetricsCollector collector;
    auto& metrics = collector.get_metrics();

    // Test order counters
    assert(metrics.orders_sent.load() == 0);
    assert(metrics.orders_filled.load() == 0);
    assert(metrics.orders_rejected.load() == 0);

    collector.increment_orders_sent();
    collector.increment_orders_sent();
    assert(metrics.orders_sent.load() == 2);

    collector.increment_orders_filled();
    collector.increment_orders_filled();
    collector.increment_orders_filled();
    assert(metrics.orders_filled.load() == 3);

    collector.increment_orders_rejected();
    assert(metrics.orders_rejected.load() == 1);

    std::cout << "Order counters passed" << std::endl;
}

void test_hawkes_intensity() {
    std::cout << "Testing Hawkes process intensity updates..." << std::endl;

    MetricsCollector collector;
    auto& metrics = collector.get_metrics();

    // Update intensities
    collector.update_hawkes_intensity(2.5, 1.8);
    assert(metrics.buy_intensity.load() == 2.5);
    assert(metrics.sell_intensity.load() == 1.8);
    assert(std::abs(metrics.intensity_imbalance.load() - 0.16) < 0.01);  // (2.5-1.8)/(2.5+1.8)

    // Test equal intensities
    collector.update_hawkes_intensity(2.0, 2.0);
    assert(std::abs(metrics.intensity_imbalance.load()) < 0.001);

    std::cout << "Hawkes intensity updates passed" << std::endl;
}

void test_risk_metrics() {
    std::cout << "Testing risk metrics updates..." << std::endl;

    MetricsCollector collector;
    auto& metrics = collector.get_metrics();

    // Update risk metrics
    collector.update_risk(2, 0.6, 85.5);  // High stress regime
    assert(metrics.current_regime.load() == 2);
    assert(metrics.regime_multiplier.load() == 0.6);
    assert(metrics.position_limit_usage.load() == 85.5);

    std::cout << "Risk metrics updates passed" << std::endl;
}

void test_snapshots() {
    std::cout << "Testing snapshot functionality..." << std::endl;

    MetricsCollector collector(100);  // Small history for testing

    // Set up some metrics
    collector.update_market_data(100.0, 99.5, 100.5);
    collector.update_position(50, 25.0, 10.0);
    collector.update_hawkes_intensity(3.0, 2.0);
    collector.update_risk(1, 0.8, 50.0);
    collector.increment_orders_sent();
    collector.increment_orders_filled();

    // Take snapshot
    collector.take_snapshot();

    // Get recent snapshots
    auto snapshots = collector.get_recent_snapshots(10);
    assert(snapshots.size() == 1);

    const auto& snap = snapshots[0];
    assert(snap.mid_price == 100.0);
    assert(snap.position == 50);
    assert(snap.pnl == 35.0);  // 25 + 10
    assert(snap.orders_sent == 1);
    assert(snap.orders_filled == 1);
    assert(snap.regime == 1);

    std::cout << "Snapshot functionality passed" << std::endl;
}

void test_snapshot_history() {
    std::cout << "Testing snapshot history management..." << std::endl;

    MetricsCollector collector(5);  // Very small history

    // Take multiple snapshots
    for (int i = 0; i < 10; ++i) {
        collector.update_position(i, 0.0, 0.0);
        collector.take_snapshot();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Ensure different timestamps
    }

    // Should only keep last 5
    auto snapshots = collector.get_recent_snapshots(10);
    assert(snapshots.size() == 5);

    // Check that we have the most recent ones
    for (size_t i = 0; i < snapshots.size(); ++i) {
        assert(snapshots[i].position == static_cast<int64_t>(5 + i));
    }

    std::cout << "Snapshot history management passed" << std::endl;
}

void test_csv_export() {
    std::cout << "Testing CSV export functionality..." << std::endl;

    MetricsCollector collector(10);

    // Add some data
    collector.update_market_data(100.0, 99.5, 100.5);
    collector.update_position(100, 50.0, 25.0);
    collector.take_snapshot();

    collector.update_market_data(101.0, 100.5, 101.5);
    collector.update_position(150, 75.0, 30.0);
    collector.take_snapshot();

    // Export to CSV
    const std::string filename = "test_metrics.csv";
    collector.export_to_csv(filename);

    // Verify file exists and has content
    assert(std::filesystem::exists(filename));

    std::ifstream file(filename);
    std::string line;

    // Check header
    std::getline(file, line);
    assert(line == "timestamp_ns,mid_price,spread_bps,pnl,position,buy_intensity,sell_intensity,latency_us,orders_sent,orders_filled,regime,position_limit_usage");

    // Check we have data lines
    int data_lines = 0;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            data_lines++;
        }
    }
    assert(data_lines == 2);

    // Clean up
    std::filesystem::remove(filename);

    std::cout << "CSV export functionality passed" << std::endl;
}

void test_summary_statistics() {
    std::cout << "Testing summary statistics..." << std::endl;

    MetricsCollector collector(10);

    // Add some varied data
    collector.update_market_data(100.0, 99.5, 100.5);
    collector.update_position(0, 0.0, 0.0);
    collector.take_snapshot();

    collector.update_market_data(101.0, 100.5, 101.5);
    collector.update_position(100, 10.0, 5.0);
    collector.increment_orders_sent();
    collector.increment_orders_filled();
    collector.take_snapshot();

    collector.update_market_data(102.0, 101.5, 102.5);
    collector.update_position(50, 25.0, 15.0);
    collector.increment_orders_sent();
    collector.increment_orders_filled();
    collector.take_snapshot();

    auto stats = collector.get_summary();
    assert(stats.total_trades == 2);  // orders_filled
    assert(std::abs(stats.fill_rate - 1.0) < 0.001);  // 2/2 = 100%
    assert(stats.max_pnl == 40.0);  // 25 + 15
    assert(stats.min_pnl == 0.0);   // initial
    assert(std::abs(stats.avg_pnl - (55.0/3.0)) < 0.001);  // (0 + 15 + 40) / 3

    std::cout << "Summary statistics passed" << std::endl;
}

void test_concurrent_access() {
    std::cout << "Testing concurrent access..." << std::endl;

    MetricsCollector collector(1000);

    const int num_threads = 4;
    const int updates_per_thread = 100;

    std::vector<std::thread> threads;

    // Multiple threads updating metrics concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&collector, updates_per_thread, t]() {
            for (int i = 0; i < updates_per_thread; ++i) {
                collector.update_position(t * 100 + i, i * 1.0, i * 0.5);
                collector.increment_orders_sent();
                if (i % 2 == 0) {
                    collector.increment_orders_filled();
                }
                collector.take_snapshot();
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify we have snapshots
    auto snapshots = collector.get_recent_snapshots(1000);
    assert(snapshots.size() > 0);

    // Verify metrics are reasonable
    auto& metrics = collector.get_metrics();
    assert(metrics.orders_sent.load() == num_threads * updates_per_thread);
    assert(metrics.orders_filled.load() == num_threads * (updates_per_thread / 2));

    std::cout << "Concurrent access passed" << std::endl;
}

int main() {
    std::cout << "Running Metrics Collector Unit Tests" << std::endl;
    std::cout << "====================================" << std::endl;

    try {
        test_basic_metrics_updates();
        test_order_counters();
        test_hawkes_intensity();
        test_risk_metrics();
        test_snapshots();
        test_snapshot_history();
        test_csv_export();
        test_summary_statistics();
        test_concurrent_access();

        std::cout << std::endl;
        std::cout << " All metrics collector tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}