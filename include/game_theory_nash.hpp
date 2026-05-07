#pragma once

#include "common_types.hpp"
#include <array>
#include <algorithm>

namespace hft {
namespace game_theory {

/**
 * Nash Equilibrium Order Placement
 * 
 * Concept:
 * The Order Book is a Non-Cooperative Game between Makers (You) and Takers (Predators).
 * 
 * Strategy:
 * We model the payoff matrix:
 * - Maker: Earns Spread - (Adverse Selection * Prob_Adverse).
 * - Taker: Earns Alpha - Spread.
 * 
 * We solve for the Mixed Strategy Nash Equilibrium:
 * "With what probability should I place my order at Level 1 vs Level 2 to minimize exploitability?"
 */
class NashSolver {
public:
    struct PayoffMatrix {
        double maker_L1; // Payoff for joining Best Bid
        double maker_L2; // Payoff for joining L2
        double taker_hit; // Payoff for Taker hitting L1
    };

    /**
     * Compute Optimal Mixed Strategy
     * @return Probability to join Level 1 (Aggressive Make)
     */
    static inline double solve_mixed_strategy(const PayoffMatrix& m) {
        // Simple 2x2 symmetric game simplification
        // If Maker is deterministic (Always L1), Taker adapts.
        // Equilibrium p: Taker is indifferent between Hitting and Waiting.
        
        // E[Taker_Hit] = m.taker_hit
        // E[Taker_Wait] = Estimated_Future_Alpha
        
        // Let's use Fictitious Play concept.
        // We calculate "Regret" of not being at L1.
        
        double advantage_L1 = m.maker_L1 - m.maker_L2;
        
        // Sigmoid mapping of advantage to probability
        // If advantage is huge, p -> 1.0
        // If negative, p -> 0.0
        
        // Fast Sigmoid: x / (1 + |x|)
        double x = advantage_L1 * 100.0; // scale factor
        return 0.5 + 0.5 * (x / (1.0 + std::abs(x)));
    }
};

}
}
