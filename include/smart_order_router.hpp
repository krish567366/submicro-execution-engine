#pragma once

#include "common_types.hpp"
#include <cmath>
#include <array>
#include <limits>

namespace hft {
namespace execution {

/**
 * Smart Order Router (SOR) using Probability Maps
 * 
 * Capability:
 * Decides optimally between:
 * 1. Aggressive Take (Market/Limit IOC)
 * 2. Passive Posting (Limit GTC) based on Alpha Decay vs Queue Position.
 * 
 * Model:
 * ExpectedFillCost(Join) = Spread/2 + ProbabilityOfAdverseSelection * Impact
 * ExpectedFillCost(Take) = Spread/2 + Fees
 * 
 * Logic:
 * If (AlphaStrength > SpreadCost) -> Aggressive.
 * If (AlphaDecay < QueueWaitTime) -> Aggressive.
 * Else -> Passive.
 */
class SmartOrderRouter {
public:
    struct RoutingDecision {
        bool aggressive;
        double price;
        uint64_t target_venue_id; // 0=Primary, 1=Secondary...
    };

    SmartOrderRouter(double taker_fee_bps, double maker_fee_bps)
        : taker_fee_bps_(taker_fee_bps), maker_fee_bps_(maker_fee_bps) {}

    /**
     * Stateless Routing Decision
     * @param alpha Estimated edge (bps)
     * @param spread_bps Current Bid-Ask Spread (bps)
     * @param queue_pos Estimated qty ahead in queue
     * @param adv_selection_prob Probability trade moves against us while queuing (0.0-1.0)
     */
    inline RoutingDecision decide(double alpha_bps, 
                                  double spread_bps, 
                                  uint64_t queue_pos, 
                                  double adv_selection_prob,
                                  double best_bid,
                                  double best_ask,
                                  bool buying) const {
        
        // Cost to Take: Half Spread + Fee
        double take_cost = (spread_bps * 0.5) + taker_fee_bps_;
        
        // Edge if Taken:
        double net_alpha_take = alpha_bps - take_cost;
        
        // Cost to Make: 
        // Benefit: Earn maker rebate (negative fee) or pay lower fee
        // Risk: Adverse Selection (getting run over)
        // If we join queue, we save spread/2 but risk market moving away (Alpha decay or price run)
        
        // Expected Cost = MakerFee + (ProbAdverse * AlphaLoss) + (ProbMiss * MissOpportunity)
        // Simplified: Effective Cost = maker_fee_bps_ + (adv_selection_prob * 5.0); // heuristic 5bps impact
        
        bool aggressive = false;
        
        // 1. If Alpha is huge, don't wait.
        if (net_alpha_take > 2.0) { // > 2bps pure profit
            aggressive = true;
        }
        // 2. If spread is tight (1 tick), taking is cheap.
        else if (spread_bps < 1.5 && alpha_bps > take_cost) {
            aggressive = true;
        }
        // 3. If queue is massive, we won't get filled passive.
        else if (queue_pos > 100000) { 
            aggressive = true;
        }
        
        double price;
        if (aggressive) {
            price = buying ? best_ask : best_bid;
        } else {
            price = buying ? best_bid : best_ask;
        }
        
        return RoutingDecision{aggressive, price, 0};
    }

private:
    double taker_fee_bps_;
    double maker_fee_bps_;
};

} // namespace execution
} // namespace hft