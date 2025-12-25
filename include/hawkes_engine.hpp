#pragma once

#include "common_types.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

namespace hft {

// ============================================================================
// Multivariate Hawkes Process with Power-Law Kernel
// Models self-exciting point processes for buy/sell order arrivals
// Intensity: λ_i(t) = μ_i + Σ_j Σ_{t_k < t} α_ij * K(t - t_k)
// Power-Law Kernel: K(τ) = (β + τ)^(-γ) where γ > 1
// ============================================================================

class HawkesIntensityEngine {
public:
    // ========================================================================
    // Constructor
    // ========================================================================
    HawkesIntensityEngine(
        double baseline_buy,          // μ_buy: baseline intensity for buy events
        double baseline_sell,         // μ_sell: baseline intensity for sell events
        double alpha_self,            // α_ii: self-excitation parameter
        double alpha_cross,           // α_ij: cross-excitation parameter
        double power_law_beta,        // β: power-law kernel offset (prevents singularity)
        double power_law_gamma,       // γ: power-law decay exponent (> 1)
        size_t max_history = 1000     // Maximum events to retain in memory
    ) : mu_buy_(baseline_buy),
        mu_sell_(baseline_sell),
        alpha_self_(alpha_self),
        alpha_cross_(alpha_cross),
        beta_(power_law_beta),
        gamma_(power_law_gamma),
        max_history_(max_history),
        current_time_(now()),
        intensity_buy_(baseline_buy),
        intensity_sell_(baseline_sell) {
        
        // Validate parameters
        if (gamma_ <= 1.0) {
            gamma_ = 1.5;  // Ensure convergence
        }
        if (beta_ <= 0.0) {
            beta_ = 1e-6;  // Prevent division by zero
        }
    }
    
    // ========================================================================
    // Update intensity with new market event (O(N) but with efficient pruning)
    // This is called on every trade or significant quote update
    // ========================================================================
    void update(const TradingEvent& event) {
        current_time_ = event.arrival_time;
        
        // Add event to history
        if (event.event_type == Side::BUY) {
            buy_events_.push_back(event);
            if (buy_events_.size() > max_history_) {
                buy_events_.pop_front();
            }
        } else {
            sell_events_.push_back(event);
            if (sell_events_.size() > max_history_) {
                sell_events_.pop_front();
            }
        }
        
        // Recalculate intensities
        recalculate_intensity();
    }
    
    // ========================================================================
    // Get current intensity for buy orders
    // ========================================================================
    double get_buy_intensity() const {
        return intensity_buy_;
    }
    
    // ========================================================================
    // Get current intensity for sell orders
    // ========================================================================
    double get_sell_intensity() const {
        return intensity_sell_;
    }
    
    // ========================================================================
    // Get intensity imbalance (directional signal)
    // Positive = more buy pressure, Negative = more sell pressure
    // ========================================================================
    double get_intensity_imbalance() const {
        const double total = intensity_buy_ + intensity_sell_;
        if (total < 1e-10) return 0.0;
        return (intensity_buy_ - intensity_sell_) / total;
    }
    
    // ========================================================================
    // Predict intensity at future time (for latency compensation)
    // ========================================================================
    double predict_buy_intensity(Duration forecast_horizon) const {
        Timestamp future_time = current_time_ + forecast_horizon;
        return compute_intensity(Side::BUY, future_time);
    }
    
    double predict_sell_intensity(Duration forecast_horizon) const {
        Timestamp future_time = current_time_ + forecast_horizon;
        return compute_intensity(Side::SELL, future_time);
    }
    
    // ========================================================================
    // Reset the engine (clear history)
    // ========================================================================
    void reset() {
        buy_events_.clear();
        sell_events_.clear();
        intensity_buy_ = mu_buy_;
        intensity_sell_ = mu_sell_;
        current_time_ = now();
    }
    
    // ========================================================================
    // Get event counts for diagnostics
    // ========================================================================
    size_t get_buy_event_count() const { return buy_events_.size(); }
    size_t get_sell_event_count() const { return sell_events_.size(); }
    
private:
    // ========================================================================
    // Power-Law Kernel: K(τ) = (β + τ)^(-γ)
    // ========================================================================
    double power_law_kernel(double tau_seconds) const {
        if (tau_seconds < 0.0) return 0.0;
        return std::pow(beta_ + tau_seconds, -gamma_);
    }
    
    // ========================================================================
    // Recalculate intensity based on current event history
    // λ_i(t) = μ_i + Σ_j Σ_{t_k < t} α_ij * K(t - t_k)
    // ========================================================================
    void recalculate_intensity() {
        intensity_buy_ = compute_intensity(Side::BUY, current_time_);
        intensity_sell_ = compute_intensity(Side::SELL, current_time_);
    }
    
    // ========================================================================
    // Compute intensity for a given side at specified time
    // ========================================================================
    double compute_intensity(Side side, Timestamp eval_time) const {
        double intensity = (side == Side::BUY) ? mu_buy_ : mu_sell_;
        
        const int64_t eval_nanos = to_nanos(eval_time);
        
        // Self-excitation from same-side events
        const auto& same_events = (side == Side::BUY) ? buy_events_ : sell_events_;
        for (const auto& evt : same_events) {
            const int64_t event_nanos = to_nanos(evt.arrival_time);
            if (event_nanos < eval_nanos) {
                const double tau_seconds = (eval_nanos - event_nanos) * 1e-9;
                intensity += alpha_self_ * power_law_kernel(tau_seconds);
            }
        }
        
        // Cross-excitation from opposite-side events
        const auto& cross_events = (side == Side::BUY) ? sell_events_ : buy_events_;
        for (const auto& evt : cross_events) {
            const int64_t event_nanos = to_nanos(evt.arrival_time);
            if (event_nanos < eval_nanos) {
                const double tau_seconds = (eval_nanos - event_nanos) * 1e-9;
                intensity += alpha_cross_ * power_law_kernel(tau_seconds);
            }
        }
        
        return std::max(intensity, 1e-10);  // Prevent negative/zero intensity
    }
    
    // ========================================================================
    // Member variables
    // ========================================================================
    double mu_buy_;          // Baseline intensity for buy events
    double mu_sell_;         // Baseline intensity for sell events
    double alpha_self_;      // Self-excitation coefficient
    double alpha_cross_;     // Cross-excitation coefficient
    double beta_;            // Power-law kernel offset
    double gamma_;           // Power-law decay exponent
    size_t max_history_;     // Maximum event history size
    
    Timestamp current_time_;
    double intensity_buy_;   // Current buy intensity
    double intensity_sell_;  // Current sell intensity
    
    // Event history (using deque for efficient pop_front)
    std::deque<TradingEvent> buy_events_;
    std::deque<TradingEvent> sell_events_;
};

} // namespace hft
