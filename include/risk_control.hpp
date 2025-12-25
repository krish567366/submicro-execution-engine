#pragma once

#include "common_types.hpp"
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>

namespace hft {

// ====
// Adaptive Risk Control System
// Implements regime-based position limits and circuit breakers
// ====

class RiskControl {
public:
    // 
    // Constructor
    // 
    explicit RiskControl(
        int64_t max_position = 1000,        // Maximum absolute position
        double max_loss_threshold = 10000.0, // Maximum loss before halt
        double max_order_value = 100000.0    // Maximum single order value
    ) : base_max_position_(max_position),
        current_max_position_(max_position),
        max_loss_threshold_(max_loss_threshold),
        max_order_value_(max_order_value),
        regime_multiplier_(1.0),
        current_regime_(MarketRegime::NORMAL),
        kill_switch_triggered_(false),
        total_pnl_(0.0),
        current_position_(0),
        daily_trade_count_(0),
        max_daily_trades_(10000) {
    }
    
    // 
    // Pre-trade risk checks
    // Returns true if order passes all checks
    // 
    bool check_pre_trade_limits(const Order& order, int64_t current_position) const {
        // Check 1: Kill switch
        if (kill_switch_triggered_.load(std::memory_order_acquire)) {
            return false;  // Trading halted
        }
        
        // Check 2: Position limit
        const int64_t new_position = current_position + 
            (order.side == Side::BUY ? 
             static_cast<int64_t>(order.quantity) : 
             -static_cast<int64_t>(order.quantity));
        
        const int64_t max_pos = current_max_position_.load(std::memory_order_acquire);
        if (std::abs(new_position) > max_pos) {
            return false;  // Would exceed position limit
        }
        
        // Check 3: Order value limit
        const double order_value = order.price * order.quantity;
        if (order_value > max_order_value_) {
            return false;  // Single order too large
        }
        
        // Check 4: Daily trade count limit
        if (daily_trade_count_.load(std::memory_order_acquire) >= max_daily_trades_) {
            return false;  // Exceeded daily trade limit
        }
        
        // Check 5: Loss limit
        const double current_pnl = total_pnl_.load(std::memory_order_acquire);
        if (current_pnl < -max_loss_threshold_) {
            // Trigger kill switch on excessive loss
            trigger_kill_switch();
            return false;
        }
        
        // Check 6: Regime-specific restrictions
        if (current_regime_.load(std::memory_order_acquire) == MarketRegime::HALTED) {
            return false;
        }
        
        return true;
    }
    
    // 
    // Set regime multiplier based on market conditions
    // Dynamically scales position limits
    // 
    // volatility_index: normalized volatility measure (0.0 = calm, 1.0+ = stressed)
    // 
    void set_regime_multiplier(double volatility_index) {
        MarketRegime new_regime;
        double multiplier;
        
        // Regime classification
        if (volatility_index < 0.5) {
            // Normal market conditions
            new_regime = MarketRegime::NORMAL;
            multiplier = 1.0;
        } else if (volatility_index < 1.0) {
            // Elevated volatility
            new_regime = MarketRegime::ELEVATED_VOLATILITY;
            multiplier = 0.7;  // Reduce position limit by 30%
        } else if (volatility_index < 2.0) {
            // High stress
            new_regime = MarketRegime::HIGH_STRESS;
            multiplier = 0.4;  // Reduce position limit by 60%
        } else {
            // Extreme stress - halt trading
            new_regime = MarketRegime::HALTED;
            multiplier = 0.0;
        }
        
        // Update regime and multiplier atomically
        current_regime_.store(new_regime, std::memory_order_release);
        regime_multiplier_.store(multiplier, std::memory_order_release);
        
        // Update position limit
        const int64_t new_limit = static_cast<int64_t>(base_max_position_ * multiplier);
        current_max_position_.store(new_limit, std::memory_order_release);
    }
    
    // 
    // Trigger kill switch (irreversible trading halt)
    // 
    void trigger_kill_switch() const {
        kill_switch_triggered_.store(true, std::memory_order_release);
        // In production: would also send alerts, close positions, etc.
    }
    
    // 
    // Check if kill switch is active
    // 
    bool is_kill_switch_triggered() const {
        return kill_switch_triggered_.load(std::memory_order_acquire);
    }
    
    // 
    // Update P&L (called after fills)
    // 
    void update_pnl(double pnl_delta) {
        // Atomic add
        double current = total_pnl_.load(std::memory_order_relaxed);
        while (!total_pnl_.compare_exchange_weak(current, current + pnl_delta,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
            // Retry on contention
        }
        
        // Check loss limit after update
        if (total_pnl_.load(std::memory_order_acquire) < -max_loss_threshold_) {
            trigger_kill_switch();
        }
    }
    
    // 
    // Update position (called after fills)
    // 
    void update_position(Side side, uint64_t quantity) {
        const int64_t delta = (side == Side::BUY) ? 
            static_cast<int64_t>(quantity) : -static_cast<int64_t>(quantity);
        
        int64_t current = current_position_.load(std::memory_order_relaxed);
        while (!current_position_.compare_exchange_weak(current, current + delta,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
            // Retry on contention
        }
    }
    
    // 
    // Increment trade counter
    // 
    void increment_trade_count() {
        daily_trade_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // 
    // Reset daily counters (called at start of trading day)
    // 
    void reset_daily_counters() {
        daily_trade_count_.store(0, std::memory_order_release);
        total_pnl_.store(0.0, std::memory_order_release);
    }
    
    // 
    // Reset kill switch (requires manual intervention)
    // 
    void reset_kill_switch(const std::string& authorization_code) {
        // In production: would require proper authentication
        if (authorization_code == "EMERGENCY_RESET") {
            kill_switch_triggered_.store(false, std::memory_order_release);
        }
    }
    
    // 
    // Calculate position-adjusted risk limit for quote sizing
    // Returns: safe quote size given current position
    // 
    double get_safe_quote_size(int64_t current_position, double base_size) const {
        const int64_t max_pos = current_max_position_.load(std::memory_order_acquire);
        const int64_t available_capacity = max_pos - std::abs(current_position);
        
        if (available_capacity <= 0) {
            return 0.0;  // At limit, no more quoting
        }
        
        // Scale quote size by available capacity
        const double capacity_ratio = static_cast<double>(available_capacity) / max_pos;
        return base_size * std::min(capacity_ratio, 1.0);
    }
    
    // 
    // Check if position unwinding is needed
    // Returns: recommended unwind size (positive = sell, negative = buy)
    // 
    int64_t get_unwind_recommendation(int64_t current_position) const {
        const int64_t max_pos = current_max_position_.load(std::memory_order_acquire);
        const int64_t abs_pos = std::abs(current_position);
        
        // If over 80% of limit, recommend unwinding
        if (abs_pos > max_pos * 0.8) {
            const int64_t excess = abs_pos - static_cast<int64_t>(max_pos * 0.5);
            return (current_position > 0) ? excess : -excess;
        }
        
        return 0;  // No unwind needed
    }
    
    // 
    // Getters
    // 
    int64_t get_max_position() const {
        return current_max_position_.load(std::memory_order_acquire);
    }
    
    int64_t get_current_position() const {
        return current_position_.load(std::memory_order_acquire);
    }
    
    double get_total_pnl() const {
        return total_pnl_.load(std::memory_order_acquire);
    }
    
    MarketRegime get_current_regime() const {
        return current_regime_.load(std::memory_order_acquire);
    }
    
    double get_regime_multiplier() const {
        return regime_multiplier_.load(std::memory_order_acquire);
    }
    
    int64_t get_daily_trade_count() const {
        return daily_trade_count_.load(std::memory_order_acquire);
    }
    
private:
    // 
    // Configuration (immutable after construction)
    // 
    const int64_t base_max_position_;
    const double max_loss_threshold_;
    const double max_order_value_;
    const int64_t max_daily_trades_;
    
    // 
    // Atomic state variables (thread-safe)
    // 
    mutable std::atomic<bool> kill_switch_triggered_;
    std::atomic<int64_t> current_max_position_;
    std::atomic<double> regime_multiplier_;
    std::atomic<MarketRegime> current_regime_;
    std::atomic<double> total_pnl_;
    std::atomic<int64_t> current_position_;
    std::atomic<int64_t> daily_trade_count_;
};

} // namespace hft
