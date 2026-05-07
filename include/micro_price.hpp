#pragma once

#include "common_types.hpp"
#include <cmath>
#include <array>

namespace hft {
namespace mm {

/**
 * Stoikov Micro-Price Estimator (2017)
 * 
 * Concept:
 * Mid-Price is a bad predictor of future price because it ignores the Imbalance.
 * Micro-Price is the "True" fair value adjusted for L1 imbalance.
 * 
 * Formula:
 * P_micro = P_mid + g(I) * Spread
 * I = Imbalance = Q_bid / (Q_bid + Q_ask)
 * g(I) = (2*I - 1) * adjustment_factor
 * 
 * Capability:
 * Uses a pre-calibrated look-up table (or polynomial approximation) for g(I).
 * This price is used to Skew quotes.
 */
class MicroPrice {
public:
    MicroPrice() {}

    /**
     * Calculate Micro-Price O(1)
     */
    static inline double calculate(double bid_px, double ask_px, double bid_qty, double ask_qty) {
        double spread = ask_px - bid_px;
        double mid = (ask_px + bid_px) * 0.5;
        
        double total_qty = bid_qty + ask_qty;
        if (total_qty < 1e-5) return mid;
        
        double imbalance = bid_qty / total_qty; // [0, 1]
        
        // Stoikov approximation: g(I) is roughly linear or sigmoid
        // Let's use a fast linear approx: g(I) = 0.6 * (2*I - 1)
        // 0.6 is a common "fitting" parameter for liquid assets
        
        double g_I = 0.6 * (2.0 * imbalance - 1.0); // Range [-0.6, 0.6]
        
        return mid + (g_I * spread * 0.5); // Adjust half-spread
        // If I=1 (Full Bid), g=0.6 -> Mid + 0.3*Spread -> Towards Ask.
    }
};

}
}
