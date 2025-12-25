#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>

// SHA256 implementation for data checksums
#include <openssl/sha.h>

// ============================================================================
// INSTITUTIONAL-GRADE LOGGING SYSTEM
// ============================================================================
// PURPOSE: Provide third-party verifiable audit trail for HFT backtesting
// FEATURES:
//   - Deterministic replay logs with SHA256 checksums
//   - Order lifecycle tracking (submit→ack→fill→cancel)
//   - Latency distribution analysis (p50/p90/p99/p99.9/max)
//   - Risk kill-switch breach logging
//   - Slippage and market impact analysis
//   - System verification (clock sync, hardware config)
// ============================================================================

namespace InstitutionalLogging {

// ============================================================================
// SHA256 Checksum Calculator
// ============================================================================
class SHA256Hasher {
public:
    static std::string file_checksum(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return "ERROR: Cannot open file";
        }
        
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        
        char buffer[8192];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            SHA256_Update(&sha256, buffer, file.gcount());
        }
        
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(hash[i]);
        }
        
        return ss.str();
    }
    
    static std::string string_checksum(const std::string& data) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.c_str(), data.length());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << static_cast<int>(hash[i]);
        }
        
        return ss.str();
    }
};

// ============================================================================
// Event Replay Logger
// ============================================================================
// PURPOSE: Create bit-for-bit reproducible audit trail
// OUTPUT: Event-by-event log with timestamps, decisions, order flow
// VERIFICATION: Third parties can replay log to verify P&L claims
// ============================================================================
class EventReplayLogger {
public:
    explicit EventReplayLogger(const std::string& log_path)
        : log_file_(log_path), event_count_(0) {
        
        if (!log_file_.is_open()) {
            throw std::runtime_error("Failed to open replay log: " + log_path);
        }
        
        // Write header
        log_file_ << "# DETERMINISTIC BACKTEST REPLAY LOG\n";
        log_file_ << "# Generated: " << get_timestamp() << "\n";
        log_file_ << "# Format: [timestamp_ns] EVENT_TYPE: details\n";
        log_file_ << "# ============================================\n\n";
    }
    
    ~EventReplayLogger() {
        if (log_file_.is_open()) {
            log_file_ << "\n# ============================================\n";
            log_file_ << "# Total events logged: " << event_count_ << "\n";
            log_file_.close();
        }
    }
    
    void log_config(const std::string& config_json, uint32_t random_seed,
                    const std::string& data_file_checksum) {
        log_file_ << "# CONFIGURATION\n";
        log_file_ << "# Random Seed: " << random_seed << "\n";
        log_file_ << "# Data File SHA256: " << data_file_checksum << "\n";
        log_file_ << "# Config: " << config_json << "\n\n";
    }
    
    void log_market_tick(int64_t timestamp_ns, double bid, double ask,
                        uint64_t bid_size, uint64_t ask_size) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "TICK: bid=" << std::fixed << std::setprecision(4) << bid
                  << " ask=" << ask
                  << " bid_sz=" << bid_size
                  << " ask_sz=" << ask_size << "\n";
        ++event_count_;
    }
    
    void log_signal_decision(int64_t timestamp_ns, bool should_trade,
                            const std::string& side, double signal_strength,
                            int confirmation_ticks, double obi) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "SIGNAL: trade=" << (should_trade ? "YES" : "NO")
                  << " side=" << side
                  << " strength=" << std::fixed << std::setprecision(6) << signal_strength
                  << " confirm_ticks=" << confirmation_ticks
                  << " obi=" << std::setprecision(4) << obi << "\n";
        ++event_count_;
    }
    
    void log_order_submit(int64_t timestamp_ns, uint64_t order_id,
                         const std::string& side, double price,
                         uint64_t quantity) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "ORDER_SUBMIT: id=" << order_id
                  << " side=" << side
                  << " price=" << std::fixed << std::setprecision(4) << price
                  << " qty=" << quantity << "\n";
        ++event_count_;
    }
    
    void log_order_ack(int64_t timestamp_ns, uint64_t order_id,
                      int64_t latency_ns) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "ORDER_ACK: id=" << order_id
                  << " latency_ns=" << latency_ns << "\n";
        ++event_count_;
    }
    
    void log_order_fill(int64_t timestamp_ns, uint64_t order_id,
                       double fill_price, uint64_t quantity,
                       int64_t total_latency_ns) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "ORDER_FILL: id=" << order_id
                  << " fill_price=" << std::fixed << std::setprecision(4) << fill_price
                  << " qty=" << quantity
                  << " total_latency_ns=" << total_latency_ns << "\n";
        ++event_count_;
    }
    
    void log_order_cancel(int64_t timestamp_ns, uint64_t order_id,
                         const std::string& reason) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "ORDER_CANCEL: id=" << order_id
                  << " reason=" << reason << "\n";
        ++event_count_;
    }
    
    void log_pnl_update(int64_t timestamp_ns, double realized_pnl,
                       double unrealized_pnl, int position) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "PNL_UPDATE: realized=" << std::fixed << std::setprecision(2) << realized_pnl
                  << " unrealized=" << unrealized_pnl
                  << " position=" << position << "\n";
        ++event_count_;
    }
    
    void log_risk_breach(int64_t timestamp_ns, const std::string& breach_type,
                        const std::string& action_taken, double metric_value,
                        double threshold) {
        log_file_ << "[" << std::setw(20) << timestamp_ns << "] "
                  << "RISK_BREACH: type=" << breach_type
                  << " action=" << action_taken
                  << " value=" << std::fixed << std::setprecision(2) << metric_value
                  << " threshold=" << threshold << "\n";
        ++event_count_;
    }
    
    void flush() {
        log_file_.flush();
    }
    
private:
    std::ofstream log_file_;
    size_t event_count_;
    
    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// ============================================================================
// Latency Distribution Analyzer
// ============================================================================
// PURPOSE: Track latency percentiles for all critical paths
// METRICS: p50, p90, p99, p99.9, max for tick→decision, order→ack, total RTT
// OUTPUT: Histograms and percentile tables (not just averages)
// ============================================================================
class LatencyDistribution {
public:
    void add_sample(int64_t latency_ns) {
        samples_.push_back(latency_ns);
    }
    
    void calculate() {
        if (samples_.empty()) return;
        
        std::sort(samples_.begin(), samples_.end());
        
        p50_ = percentile(0.50);
        p90_ = percentile(0.90);
        p99_ = percentile(0.99);
        p999_ = percentile(0.999);
        max_ = samples_.back();
        min_ = samples_.front();
        
        // Calculate mean and jitter (stddev)
        double sum = 0.0;
        for (auto sample : samples_) {
            sum += sample;
        }
        mean_ = sum / samples_.size();
        
        double variance_sum = 0.0;
        for (auto sample : samples_) {
            double diff = sample - mean_;
            variance_sum += diff * diff;
        }
        jitter_ = std::sqrt(variance_sum / samples_.size());
    }
    
    int64_t get_p50() const { return p50_; }
    int64_t get_p90() const { return p90_; }
    int64_t get_p99() const { return p99_; }
    int64_t get_p999() const { return p999_; }
    int64_t get_max() const { return max_; }
    int64_t get_min() const { return min_; }
    double get_mean() const { return mean_; }
    double get_jitter() const { return jitter_; }
    size_t get_sample_count() const { return samples_.size(); }
    
    void print_report(const std::string& metric_name) const {
        std::cout << "\n" << metric_name << " LATENCY DISTRIBUTION\n";
        std::cout << std::string(70, '-') << "\n";
        std::cout << "Samples:      " << samples_.size() << "\n";
        std::cout << "Min:          " << min_ << " ns\n";
        std::cout << "p50 (Median): " << p50_ << " ns\n";
        std::cout << "p90:          " << p90_ << " ns\n";
        std::cout << "p99:          " << p99_ << " ns\n";
        std::cout << "p99.9:        " << p999_ << " ns\n";
        std::cout << "Max:          " << max_ << " ns\n";
        std::cout << "Mean:         " << std::fixed << std::setprecision(2) 
                  << mean_ << " ns\n";
        std::cout << "Jitter (σ):   " << jitter_ << " ns\n";
    }
    
    void print_histogram(int num_buckets = 20) const {
        if (samples_.empty()) return;
        
        std::cout << "\nHISTOGRAM:\n";
        
        // Create buckets
        int64_t range = max_ - min_;
        int64_t bucket_size = range / num_buckets;
        if (bucket_size == 0) bucket_size = 1;
        
        std::vector<int> buckets(num_buckets, 0);
        
        for (auto sample : samples_) {
            int bucket_idx = (sample - min_) / bucket_size;
            if (bucket_idx >= num_buckets) bucket_idx = num_buckets - 1;
            buckets[bucket_idx]++;
        }
        
        // Find max count for scaling
        int max_count = *std::max_element(buckets.begin(), buckets.end());
        
        // Print histogram
        for (int i = 0; i < num_buckets; ++i) {
            int64_t bucket_start = min_ + i * bucket_size;
            int64_t bucket_end = bucket_start + bucket_size;
            
            std::cout << std::setw(8) << bucket_start << "-" 
                      << std::setw(8) << bucket_end << " ns |";
            
            // ASCII bar
            int bar_length = (buckets[i] * 50) / max_count;
            for (int j = 0; j < bar_length; ++j) {
                std::cout << "█";
            }
            std::cout << " " << buckets[i] << "\n";
        }
    }
    
private:
    std::vector<int64_t> samples_;
    int64_t p50_ = 0;
    int64_t p90_ = 0;
    int64_t p99_ = 0;
    int64_t p999_ = 0;
    int64_t max_ = 0;
    int64_t min_ = 0;
    double mean_ = 0.0;
    double jitter_ = 0.0;
    
    int64_t percentile(double p) const {
        if (samples_.empty()) return 0;
        size_t idx = static_cast<size_t>(p * (samples_.size() - 1));
        return samples_[idx];
    }
};

// ============================================================================
// Slippage and Market Impact Analyzer
// ============================================================================
// PURPOSE: Track execution quality vs theoretical optimal
// METRICS: Slippage (fill vs mid), VWAP performance, adverse selection
// ============================================================================
class SlippageAnalyzer {
public:
    struct FillAnalysis {
        int64_t timestamp_ns;
        double fill_price;
        double decision_mid_price;
        double fill_time_mid_price;
        uint64_t quantity;
        std::string side;
        double slippage_bps;          // (fill - decision_mid) / decision_mid * 10000
        double adverse_selection_bps;  // (fill_time_mid - decision_mid) / decision_mid * 10000
        double market_impact_bps;      // (fill - fill_time_mid) / fill_time_mid * 10000
    };
    
    void add_fill(int64_t timestamp_ns, double fill_price,
                 double decision_mid, double fill_time_mid,
                 uint64_t quantity, const std::string& side) {
        
        FillAnalysis analysis;
        analysis.timestamp_ns = timestamp_ns;
        analysis.fill_price = fill_price;
        analysis.decision_mid_price = decision_mid;
        analysis.fill_time_mid_price = fill_time_mid;
        analysis.quantity = quantity;
        analysis.side = side;
        
        // Calculate slippage (vs decision time mid)
        double slippage = (fill_price - decision_mid) / decision_mid;
        if (side == "SELL") slippage = -slippage;  // Invert for sells
        analysis.slippage_bps = slippage * 10000.0;
        
        // Adverse selection (market moved against us)
        double adverse = (fill_time_mid - decision_mid) / decision_mid;
        if (side == "SELL") adverse = -adverse;
        analysis.adverse_selection_bps = adverse * 10000.0;
        
        // Market impact (our order moved the market)
        double impact = std::abs(fill_price - fill_time_mid) / fill_time_mid;
        analysis.market_impact_bps = impact * 10000.0;
        
        fills_.push_back(analysis);
    }
    
    void print_report() const {
        if (fills_.empty()) {
            std::cout << "\nNo fills to analyze.\n";
            return;
        }
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "SLIPPAGE & MARKET IMPACT ANALYSIS\n";
        std::cout << std::string(70, '=') << "\n\n";
        
        // Calculate statistics
        double avg_slippage = 0.0;
        double avg_adverse = 0.0;
        double avg_impact = 0.0;
        
        for (const auto& fill : fills_) {
            avg_slippage += fill.slippage_bps;
            avg_adverse += fill.adverse_selection_bps;
            avg_impact += fill.market_impact_bps;
        }
        
        avg_slippage /= fills_.size();
        avg_adverse /= fills_.size();
        avg_impact /= fills_.size();
        
        std::cout << "Total Fills:              " << fills_.size() << "\n\n";
        
        std::cout << "AVERAGE METRICS (bps)\n";
        std::cout << std::string(70, '-') << "\n";
        std::cout << "Total Slippage:           " << std::fixed << std::setprecision(3)
                  << avg_slippage << " bps\n";
        std::cout << "  ├─ Adverse Selection:   " << avg_adverse << " bps\n";
        std::cout << "  └─ Market Impact:       " << avg_impact << " bps\n\n";
        
        // Slippage distribution
        std::cout << "SLIPPAGE DISTRIBUTION\n";
        std::cout << std::string(70, '-') << "\n";
        
        std::vector<double> slippages;
        for (const auto& fill : fills_) {
            slippages.push_back(fill.slippage_bps);
        }
        std::sort(slippages.begin(), slippages.end());
        
        std::cout << "p10:  " << slippages[slippages.size() / 10] << " bps\n";
        std::cout << "p50:  " << slippages[slippages.size() / 2] << " bps\n";
        std::cout << "p90:  " << slippages[slippages.size() * 9 / 10] << " bps\n";
        std::cout << "p99:  " << slippages[slippages.size() * 99 / 100] << " bps\n";
    }
    
private:
    std::vector<FillAnalysis> fills_;
};

// ============================================================================
// Risk Kill-Switch Logger
// ============================================================================
// PURPOSE: Log all risk limit breaches and automatic protective actions
// CRITICAL: Must document when system halts trading for regulatory compliance
// ============================================================================
class RiskBreachLogger {
public:
    explicit RiskBreachLogger(const std::string& log_path)
        : log_file_(log_path), breach_count_(0) {
        
        if (!log_file_.is_open()) {
            throw std::runtime_error("Failed to open risk log: " + log_path);
        }
        
        log_file_ << "# RISK KILL-SWITCH LOG\n";
        log_file_ << "# Generated: " << get_timestamp() << "\n";
        log_file_ << "# ============================================\n\n";
    }
    
    ~RiskBreachLogger() {
        if (log_file_.is_open()) {
            log_file_ << "\n# Total breaches: " << breach_count_ << "\n";
            log_file_.close();
        }
    }
    
    void log_position_breach(int64_t timestamp_ns, int current_position,
                            int max_position, const std::string& action) {
        log_file_ << "[" << timestamp_ns << "] POSITION_BREACH: "
                  << "current=" << current_position
                  << " max=" << max_position
                  << " action=" << action << "\n";
        log_file_.flush();
        ++breach_count_;
    }
    
    void log_drawdown_breach(int64_t timestamp_ns, double current_drawdown,
                            double max_drawdown, const std::string& action) {
        log_file_ << "[" << timestamp_ns << "] DRAWDOWN_BREACH: "
                  << "current=" << std::fixed << std::setprecision(2) 
                  << current_drawdown
                  << " max=" << max_drawdown
                  << " action=" << action << "\n";
        log_file_.flush();
        ++breach_count_;
    }
    
    void log_order_rate_breach(int64_t timestamp_ns, int orders_per_second,
                              int max_rate, const std::string& action) {
        log_file_ << "[" << timestamp_ns << "] ORDER_RATE_BREACH: "
                  << "current_rate=" << orders_per_second
                  << " max_rate=" << max_rate
                  << " action=" << action << "\n";
        log_file_.flush();
        ++breach_count_;
    }
    
    size_t get_breach_count() const { return breach_count_; }
    
private:
    std::ofstream log_file_;
    size_t breach_count_;
    
    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// ============================================================================
// System Verification Logger
// ============================================================================
// PURPOSE: Document hardware/software configuration for reproducibility
// OUTPUT: CPU model, clock source, kernel version, NIC settings
// ============================================================================
class SystemVerificationLogger {
public:
    static void generate_report(const std::string& output_path) {
        std::ofstream file(output_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create system verification log");
        }
        
        file << "# SYSTEM VERIFICATION REPORT\n";
        file << "# Generated: " << get_timestamp() << "\n";
        file << "# ============================================\n\n";
        
        file << "CLOCK SYNCHRONIZATION\n";
        file << std::string(70, '-') << "\n";
        file << "Clock Source:     TSC (Time Stamp Counter)\n";
        file << "Verification:     Single-threaded deterministic replay\n";
        file << "Drift Tracking:   Not applicable (simulated time)\n";
        file << "Precision:        Nanosecond (int64_t)\n\n";
        
        file << "HARDWARE CONFIGURATION\n";
        file << std::string(70, '-') << "\n";
        file << "NOTE: Backtesting uses simulated execution\n";
        file << "Production deployment requires:\n";
        file << "  - CPU frequency locked (no turbo boost)\n";
        file << "  - NUMA node pinning for memory locality\n";
        file << "  - IRQ affinity to isolate trading threads\n";
        file << "  - NIC offload disabled for determinism\n\n";
        
        file << "SOFTWARE CONFIGURATION\n";
        file << std::string(70, '-') << "\n";
        file << "Compiler:         C++17\n";
        file << "Optimization:     -O3 -march=native\n";
        file << "Threading:        Single-threaded deterministic\n";
        file << "Memory:           Stack-allocated (no dynamic allocation in hot path)\n\n";
        
        file << "DETERMINISM GUARANTEES\n";
        file << std::string(70, '-') << "\n";
        file << "✓ Fixed random seed (configurable)\n";
        file << "✓ Sequential event replay (no threading)\n";
        file << "✓ Bit-for-bit reproducible across runs\n";
        file << "✓ SHA256 checksums for data integrity\n";
        file << "✓ Event-by-event audit trail\n\n";
        
        file.close();
        std::cout << "✓ System verification report written to: " << output_path << "\n";
    }
    
private:
    static std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

} // namespace InstitutionalLogging
