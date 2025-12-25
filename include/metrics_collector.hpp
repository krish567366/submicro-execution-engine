#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP

#include "common_types.hpp"
#include <atomic>
#include <mutex>
#include <deque>
#include <chrono>
#include <fstream>

// Trading metrics structure
struct TradingMetrics {
    // Timestamps
    int64_t timestamp_ns;
    
    // Position & PnL
    std::atomic<int64_t> current_position{0};
    std::atomic<double> unrealized_pnl{0.0};
    std::atomic<double> realized_pnl{0.0};
    std::atomic<double> total_pnl{0.0};
    
    // Market data
    std::atomic<double> mid_price{0.0};
    std::atomic<double> spread_bps{0.0};
    std::atomic<double> bid_price{0.0};
    std::atomic<double> ask_price{0.0};
    
    // Orders
    std::atomic<uint64_t> orders_sent{0};
    std::atomic<uint64_t> orders_filled{0};
    std::atomic<uint64_t> orders_rejected{0};
    std::atomic<uint64_t> orders_cancelled{0};
    
    // Hawkes process
    std::atomic<double> buy_intensity{0.0};
    std::atomic<double> sell_intensity{0.0};
    std::atomic<double> intensity_imbalance{0.0};
    
    // Risk metrics
    std::atomic<double> position_limit_usage{0.0};  // Percentage
    std::atomic<int> current_regime{0};  // 0=NORMAL, 1=ELEVATED, 2=STRESS, 3=HALTED
    std::atomic<double> regime_multiplier{1.0};
    
    // Latency metrics (microseconds)
    std::atomic<double> avg_cycle_latency_us{0.0};
    std::atomic<double> max_cycle_latency_us{0.0};
    std::atomic<double> min_cycle_latency_us{999999.0};
    
    // Queue utilization
    std::atomic<double> market_queue_util{0.0};
    std::atomic<double> order_queue_util{0.0};
    
    // Strategy metrics
    std::atomic<double> inventory_skew{0.0};
    std::atomic<double> reservation_price{0.0};
    std::atomic<double> optimal_spread{0.0};
    
    TradingMetrics() : timestamp_ns(std::chrono::steady_clock::now().time_since_epoch().count()) {}
};

// Time-series data point
struct MetricSnapshot {
    int64_t timestamp_ns;
    double mid_price;
    double spread_bps;
    double pnl;
    int64_t position;
    double buy_intensity;
    double sell_intensity;
    double cycle_latency_us;
    uint64_t orders_sent;
    uint64_t orders_filled;
    int regime;
    double position_limit_usage;
};

// Metrics collector with circular buffer
class MetricsCollector {
public:
    explicit MetricsCollector(size_t history_size = 10000)
        : history_size_(history_size),
          metrics_(),
          running_(true) {
    }
     
    ~MetricsCollector() {
        running_.store(false, std::memory_order_release);
    }
    
    // Get current metrics (lock-free read)
    TradingMetrics& get_metrics() {
        return metrics_;
    }
    
    // Take snapshot for time-series
    void take_snapshot() {
        MetricSnapshot snap;
        snap.timestamp_ns = std::chrono::steady_clock::now().time_since_epoch().count();
        snap.mid_price = metrics_.mid_price.load(std::memory_order_acquire);
        snap.spread_bps = metrics_.spread_bps.load(std::memory_order_acquire);
        snap.pnl = metrics_.total_pnl.load(std::memory_order_acquire);
        snap.position = metrics_.current_position.load(std::memory_order_acquire);
        snap.buy_intensity = metrics_.buy_intensity.load(std::memory_order_acquire);
        snap.sell_intensity = metrics_.sell_intensity.load(std::memory_order_acquire);
        snap.cycle_latency_us = metrics_.avg_cycle_latency_us.load(std::memory_order_acquire);
        snap.orders_sent = metrics_.orders_sent.load(std::memory_order_acquire);
        snap.orders_filled = metrics_.orders_filled.load(std::memory_order_acquire);
        snap.regime = metrics_.current_regime.load(std::memory_order_acquire);
        snap.position_limit_usage = metrics_.position_limit_usage.load(std::memory_order_acquire);
        
        std::lock_guard<std::mutex> lock(snapshots_mutex_);
        snapshots_.push_back(snap);
        
        // Keep only recent history
        if (snapshots_.size() > history_size_) {
            snapshots_.pop_front();
        }
    }
    
    // Get recent snapshots for charting
    std::vector<MetricSnapshot> get_recent_snapshots(size_t count = 1000) {
        std::lock_guard<std::mutex> lock(snapshots_mutex_);
        
        size_t start = (snapshots_.size() > count) ? (snapshots_.size() - count) : 0;
        return std::vector<MetricSnapshot>(
            snapshots_.begin() + start,
            snapshots_.end()
        );
    }
    
    // Export to CSV
    void export_to_csv(const std::string& filename) {
        std::lock_guard<std::mutex> lock(snapshots_mutex_);
        std::ofstream file(filename);
        
        // Header
        file << "timestamp_ns,mid_price,spread_bps,pnl,position,"
             << "buy_intensity,sell_intensity,latency_us,orders_sent,"
             << "orders_filled,regime,position_limit_usage\n";
        
        // Data
        for (const auto& snap : snapshots_) {
            file << snap.timestamp_ns << ","
                 << snap.mid_price << ","
                 << snap.spread_bps << ","
                 << snap.pnl << ","
                 << snap.position << ","
                 << snap.buy_intensity << ","
                 << snap.sell_intensity << ","
                 << snap.cycle_latency_us << ","
                 << snap.orders_sent << ","
                 << snap.orders_filled << ","
                 << snap.regime << ","
                 << snap.position_limit_usage << "\n";
        }
    }
    
    // Get summary statistics
    struct SummaryStats {
        double avg_pnl;
        double max_pnl;
        double min_pnl;
        double sharpe_ratio;
        double max_drawdown;
        double avg_latency_us;
        double max_latency_us;
        uint64_t total_trades;
        double fill_rate;
    };
    
    SummaryStats get_summary() {
        std::lock_guard<std::mutex> lock(snapshots_mutex_);
        
        SummaryStats stats{};
        if (snapshots_.empty()) return stats;
        
        double sum_pnl = 0.0;
        double max_pnl = -1e9;
        double min_pnl = 1e9;
        double sum_latency = 0.0;
        double max_latency = 0.0;
        
        for (const auto& snap : snapshots_) {
            sum_pnl += snap.pnl;
            max_pnl = std::max(max_pnl, snap.pnl);
            min_pnl = std::min(min_pnl, snap.pnl);
            sum_latency += snap.cycle_latency_us;
            max_latency = std::max(max_latency, snap.cycle_latency_us);
        }
        
        stats.avg_pnl = sum_pnl / snapshots_.size();
        stats.max_pnl = max_pnl;
        stats.min_pnl = min_pnl;
        stats.avg_latency_us = sum_latency / snapshots_.size();
        stats.max_latency_us = max_latency;
        
        auto last_snap = snapshots_.back();
        stats.total_trades = last_snap.orders_filled;
        stats.fill_rate = (last_snap.orders_sent > 0) 
            ? (double)last_snap.orders_filled / last_snap.orders_sent 
            : 0.0;
        
        return stats;
    }
    
    // Update metrics (called from trading loop)
    void update_cycle_latency(double latency_us) {
        metrics_.avg_cycle_latency_us.store(latency_us, std::memory_order_release);
        
        double current_max = metrics_.max_cycle_latency_us.load(std::memory_order_acquire);
        if (latency_us > current_max) {
            metrics_.max_cycle_latency_us.store(latency_us, std::memory_order_release);
        }
        
        double current_min = metrics_.min_cycle_latency_us.load(std::memory_order_acquire);
        if (latency_us < current_min) {
            metrics_.min_cycle_latency_us.store(latency_us, std::memory_order_release);
        }
    }
    
    void update_market_data(double mid, double bid, double ask) {
        metrics_.mid_price.store(mid, std::memory_order_release);
        metrics_.bid_price.store(bid, std::memory_order_release);
        metrics_.ask_price.store(ask, std::memory_order_release);
        
        double spread = ((ask - bid) / mid) * 10000.0;  // bps
        metrics_.spread_bps.store(spread, std::memory_order_release);
    }
    
    void update_position(int64_t position, double unrealized_pnl, double realized_pnl) {
        metrics_.current_position.store(position, std::memory_order_release);
        metrics_.unrealized_pnl.store(unrealized_pnl, std::memory_order_release);
        metrics_.realized_pnl.store(realized_pnl, std::memory_order_release);
        metrics_.total_pnl.store(unrealized_pnl + realized_pnl, std::memory_order_release);
    }
    
    void increment_orders_sent() {
        metrics_.orders_sent.fetch_add(1, std::memory_order_acq_rel);
    }
    
    void increment_orders_filled() {
        metrics_.orders_filled.fetch_add(1, std::memory_order_acq_rel);
    }
    
    void increment_orders_rejected() {
        metrics_.orders_rejected.fetch_add(1, std::memory_order_acq_rel);
    }
    
    void update_hawkes_intensity(double buy, double sell) {
        metrics_.buy_intensity.store(buy, std::memory_order_release);
        metrics_.sell_intensity.store(sell, std::memory_order_release);
        metrics_.intensity_imbalance.store(
            (buy - sell) / (buy + sell + 1e-10),
            std::memory_order_release
        );
    }
    
    void update_risk(int regime, double multiplier, double position_usage) {
        metrics_.current_regime.store(regime, std::memory_order_release);
        metrics_.regime_multiplier.store(multiplier, std::memory_order_release);
        metrics_.position_limit_usage.store(position_usage, std::memory_order_release);
    }
    
private:
    size_t history_size_;
    TradingMetrics metrics_;
    std::atomic<bool> running_;
    
    std::deque<MetricSnapshot> snapshots_;
    std::mutex snapshots_mutex_;
};

#endif // METRICS_COLLECTOR_HPP
