#pragma once

#include "common_types.hpp"
#include <vector>
#include <cmath>
#include <numeric>

namespace hft {
namespace chaos {

/**
 * Short-Term Lyapunov Exponent Estimator
 * 
 * Concept:
 * Chaos Theory measures "Sensitive Dependence on Initial Conditions".
 * Lambda > 0: System is Chaotic (Predictable in short term, diverges in long term).
 * Lambda < 0: System is Stable (Mean Reverting).
 * 
 * Application:
 * If Lambda > 0 is detected in the price series, it implies a "Regime Shift" or "Breakout" is occurring.
 * Standard statistical volatility (Sigma) is a lagging indicator. Lyapunov is leading.
 * 
 * Algorithm:
 * Simplified Rosenstein Algorithm on rolling window.
 * Measures average rate of divergence of nearest neighbors in phase space.
 */
class LyapunovExponent {
public:
    static constexpr int EMBED_DIM = 3;
    static constexpr int LAG = 1;
    static constexpr int WINDOW = 100;

    LyapunovExponent() {}

    void push(double price) {
        history_.push_back(price);
        if (history_.size() > WINDOW) history_.erase(history_.begin());
    }

    double calculate_lambda() {
        if (history_.size() < WINDOW) return 0.0;

        double sum_log_divergence = 0.0;
        int count = 0;

        // Rosenstein's Algorithm: Track divergence of nearby trajectories
        const size_t max_idx = history_.size() - (EMBED_DIM - 1) * LAG - 10;
        if (max_idx < 20) return 0.0;

        // For each point in phase space, find nearest neighbor and track divergence
        for (size_t i = 10; i < max_idx; i += 5) {  // Subsample for speed
            auto vec_i = get_vector(i);
            
            double min_dist = 1e9;
            size_t neighbor_idx = 0;

            // Find nearest neighbor (excluding temporal neighbors)
            for (size_t j = 0; j < max_idx; ++j) {
                if (std::abs((int)j - (int)i) < 10) continue;  // Temporal separation
                
                double dist = distance(vec_i, get_vector(j));
                if (dist > 1e-10 && dist < min_dist) {
                    min_dist = dist;
                    neighbor_idx = j;
                }
            }

            if (min_dist > 1e9 - 1 || min_dist < 1e-10) continue;

            // Track divergence over next 5 steps
            for (int k = 1; k <= 5 && i + k < max_idx && neighbor_idx + k < max_idx; ++k) {
                auto vec_i_evolved = get_vector(i + k);
                auto vec_n_evolved = get_vector(neighbor_idx + k);
                
                double dist_evolved = distance(vec_i_evolved, vec_n_evolved);
                
                if (dist_evolved > 1e-10) {
                    // Log of divergence rate
                    sum_log_divergence += std::log(dist_evolved / min_dist) / k;
                    count++;
                }
            }
        }

        // Average Lyapunov exponent
        return (count > 0) ? (sum_log_divergence / count) : 0.0;
    }

private:
    std::vector<double> history_;

    struct Vector3 { double x, y, z; };

    Vector3 get_vector(size_t i) {
        return {history_[i], history_[i + LAG], history_[i + 2 * LAG]};
    }

    double distance(Vector3 a, Vector3 b) {
        double d = (a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z);
        return std::sqrt(d);
    }
};

}
}
