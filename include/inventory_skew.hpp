#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>

namespace hft {
namespace mm {

/**
 * Inventory Skew (Asymmetric Logic)
 * 
 * Concept:
 * When we hold Inventory, we are "Risk-Averse". We generally want to:
 * 1. Quote Aggressively on the side that Reduces Inventory (Unwind).
 * 2. Quote Passively (or not at all) on the side that Increases Inventory.
 * 3. Skew the Mid-Price input to the pricer.
 * 
 * Capability:
 * Generates dynamic biases for the `AvellanedaStoikov` pricer.
 */
class InventoryManager {
public:
    InventoryManager(double max_pos, double risk_aversion) 
        : max_pos_(max_pos), gamma_(risk_aversion) {}

    struct SkewParameters {
        double bid_offset; // Add to theoretical bid
        double ask_offset; // Add to theoretical ask
        double size_bid_scale; // Multiplier for bid size
        double size_ask_scale; // Multiplier for ask size
    };

    inline SkewParameters calculate_skew(int32_t current_inventory, double volatility) const {
        // Normalized Inventory [-1, 1]
        double q = current_inventory / max_pos_;
        
        // 1. Price Skew (Linear)
        // If Long (q > 0), we want to lower prices -> Lower Bid, Lower Ask.
        // Skew = -q * Gamma * Vol^2
        double price_skew_ticks = -q * gamma_ * (volatility * volatility);
        
        // 2. Size Skew (Logistic Decay)
        // If Long (q > 0), we want to Buy Less (Bid Size -> 0) and Sell More (Ask Size -> 1).
        // Logistic function: f(x) = 1 / (1 + exp(k*x))
        // Bid Size Scale: if q -> 1, scale -> 0.
        
        double k = 5.0; // steepness
        double bid_scale = 1.0 / (1.0 + std::exp(k * q));  // If q=0.5, exp(2.5) ~= 12, scale=0.08 (Tiny bid)
        double ask_scale = 1.0 / (1.0 + std::exp(-k * q)); // If q=0.5, exp(-2.5) ~= 0.08, scale=0.92 (Full ask)
        
        return {price_skew_ticks, price_skew_ticks, bid_scale, ask_scale};
    }

private:
    double max_pos_;
    double gamma_;
};

}
}
