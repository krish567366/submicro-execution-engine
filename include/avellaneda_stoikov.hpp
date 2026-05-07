#pragma once

#include "common_types.hpp"
#include <cmath>
#include <algorithm>

namespace hft {
namespace market_making {

/**
 * Avellaneda-Stoikov Market Making (2008) - "High-frequency trading in a limit order book"
 * 
 * Optimized for Branchless C++ Execution.
 * 
 * Core Logic:
 * 1. Reservation Price: r(s, q) = s - q * gamma * sigma^2 * (T - t)
 *    - s: mid price, q: inventory, gamma: risk aversion, sigma: volatility
 * 
 * 2. Optimal Spreads: delta_a + delta_b = gamma * sigma^2 * (T - t) + (2 / gamma) * ln(1 + gamma/k)
 *    - k: book density (lambda)
 */
class AvellanedaStoikov {
public:
    struct Parameters {
        double gamma; // Risk aversion (e.g., 0.1)
        double sigma; // Volatility (e.g., 2.0)
        double k;     // Order book liquidity factor (e.g., 1.5)
        double T;     // Time horizon (e.g., 1.0 = end of day)
    };

    AvellanedaStoikov(Parameters params) : params_(params) {
        // Precompute constants
        vol_variance_ = params_.sigma * params_.sigma;
        spread_log_term_ = (2.0 / params_.gamma) * std::log(1.0 + params_.gamma / params_.k);
    }

    struct Quotes {
        double bid_price;
        double ask_price;
        double reservation_price;
    };

    /**
     * Calculate optimal quotes branchlessly.
     * @param mid_price Current Mid Price
     * @param inventory Current Net Position (Signed: +Long, -Short)
     * @param time_fraction T-t (Time remaining fraction, 1.0 to 0.0)
     */
    inline Quotes calculate(double mid_price, int32_t inventory, double time_fraction) const {
        // 1. Reservation Price
        // r = s - q * gamma * sigma^2 * time
        // If inventory > 0 (Long), we Lower price to sell.
        // If inventory < 0 (Short), we Raise price to buy.
        double risk_factor = inventory * params_.gamma * vol_variance_ * time_fraction;
        double reservation_px = mid_price - risk_factor;

        // 2. Optimal Half-Spread
        // spread = gamma * sigma^2 * time + log_term
        double half_spread = (params_.gamma * vol_variance_ * time_fraction) + spread_log_term_;
        half_spread *= 0.5; // Split bid/ask

        // 3. Final Quotes around Reservation Price
        return Quotes{
            reservation_px - half_spread, // Bid
            reservation_px + half_spread, // Ask
            reservation_px
        };
    }

private:
    Parameters params_;
    double vol_variance_;
    double spread_log_term_;
};

} // namespace market_making
} // namespace hft