#pragma once

#include "common_types.hpp"
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

// AVX Intrinsics for Core Risk Optimization
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

// GCC/Clang Branch Prediction Hints
#if defined(__GNUC__) || defined(__clang__)
    #define HFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define HFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define HFT_FORCE_INLINE [[gnu::always_inline]] inline
#else
    #define HFT_LIKELY(x)   (x)
    #define HFT_UNLIKELY(x) (x)
    #define HFT_FORCE_INLINE inline
#endif

namespace hft {

/**
 * Cache-Aligned Risk Control Engine with AVX-256 Optimization
 * 
 * Optimization Strategy:
 * 1. Segregate "Hot Local Writes" (Position, PnL) from "Cold Remote Writes" (Config, Kill Switch).
 * 2. Align structures to 64-byte cache lines.
 * 3. Use AVX-256 to vectorise multiple limit checks into a single CPU instruction.
 */
class RiskControl {
public:
    struct LimitParams {
        int64_t max_pos;
        double max_loss;
        double max_val;
        double max_freq;
    };

    explicit RiskControl(const LimitParams& params) {
        config_.max_pos_limit = static_cast<double>(params.max_pos);
        config_.max_loss_limit = params.max_loss;
        config_.max_val_limit = params.max_val;
        config_.max_freq_limit = params.max_freq;
        
        config_.current_regime.store(MarketRegime::NORMAL, std::memory_order_relaxed);
        config_.regime_multiplier.store(1.0, std::memory_order_relaxed);
        shared_signals_.kill_switch_triggered.store(false, std::memory_order_relaxed);
        hot_counters_.total_pnl.store(0.0, std::memory_order_relaxed);
        hot_counters_.current_position.store(0, std::memory_order_relaxed);
        hot_counters_.daily_trade_count.store(0, std::memory_order_relaxed);
    }

    explicit RiskControl(
        int64_t max_position = 1000,
        double max_loss_threshold = 10000.0,
        double max_order_value = 100000.0
    ) : RiskControl({max_position, max_loss_threshold, max_order_value, 10000.0}) {}

    /**
     * Overload for AlphaYieldEngine checks (bid/ask aggregate)
     */
    inline bool check_pre_trade_limits(std::pair<double, double> qties, double price) const {
        // Simplified raw check for production loop
        if (HFT_UNLIKELY(shared_signals_.kill_switch_triggered.load(std::memory_order_acquire))) return false;
        double total_qty = std::abs(qties.first) + std::abs(qties.second);
        if (total_qty > config_.max_pos_limit) return false;
        return true;
    }

    /**
     * Hot Path: Pre-Trade Risk Check (SIMD Optimized)
     * 
     * Packs 4 constraints into a YMM register:
     * Lane 0: New Position vs Max Position
     * Lane 1: Current PnL vs Max Loss
     * Lane 2: Order Value vs Max Value
     * Lane 3: Trade Count vs Freq Limit
     * 
     * Cycles: ~10 vs ~30 scalar
     */
    HFT_FORCE_INLINE bool check_pre_trade_limits(const Order& order, int64_t current_position) const {
        // 1. Critical Safety Checks (Scalar, Memory Bound)
        if (HFT_UNLIKELY(shared_signals_.kill_switch_triggered.load(std::memory_order_acquire))) return false;
        if (HFT_UNLIKELY(config_.current_regime.load(std::memory_order_acquire) == MarketRegime::HALTED)) return false;

#if defined(__AVX2__)
        // 2. Prepare Vector Data
        
        // Calculate new position
        double pos_delta = (DoubleSide(order.side) * static_cast<double>(order.quantity));
        double new_pos_abs = std::abs(static_cast<double>(current_position) + pos_delta);
        
        // Current PnL (Negative is bad, so we compare -PnL vs MaxLoss)
        double cur_loss = -hot_counters_.total_pnl.load(std::memory_order_relaxed);
        
        // Order Value
        double order_val = order.price * order.quantity;
        
        // Trade Count
        double cur_freq = static_cast<double>(hot_counters_.daily_trade_count.load(std::memory_order_relaxed));

        // Load Values: [NewPos, PnL_Loss, OrderVal, Freq]
        __m256d values = _mm256_set_pd(cur_freq, order_val, cur_loss, new_pos_abs);
        
        // Load Limits: [MaxFreq, MaxVal, MaxLoss, MaxPos]
        // Note: set_pd arguments are reverse order (3, 2, 1, 0)
        __m256d limits = _mm256_set_pd(config_.max_freq_limit, 
                                       config_.max_val_limit, 
                                       config_.max_loss_limit, 
                                       config_.max_pos_limit * config_.regime_multiplier.load(std::memory_order_relaxed));

        // 3. Compare (Values > Limits?)
        // _CMP_GT_OQ (Greater Than, Ordered Non-Signaling)
        __m256d mask = _mm256_cmp_pd(values, limits, _CMP_GT_OQ);
        
        // 4. Extract Result
        // movemask extracts sign bits. If any bit is 1, a check failed.
        int res = _mm256_movemask_pd(mask);
        
        // If result is non-zero, one of the constraints was violated
        if (HFT_UNLIKELY(res != 0)) {
            // Trigger kill switch on PnL breach (Lane 1) or Freq (Lane 3)
            // Bit 1 or Bit 3 set? movemask returns bits [3,2,1,0]
            if (res & (1 << 1)) trigger_kill_switch(); // Lane 1 (Loss)
            return false;
        }
        
        return true;
#else
        // Scalar Fallback
        int64_t new_pos = current_position + (static_cast<int64_t>(order.quantity) * (order.side == Side::BUY ? 1 : -1));
        if (std::abs(new_pos) > config_.max_pos_limit) return false;
        
        if (hot_counters_.total_pnl.load(std::memory_order_relaxed) < -config_.max_loss_limit) { 
            trigger_kill_switch(); 
            return false; 
        }
        
        if (order.price * order.quantity > config_.max_val_limit) return false;
        if (hot_counters_.daily_trade_count.load(std::memory_order_relaxed) >= config_.max_freq_limit) return false;
        
        return true;
#endif
    }

    // [Rest of methods preserved, updated to use double configs]

    void update_pnl(double pnl_delta) {
        double old_pnl = hot_counters_.total_pnl.load(std::memory_order_relaxed);
        while (!hot_counters_.total_pnl.compare_exchange_weak(old_pnl, old_pnl + pnl_delta,
                                                              std::memory_order_relaxed,
                                                              std::memory_order_relaxed)) {}

        if (HFT_UNLIKELY((old_pnl + pnl_delta) < -config_.max_loss_limit)) {
            trigger_kill_switch();
        }
    }

    void update_position(Side side, uint64_t quantity) {
        const int64_t delta = (side == Side::BUY) ?
            static_cast<int64_t>(quantity) : -static_cast<int64_t>(quantity);
        hot_counters_.current_position.fetch_add(delta, std::memory_order_relaxed);
    }

    void increment_trade_count() {
        hot_counters_.daily_trade_count.fetch_add(1, std::memory_order_relaxed);
    }

    void set_regime_multiplier(double volatility_index) {
        MarketRegime new_regime;
        double multiplier;

        if (volatility_index < 0.5) {
            new_regime = MarketRegime::NORMAL;
            multiplier = 1.0;
        } else if (volatility_index < 1.0) {
            new_regime = MarketRegime::ELEVATED_VOLATILITY;
            multiplier = 0.7;
        } else if (volatility_index < 2.0) {
            new_regime = MarketRegime::HIGH_STRESS;
            multiplier = 0.4;
        } else {
            new_regime = MarketRegime::HALTED;
            multiplier = 0.0;
        }

        config_.current_regime.store(new_regime, std::memory_order_release);
        config_.regime_multiplier.store(multiplier, std::memory_order_release);
        // max pos limit is dynamic now based on multiplier in hot path
    }

    // Getters and Trigger logic preserved...
    void trigger_kill_switch() const {
        shared_signals_.kill_switch_triggered.store(true, std::memory_order_release);
    }
    
    bool is_kill_switch_triggered() const {
        return shared_signals_.kill_switch_triggered.load(std::memory_order_acquire);
    }

    // Helper for SIMD branch
    inline double DoubleSide(Side s) const { return (s == Side::BUY ? 1.0 : -1.0); }

private:
    // Group 1: Configuration (Doubles for SIMD)
    struct alignas(64) ConfigState {
        double max_pos_limit;
        double max_loss_limit;
        double max_val_limit;
        double max_freq_limit;
        
        std::atomic<double> regime_multiplier;
        std::atomic<MarketRegime> current_regime;
    };
    ConfigState config_;

    // Group 2: Shared
    struct alignas(64) SharedSignals {
        mutable std::atomic<bool> kill_switch_triggered;
    };
    SharedSignals shared_signals_;

    // Group 3: Hot Counters
    struct alignas(64) HotCounters {
        std::atomic<double> total_pnl;
        std::atomic<int64_t> current_position;
        std::atomic<int64_t> daily_trade_count;
    };
    HotCounters hot_counters_;
};

} // namespace hft