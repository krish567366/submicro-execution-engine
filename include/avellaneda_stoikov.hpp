#pragma once

#include "common_types.hpp"
#include <cmath>
#include <algorithm>

namespace hft {

// ============================================================================
// Dynamic Market-Making Strategy
// Implements Avellaneda-Stoikov (AS) model with latency-aware HJB extension
// 
// Key innovations:
// 1. Inventory skew: asymmetric quotes based on position
// 2. Reservation price: dynamic mid-price adjustment for risk management
// 3. Latency cost: incorporates operational delay into profitability check
// 
// Model:
// - Reservation price: r = s - q*γ*σ²*(T-t)
// - Optimal spread: δ_a + δ_b = γ*σ²*(T-t) + (2/γ)*ln(1 + γ/k)
// - Bid: r - δ_b,  Ask: r + δ_a
// - Latency constraint: profit > latency_cost
// ============================================================================

class DynamicMMStrategy {
public:
    // ========================================================================
    // Constructor
    // ========================================================================
    DynamicMMStrategy(
        double risk_aversion,           // γ: risk aversion parameter
        double volatility,              // σ: price volatility (daily, annualized)
        double time_horizon,            // T: time horizon in seconds
        double order_arrival_rate,      // k: market order arrival rate
        double tick_size,               // Minimum price increment
        int64_t system_latency_ns       // Round-trip latency in nanoseconds
    ) : gamma_(risk_aversion),
        sigma_(volatility),
        time_horizon_(time_horizon),
        k_(order_arrival_rate),
        tick_size_(tick_size),
        system_latency_ns_(system_latency_ns),
        min_spread_(tick_size * 2.0),   // Minimum spread = 2 ticks
        max_inventory_(1000) {
        
        // Precompute constants
        sigma_squared_ = sigma_ * sigma_;
        
        // Annualized volatility to per-second
        sigma_per_second_ = sigma_ / std::sqrt(252.0 * 6.5 * 3600.0);
        sigma_squared_per_second_ = sigma_per_second_ * sigma_per_second_;
    }
    
    // ========================================================================
    // Calculate optimal bid/ask quotes with latency awareness
    // Returns: QuotePair with skewed quotes based on inventory
    // ========================================================================
    QuotePair calculate_quotes(
        double current_mid_price,
        int64_t current_inventory,      // Positive = long, Negative = short
        double time_remaining_seconds,
        double latency_cost_per_trade
    ) const {
        QuotePair quotes;
        quotes.mid_price = current_mid_price;
        
        // Validate inputs
        if (current_mid_price <= 0.0 || time_remaining_seconds <= 0.0) {
            // Return no quotes if invalid
            quotes.bid_price = 0.0;
            quotes.ask_price = 0.0;
            quotes.spread = 0.0;
            return quotes;
        }
        
        // ====================================================================
        // Step 1: Calculate reservation price (inventory adjustment)
        // r = s - q * γ * σ² * (T-t)
        // ====================================================================
        const double inventory_penalty = static_cast<double>(current_inventory) * 
                                         gamma_ * 
                                         sigma_squared_per_second_ * 
                                         time_remaining_seconds;
        
        const double reservation_price = current_mid_price - inventory_penalty;
        
        // ====================================================================
        // Step 2: Calculate optimal spread
        // δ_a + δ_b = γ*σ²*(T-t) + (2/γ)*ln(1 + γ/k)
        // ====================================================================
        const double time_component = gamma_ * sigma_squared_per_second_ * time_remaining_seconds;
        const double arrival_component = (2.0 / gamma_) * std::log(1.0 + gamma_ / k_);
        
        double total_spread = time_component + arrival_component;
        
        // Apply minimum spread constraint
        total_spread = std::max(total_spread, min_spread_);
        
        // ====================================================================
        // Step 3: Incorporate latency cost
        // Only quote if expected profit > latency cost
        // ====================================================================
        const double half_spread = total_spread / 2.0;
        
        // Expected profit per filled order ≈ half_spread
        // If latency cost exceeds expected profit, widen spread or don't quote
        if (latency_cost_per_trade > half_spread) {
            // Widen spread to compensate for latency
            total_spread += 2.0 * (latency_cost_per_trade - half_spread);
        }
        
        // ====================================================================
        // Step 4: Calculate asymmetric quotes (inventory skew)
        // Skew quotes to encourage inventory mean-reversion
        // ====================================================================
        const double inventory_skew_factor = calculate_inventory_skew(current_inventory);
        
        // Allocate spread asymmetrically based on inventory
        // If long (positive inventory): widen ask, tighten bid (encourage selling)
        // If short (negative inventory): widen bid, tighten ask (encourage buying)
        const double bid_spread = half_spread * (1.0 - inventory_skew_factor);
        const double ask_spread = half_spread * (1.0 + inventory_skew_factor);
        
        // ====================================================================
        // Step 5: Final quote calculation
        // ====================================================================
        quotes.bid_price = round_to_tick(reservation_price - bid_spread);
        quotes.ask_price = round_to_tick(reservation_price + ask_spread);
        
        // Ensure bid < ask
        if (quotes.bid_price >= quotes.ask_price) {
            quotes.bid_price = quotes.ask_price - tick_size_;
        }
        
        quotes.spread = quotes.ask_price - quotes.bid_price;
        
        // Quote sizes (could be made inventory-dependent)
        quotes.bid_size = calculate_quote_size(Side::BUY, current_inventory);
        quotes.ask_size = calculate_quote_size(Side::SELL, current_inventory);
        
        quotes.generated_at = now();
        
        return quotes;
    }
    
    // ========================================================================
    // Calculate latency cost per trade
    // Cost = expected slippage during system latency period
    // ========================================================================
    double calculate_latency_cost(double current_volatility, double mid_price) const {
        // Convert nanoseconds to seconds
        const double latency_seconds = system_latency_ns_ * 1e-9;
        
        // Expected price movement during latency: σ * √(Δt)
        const double expected_slippage = current_volatility * std::sqrt(latency_seconds);
        
        // Cost as fraction of mid-price
        return expected_slippage * mid_price;
    }
    
    // ========================================================================
    // Check if quoting is profitable given latency
    // ========================================================================
    bool should_quote(double expected_spread, double latency_cost) const {
        // Only quote if expected profit exceeds latency cost
        const double expected_profit = expected_spread / 2.0;
        return expected_profit > (latency_cost * 1.1);  // 10% margin of safety
    }
    
    // ========================================================================
    // Update risk aversion (dynamic adjustment)
    // ========================================================================
    void set_risk_aversion(double new_gamma) {
        gamma_ = new_gamma;
    }
    
    // ========================================================================
    // Update volatility estimate (from realized or implied vol)
    // ========================================================================
    void set_volatility(double new_sigma) {
        sigma_ = new_sigma;
        sigma_squared_ = sigma_ * sigma_;
        sigma_per_second_ = sigma_ / std::sqrt(252.0 * 6.5 * 3600.0);
        sigma_squared_per_second_ = sigma_per_second_ * sigma_per_second_;
    }
    
    // ========================================================================
    // Getters
    // ========================================================================
    double get_risk_aversion() const { return gamma_; }
    double get_volatility() const { return sigma_; }
    int64_t get_system_latency_ns() const { return system_latency_ns_; }
    
private:
    // ========================================================================
    // Calculate inventory skew factor [-1, 1]
    // Positive inventory → positive skew (widen ask)
    // Negative inventory → negative skew (widen bid)
    // ========================================================================
    double calculate_inventory_skew(int64_t inventory) const {
        // Normalize by max inventory and apply tanh for smooth nonlinearity
        const double normalized_inventory = static_cast<double>(inventory) / 
                                            static_cast<double>(max_inventory_);
        
        // Skew factor: tanh gives asymptotic behavior at extremes
        return std::tanh(normalized_inventory * 2.0);  // [-1, 1]
    }
    
    // ========================================================================
    // Calculate quote size based on inventory
    // Larger size when trying to reduce inventory
    // ========================================================================
    double calculate_quote_size(Side side, int64_t inventory) const {
        const double base_size = 100.0;  // Base quote size
        
        // If long, increase ask size; if short, increase bid size
        if ((side == Side::SELL && inventory > 0) || 
            (side == Side::BUY && inventory < 0)) {
            // Encourage inventory reduction
            const double inventory_ratio = std::abs(static_cast<double>(inventory)) / 
                                          static_cast<double>(max_inventory_);
            return base_size * (1.0 + inventory_ratio);
        }
        
        return base_size;
    }
    
    // ========================================================================
    // Round price to nearest tick
    // ========================================================================
    double round_to_tick(double price) const {
        return std::round(price / tick_size_) * tick_size_;
    }
    
    // ========================================================================
    // Member variables
    // ========================================================================
    double gamma_;                  // Risk aversion parameter
    double sigma_;                  // Volatility (annualized)
    double sigma_squared_;
    double sigma_per_second_;
    double sigma_squared_per_second_;
    double time_horizon_;           // Time horizon (seconds)
    double k_;                      // Order arrival rate
    double tick_size_;              // Minimum price increment
    int64_t system_latency_ns_;     // System latency in nanoseconds
    double min_spread_;             // Minimum spread constraint
    int64_t max_inventory_;         // Maximum position limit
};

} // namespace hft
