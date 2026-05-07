#pragma once

#include "common_types.hpp"
#include <cmath>
#include <algorithm>

namespace hft {
namespace relativity {

/**
 * Spacetime Abitrage Engine (General Relativity)
 * 
 * Concept:
 * In global crypto markets, "Simultaneity" is relative.
 * An arbitrage opportunity exists only if you lie within the intersection of the 
 * future light cones of Exchange A and Exchange B.
 * 
 * Logic:
 * We model exchanges as points in Minkowski Space (t, x, y, z).
 * We calculate the "Interval" `ds^2 = c^2 dt^2 - dx^2`.
 * If `ds^2 >= 0` (Timelike separation), information can travel.
 * If `ds^2 < 0` (Spacelike separation), events are causally disconnected.
 * 
 * Use Case:
 * "Where should I place my server?"
 * The class calculates the geometric "Centroid" of liquidity in spacetime constraints.
 */
class SpacetimeManifold {
public:
    static constexpr double C_FIBER = 200000.0; // Speed of light in fiber (km/s) ~ 2/3 c

    struct Event {
        double t; // Timestamp (seconds)
        double x, y; // Geo-coordinates (km projected)
    };

    /**
     * Check Causality
     * Can Event A trigger Event B?
     */
    static inline bool is_causal(const Event& A, const Event& B) {
        double dt = B.t - A.t;
        if (dt < 0) return false;
        
        double dx = B.x - A.x;
        double dy = B.y - A.y;
        double dist_sq = dx*dx + dy*dy;
        
        // c*dt >= dist
        double light_dist = C_FIBER * dt;
        return (light_dist * light_dist) >= dist_sq;
    }

    /**
     * Calculate Minimum Required Head start
     * How much earlier must we know news at Source to execute at Dest before Competitor?
     * 
     * T_arrive_us = T0 + Dist(Us, Dest)/C
     * T_arrive_them = T0 + Dist(Source, Them)/C + Processing + Dist(Them, Dest)/C
     */
    static inline double required_alpha_horizon(
        double d_source_us, double d_us_dest,
        double d_source_them, double d_them_dest) 
    {
        double t_us = (d_source_us + d_us_dest) / C_FIBER;
        double t_them = (d_source_them + d_them_dest) / C_FIBER;
        
        // If t_us < t_them, we win.
        // If t_us > t_them, we need 't_us - t_them' Alpha PREDICTION horizon.
        return std::max(0.0, t_us - t_them);
    }
};

}
}
