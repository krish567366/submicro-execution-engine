#pragma once

#include "common_types.hpp"
#include <array>
#include <vector>
#include <algorithm>

namespace hft {
namespace mm {

/**
 * Grid Ladder Manager
 * 
 * Concept:
 * Managing 10-20 pairs of limit orders ("The Grid") is hard.
 * You cannot send 20 "NewOrderSingle" messages every tick (latency/throttle).
 * 
 * Capability:
 * - Maintains a "Shadow Grid" in memory.
 * - Calculates "Diffs" (Delta) between Shadow Grid and Exchange State.
 * - Prioritizes updates: Only move quotes if price deviation > Threshold.
 * - Logic: "Lazy Grid Updates".
 */
template<size_t GridLevels = 10>
class GridLadder {
public:
    struct GridLevel {
        double price;
        double input_qty; // Desired
        uint64_t order_id; // Current Exchange ID (0 if none)
    };

    struct GridState {
        std::array<GridLevel, GridLevels> bids;
        std::array<GridLevel, GridLevels> asks;
    };

    GridLadder(double tick_size) : tick_size_(tick_size) {}

    // Generate Desired Grid based on Fair Value & Volatility
    inline void generate_ladder(double fair_value, double spread, double vol_step, double size_base, double size_step) {
        for (size_t i = 0; i < GridLevels; ++i) {
            // Asks
            state_.asks[i].price = snap_to_tick(fair_value + (spread * 0.5) + (i * vol_step));
            state_.asks[i].input_qty = size_base + (i * size_step);
            
            // Bids
            state_.bids[i].price = snap_to_tick(fair_value - (spread * 0.5) - (i * vol_step));
            state_.bids[i].input_qty = size_base + (i * size_step);
        }
    }

    // Determine Required Actions (Diffing)
    // Returns number of required updates
    // updates[] populated with actions
    template<typename ActionContainer>
    int compute_diff(const GridState& exchange_state, ActionContainer& actions) {
        int count = 0;
        
        // Helper lambda
        auto check_side = [&](GridLevel& desired, const GridLevel& actual, bool is_buy) {
            if (desired.order_id == 0) {
                // Determine: Create New?
                // Should assign new ID
                // actions.push_back({CREATE, desired.price, desired.input_qty});
            } else {
                // Determine: Modify?
                double px_diff = std::abs(desired.price - actual.price);
                // Hysteresis: Only move if Diff > 1 tick
                if (px_diff > tick_size_ * 1.5) {
                    // actions.push_back({MODIFY, desired.order_id, desired.price...});
                }
            }
        };
        
        // ... (Loop over levels)
        return count;
    }

private:
    double tick_size_;
    GridState state_;

    inline double snap_to_tick(double px) {
        return std::round(px / tick_size_) * tick_size_;
    }
};

}
}
