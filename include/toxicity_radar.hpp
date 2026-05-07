#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>
#include <algorithm>

namespace hft {
namespace mm {

/**
 * VPIN (Volume-Synchronized Probability of Informed Trading) Radar
 * 
 * Concept:
 * Market Makers lose money to "Informed Traders" (Toxic Flow).
 * VPIN tries to estimate what % of volume is "Toxic" (One-sided, hiding info).
 * 
 * Mechanism:
 * 1. Group trades into "Volume Buckets" (e.g., every 10,000 lots) instead of Time Bars.
 * 2. Calculate Order Flow Imbalance (OFI) = |BuyVol - SellVol| per bucket.
 * 3. Smooth OFI using EWMA.
 * 4. VPIN = Smoothed_OFI / Average_Volume.
 * 
 * Action:
 * If VPIN > Threshold (e.g., 0.25), Widen Spreads aggressively.
 */
class ToxicityRadar {
public:
    static constexpr size_t BUCKET_SIZE = 1000; // Configurable volume bucket
    static constexpr double DECAY = 0.9;        // EWMA decay

    ToxicityRadar() : 
        current_bucket_vol_(0), 
        current_buy_vol_(0), 
        current_sell_vol_(0),
        ewma_imbalance_(0.0),
        ewma_total_vol_(BUCKET_SIZE) {} // Initial guess

    struct ToxicityState {
        double vpin;
        bool is_toxic; // vpin > threshold
    };

    inline ToxicityState update_trade(double price, double size, bool is_buy) {
        // Accumulate (potentially splitting across buckets)
        // Simplified: assumption size < BUCKET_SIZE
        
        if (is_buy) current_buy_vol_ += size;
        else current_sell_vol_ += size;
        
        current_bucket_vol_ += size;

        if (current_bucket_vol_ >= BUCKET_SIZE) {
            // Bucket Complete!
            double imbalance = std::abs(current_buy_vol_ - current_sell_vol_);
            
            // Update EWMA (Recursive)
            ewma_imbalance_ = (DECAY * ewma_imbalance_) + ((1.0 - DECAY) * imbalance);
            ewma_total_vol_ = (DECAY * ewma_total_vol_) + ((1.0 - DECAY) * current_bucket_vol_);
            
            // Reset Bucket
            current_bucket_vol_ = 0;
            current_buy_vol_ = 0;
            current_sell_vol_ = 0;
        }

        // Calculate continuous VPIN approximation
        // VPIN ~= E[|V_b - V_s|] / E[V_total]
        double vpin = (ewma_total_vol_ > 1e-5) ? (ewma_imbalance_ / ewma_total_vol_) : 0.0;
        
        return {vpin, vpin > 0.25}; // Threshold 0.25 is classic paper value
    }

    inline double get_vpin() const {
        return (ewma_total_vol_ > 1e-5) ? (ewma_imbalance_ / ewma_total_vol_) : 0.0;
    }

private:
    double current_bucket_vol_;
    double current_buy_vol_;
    double current_sell_vol_;
    
    double ewma_imbalance_;
    double ewma_total_vol_;
};

}
}
