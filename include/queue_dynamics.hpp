#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>
#include <algorithm>

namespace hft {
namespace mm {

/**
 * Queue Position & Refill Dynamics Engine
 * 
 * Specialized in:
 * 1. Queue Replenishment Prediction: Will this level refill after being hit?
 * 2. Level Decay: How fast is the LOB depth vanishing due to cancellations?
 * 3. Join Probability: Likelihood of being at the head of the queue in T+delta.
 */
class QueueDynamicsEngine {
public:
    struct LevelDynamics {
        double refill_rate = 0.0;     // Qty added per microsecond
        double decay_rate = 0.0;      // Qty canceled per microsecond
        double refill_prob = 0.5;     // Probability level survives a sweep
        double expected_wait_ns = 0;  // Estimated time to head-of-queue
    };

    QueueDynamicsEngine() {
        bid_decay_.fill(1.0); bid_refill_.fill(1.0);
        ask_decay_.fill(1.0); ask_refill_.fill(1.0);
    }

    /**
     * Update dynamics based on level delta observability
     */
    void update(const MarketTick& tick, uint64_t ns) {
        if (HFT_UNLIKELY(last_ts_ == 0)) {
            last_ts_ = ns;
            for(int i=0; i<10; ++i) { 
                last_bid_q_[i] = tick.bid_sizes[i]; 
                last_ask_q_[i] = tick.ask_sizes[i]; 
            }
            return;
        }

        double dt = (double)(ns - last_ts_) / 1000.0; // microseconds
        if (dt < 0.1) return;

        for (int i = 0; i < 10; ++i) {
            // BID DECAY & REFILL
            double b_delta = static_cast<double>(tick.bid_sizes[i]) - last_bid_q_[i];
            if (b_delta < 0) { // Decay (Cxl or Phil)
                bid_decay_[i] = (0.95 * bid_decay_[i]) + (0.05 * std::abs(b_delta) / dt);
            } else if (b_delta > 0) { // Refill (Addition)
                bid_refill_[i] = (0.95 * bid_refill_[i]) + (0.05 * b_delta / dt);
            }

            // ASK DECAY & REFILL
            double a_delta = static_cast<double>(tick.ask_sizes[i]) - last_ask_q_[i];
            if (a_delta < 0) {
                ask_decay_[i] = (0.95 * ask_decay_[i]) + (0.05 * std::abs(a_delta) / dt);
            } else if (a_delta > 0) {
                ask_refill_[i] = (0.95 * ask_refill_[i]) + (0.05 * a_delta / dt);
            }

            last_bid_q_[i] = tick.bid_sizes[i];
            last_ask_q_[i] = tick.ask_sizes[i];
        }
        last_ts_ = ns;
    }

    /**
     * Calculate Join Probability
     * P(Fill | Join at L=level)
     */
    double get_fill_probability(bool buy, int level, double time_horizon_us) const {
        const auto& decay = buy ? bid_decay_ : ask_decay_;
        const auto& refill = buy ? bid_refill_ : ask_refill_;
        
        // Intensity of hits
        double lambda = decay[level];
        double mu = refill[level];
        
        // Probability level is depleted within time_horizon
        // Simple Queueing Theory: P(T < t) = 1 - exp(-lambda * t)
        // Adjusted for refill: 1 - exp( -(lambda - mu) * t )
        double net_decay = std::max(0.5, lambda - mu * 0.5);
        return 1.0 - std::exp(-net_decay * time_horizon_us * 1e-3);
    }

    /**
     * Join Timing Score
     * High score means "Aggressive Join" (Level is replenishing, high fill prob)
     */
    double get_join_timing_score(bool buy, int level) const {
        const auto& refill = buy ? bid_refill_ : ask_refill_;
        const auto& decay = buy ? bid_decay_ : ask_decay_;
        
        // If Refill > Decay, level is "Healthy"
        double health = refill[level] / (decay[level] + 0.001);
        return std::clamp(health, 0.0, 2.0);
    }

private:
    std::array<double, 10> bid_decay_, bid_refill_;
    std::array<double, 10> ask_decay_, ask_refill_;
    std::array<uint64_t, 10> last_bid_q_, last_ask_q_;
    uint64_t last_ts_ = 0;
};

} // namespace mm
} // namespace hft
