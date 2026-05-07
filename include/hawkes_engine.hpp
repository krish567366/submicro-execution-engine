#pragma once

#include "common_types.hpp"
#include "spin_loop_engine.hpp" // for ExpLookupTable
#include <vector>
#include <cmath>

namespace hft {
namespace quant {

/**
 * Fast Hawkes Process Kernel
 * 
 * Research: "Modeling High-Frequency Limit Order Book Dynamics with Hawkes Processes"
 * 
 * Concept:
 * Self-exciting point process where past events increase the probability of future events.
 * Used to model "Market Excitement" or "Flash Crash" probability.
 * 
 * Calculation:
 * Intensity(t) = mu + sum( alpha * exp( -beta * (t - t_i) ) )
 * 
 * Recursive Formulation (O(1) update):
 * Intensity(t_new) = (Intensity(t_prev) - mu) * exp( -beta * (t_new - t_prev) ) + alpha + mu
 * 
 * This allows real-time tracking of "Cluster Arrival" intensity with single multiplication.
 */
class HawkesIntensity {
public:
    HawkesIntensity(double mu, double alpha, double beta)
        : mu_(mu), alpha_(alpha), beta_(beta),
          last_intensity_(mu), last_timestamp_(0) {}

    // O(1) Update on new event
    inline double update(uint64_t now_ns) {
        if (last_timestamp_ == 0) {
            last_timestamp_ = now_ns;
            last_intensity_ = mu_ + alpha_;
            return last_intensity_;
        }

        double dt_sec = (now_ns - last_timestamp_) * 1e-9;
        last_timestamp_ = now_ns;

        // Use fast_exp from spin_loop_engine (pre-computed LUT)
        double decay = hft::spin_loop::fast_exp(-beta_ * dt_sec);
        
        last_intensity_ = (last_intensity_ - mu_) * decay + alpha_ + mu_;
        return last_intensity_;
    }

    // Decay query (no new event)
    inline double current_value(uint64_t now_ns) const {
        double dt_sec = (now_ns - last_timestamp_) * 1e-9;
        
        // Fast decay check
        if (dt_sec > 1.0) return mu_; // Decayed to baseline
        
        double decay = hft::spin_loop::fast_exp(-beta_ * dt_sec);
        return (last_intensity_ - mu_) * decay + mu_;
    }

private:
    double mu_;    // Baseline intensity
    double alpha_; // Excitation (jump size)
    double beta_;  // Decay rate
    
    double last_intensity_;
    uint64_t last_timestamp_;
};

}
}