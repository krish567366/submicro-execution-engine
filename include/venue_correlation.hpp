#pragma once

#include <array>
#include <cmath>
#include <atomic>

namespace hft {
namespace mm {

/**
 * Multi-Venue Toxicity Hub (Tier-1 Desk Stuff)
 * 
 * Tracks toxicity indicators across multiple venues/products (e.g., NSE FO vs Cash).
 * Provides a 'Global Toxicity Multiplier' to adjust P(adverse) and ExitCost.
 */
class MultiVenueCorrelator {
public:
    static constexpr size_t MAX_VENUES = 4;
    
    struct VenueState {
        double vpin = 0.0;
        double ofi = 0.0;
        double mid_price = 0.0;
        bool is_sweeping = false;
        uint64_t last_update_ns = 0;
    };

    void update_venue(size_t venue_id, double vpin, double ofi, double mid, bool is_sweeping, uint64_t ts_ns) {
        if (venue_id >= MAX_VENUES) return;
        venues_[venue_id] = {vpin, ofi, mid, is_sweeping, ts_ns};
    }

    /**
     * Get Cross-Venue Signal (Lead-Lag Vortex)
     * Returns directional bias [-1.0, 1.0] based on correlate behavior.
     */
    double get_lead_lag_vortex(size_t primary_id) const {
        double bias = 0.0;
        for (size_t i = 0; i < MAX_VENUES; ++i) {
            if (i == primary_id) continue;
            const auto& v = venues_[i];
            // Directional Signal Propagation
            if (v.ofi > 500.0) bias += 0.25;
            if (v.ofi < -500.0) bias -= 0.25;
            if (v.is_sweeping) bias += (v.ofi > 0 ? 0.5 : -0.5);
        }
        return std::clamp(bias, -1.0, 1.0);
    }

    /**
     * Get Global Risk Multiplier [1.0, 5.0]
     * Scales with correlated toxicity.
     */
    double get_global_risk_factor(size_t primary_venue_id) const {
        double factor = 1.0;
        for (size_t i = 0; i < MAX_VENUES; ++i) {
            if (i == primary_venue_id) continue;
            
            const auto& v = venues_[i];
            // Toxicity Propagation: If other venue is sweeping or high VPIN
            if (v.is_sweeping) factor += 1.5;
            if (v.vpin > 0.6) factor += 1.0;
            
            // Check for directional correlation (OFI overlap)
            // Simplified lead-lag detection logic could go here
        }
        return std::min(5.0, factor);
    }

    bool any_correlated_sweep(size_t primary_venue_id) const {
        for (size_t i = 0; i < MAX_VENUES; ++i) {
            if (i == primary_venue_id) continue;
            if (venues_[i].is_sweeping) return true;
        }
        return false;
    }

private:
    std::array<VenueState, MAX_VENUES> venues_;
};

/**
 * Sharp Lead-Lag Radar (First-Packet Detection)
 * 
 * Cancels faster by anticipating sweeps when multiple levels
 * are hit within a sub-microsecond window on correlated venues.
 */
class SharpLeadLagRadar {
public:
    struct Cluster {
        double total_delta = 0;
        double ghost_delta = 0; // Cancellations/Additions without trades
        uint64_t last_hit_ns = 0;
    };

    /**
     * Detects sweeps by looking at both realized OFI and 'Ghost' movement (Level updates)
     */
    bool detect_sub_micro_burst(size_t venue_id, double ofi_delta, double trade_vol, uint64_t ns) {
        auto& c = clusters_[venue_id % 4];
        
        if (ns - c.last_hit_ns > 1000000) {
            c.total_delta = 0;
            c.ghost_delta = 0;
        }
        
        c.last_hit_ns = ns;
        c.total_delta += std::abs(ofi_delta);
        
        // Ghost Delta: OFI move NOT caused by trades (i.e., someone pulling/stuffing levels)
        double ghost = std::abs(ofi_delta) - trade_vol;
        if (ghost > 500.0) c.ghost_delta += ghost;

        // APEX TRIGGER: If >2 venues show ghost movement, it's a coordinated sweep starting
        int active_ghost_venues = 0;
        for (int i=0; i<4; ++i) if (clusters_[i].ghost_delta > 1000.0) active_ghost_venues++;

        return (c.total_delta > 2000.0) || (active_ghost_venues >= 2);
    }

private:
    Cluster clusters_[4];
};

/**
 * Post-Sweep Impact Diagnostic (Reversion Gate)
 */
class ImpactReversionGate {
public:
    struct State {
        bool in_post_sweep = false;
        double sweep_price = 0;
        uint64_t sweep_time = 0;
        double initial_spread = 0;
    };

    void on_sweep(double price, double spread, uint64_t ts_ns) {
        state_.in_post_sweep = true;
        state_.sweep_price = price;
        state_.sweep_time = ts_ns;
        state_.initial_spread = spread;
    }

    /**
     * Logic: Returns a 'Conviction Score' [0, 1] for reversion fading.
     */
    double get_reversion_conviction(double current_vpin, double current_ofi, double current_spread, uint64_t now_ns) {
        if (!state_.in_post_sweep) return 0.0;

        // 1. Time out (reversion opportunity vanishes)
        uint64_t age = now_ns - state_.sweep_time;
        if (age > 1500000000ULL) { // 1.5s
            state_.in_post_sweep = false;
            return 0.0;
        }

        // 2. Loosened Convergence Tests
        double trade_conviction = std::max(0.0, 1.0 - (current_vpin / 0.5)); // Loosened from 0.3
        double ofi_conviction = std::max(0.0, 1.0 - (std::abs(current_ofi) / 0.4)); // Loosened from 0.2
        
        // 3. Stabilization Check
        bool stabilized = current_spread < state_.initial_spread * 2.0;
        
        if (!stabilized) return 0.0;

        double final_conviction = (trade_conviction + ofi_conviction) * 0.5;
        // Decay conviction as time passes
        double time_decay = 1.0 - (double)age / 1500000000.0;
        
        return std::max(0.0, final_conviction * time_decay);
    }

private:
    State state_;
};

} // namespace mm
} // namespace hft
