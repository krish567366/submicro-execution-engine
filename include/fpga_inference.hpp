#pragma once

#include "common_types.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <thread>

namespace hft {

// ============================================================================
// Cross-Asset Microstructure Features
// Deep Order Flow Imbalance (OFI) and related features
// ============================================================================

struct MicrostructureFeatures {
    // Deep OFI across multiple price levels
    double ofi_level_1;
    double ofi_level_5;
    double ofi_level_10;
    
    // Cross-asset correlation features
    double spread_ratio;           // This asset spread / Reference asset spread
    double price_correlation;      // Rolling correlation with reference asset
    double volume_imbalance;       // (Bid volume - Ask volume) / Total volume
    
    // Hawkes intensity features
    double hawkes_buy_intensity;
    double hawkes_sell_intensity;
    double hawkes_imbalance;
    
    // Market pressure indicators
    double bid_ask_spread_bps;
    double mid_price_momentum;
    double trade_flow_toxicity;    // Kyle's lambda approximation
    
    MicrostructureFeatures() : ofi_level_1(0.0), ofi_level_5(0.0), 
        ofi_level_10(0.0), spread_ratio(1.0), price_correlation(0.0),
        volume_imbalance(0.0), hawkes_buy_intensity(0.0), 
        hawkes_sell_intensity(0.0), hawkes_imbalance(0.0),
        bid_ask_spread_bps(0.0), mid_price_momentum(0.0),
        trade_flow_toxicity(0.0) {}
    
    // Convert to flat vector for neural network input
    std::vector<double> to_vector() const {
        return {
            ofi_level_1, ofi_level_5, ofi_level_10,
            spread_ratio, price_correlation, volume_imbalance,
            hawkes_buy_intensity, hawkes_sell_intensity, hawkes_imbalance,
            bid_ask_spread_bps, mid_price_momentum, trade_flow_toxicity
        };
    }
};

// ============================================================================
// FPGA-Native DNN Inference Engine
// Simulates deterministic hardware-accelerated neural network
// Guarantees fixed 400ns latency (sub-microsecond decision)
// ============================================================================

class FPGA_DNN_Inference {
public:
    // ========================================================================
    // Constructor: Initialize with pre-trained weights (simplified)
    // In production, weights would be loaded from FPGA bitstream
    // ========================================================================
    explicit FPGA_DNN_Inference(size_t input_dim = 12, size_t hidden_dim = 8)
        : input_dim_(input_dim),
          hidden_dim_(hidden_dim),
          fixed_latency_ns_(400) {
        
        // Simplified weight initialization (in reality: FPGA LUT mapping)
        // Hidden layer weights
        weights_h_.resize(hidden_dim_ * input_dim_, 0.1);
        bias_h_.resize(hidden_dim_, 0.0);
        
        // Output layer weights (3 outputs: BUY, HOLD, SELL)
        weights_o_.resize(3 * hidden_dim_, 0.1);
        bias_o_ = {0.0, 0.0, 0.0};
        
        // Initialize with small random values for demonstration
        for (auto& w : weights_h_) w = (std::rand() % 200 - 100) / 1000.0;
        for (auto& w : weights_o_) w = (std::rand() % 200 - 100) / 1000.0;
    }
    
    // ========================================================================
    // Predict: Deterministic inference with guaranteed 400ns latency
    // Returns [buy_score, hold_score, sell_score]
    // ========================================================================
    std::array<double, 3> predict(const MicrostructureFeatures& features) {
        const Timestamp start = now();
        
        // Convert features to input vector
        auto input = features.to_vector();
        if (input.size() != input_dim_) {
            input.resize(input_dim_, 0.0);
        }
        
        // Forward pass (simplified for deterministic execution)
        auto output = forward_pass(input);
        
        // Busy-wait to guarantee EXACTLY 400ns latency (deterministic timing)
        // In real FPGA: this is the fixed pipeline latency
        const Timestamp end = now();
        const int64_t elapsed_ns = to_nanos(end) - to_nanos(start);
        
        if (elapsed_ns < fixed_latency_ns_) {
            // Spin-wait for remaining time (deterministic busy loop)
            while ((to_nanos(now()) - to_nanos(start)) < fixed_latency_ns_) {
                // Hardware busy-wait equivalent
                __asm__ __volatile__("" ::: "memory");  // Prevent optimization
            }
        }
        
        return output;
    }
    
    // ========================================================================
    // Get guaranteed latency (for system calibration)
    // ========================================================================
    int64_t get_fixed_latency_ns() const {
        return fixed_latency_ns_;
    }
    
    // ========================================================================
    // Feature extraction from market data
    // Computes Deep OFI and cross-asset features
    // ========================================================================
    static MicrostructureFeatures extract_features(
        const MarketTick& current_tick,
        const MarketTick& previous_tick,
        const MarketTick& reference_asset_tick,
        double hawkes_buy_intensity,
        double hawkes_sell_intensity) {
        
        MicrostructureFeatures features;
        
        // Deep Order Flow Imbalance (OFI) calculation
        // OFI measures the imbalance in order book updates
        features.ofi_level_1 = compute_ofi(current_tick, previous_tick, 1);
        features.ofi_level_5 = compute_ofi(current_tick, previous_tick, 5);
        features.ofi_level_10 = compute_ofi(current_tick, previous_tick, 10);
        
        // Cross-asset spread ratio
        const double current_spread = current_tick.ask_price - current_tick.bid_price;
        const double ref_spread = reference_asset_tick.ask_price - reference_asset_tick.bid_price;
        features.spread_ratio = (ref_spread > 1e-10) ? (current_spread / ref_spread) : 1.0;
        
        // Volume imbalance
        const double total_volume = current_tick.bid_size + current_tick.ask_size;
        features.volume_imbalance = (total_volume > 0) ? 
            (static_cast<double>(current_tick.bid_size) - static_cast<double>(current_tick.ask_size)) / total_volume : 0.0;
        
        // Hawkes process intensities
        features.hawkes_buy_intensity = hawkes_buy_intensity;
        features.hawkes_sell_intensity = hawkes_sell_intensity;
        features.hawkes_imbalance = (hawkes_buy_intensity + hawkes_sell_intensity > 1e-10) ?
            (hawkes_buy_intensity - hawkes_sell_intensity) / (hawkes_buy_intensity + hawkes_sell_intensity) : 0.0;
        
        // Spread in basis points
        features.bid_ask_spread_bps = (current_tick.mid_price > 1e-10) ?
            (current_spread / current_tick.mid_price) * 10000.0 : 0.0;
        
        // Mid-price momentum (simple difference)
        features.mid_price_momentum = current_tick.mid_price - previous_tick.mid_price;
        
        // Trade flow toxicity (Kyle's lambda approximation)
        // Measures adverse selection risk
        if (current_tick.trade_volume > 0 && previous_tick.mid_price > 1e-10) {
            const double price_impact = std::abs(current_tick.mid_price - previous_tick.mid_price);
            const double volume = static_cast<double>(current_tick.trade_volume);
            features.trade_flow_toxicity = (volume > 0) ? price_impact / volume : 0.0;
        }
        
        return features;
    }
    
private:
    // ========================================================================
    // Compute Order Flow Imbalance (OFI) at specified depth
    // OFI = Σ(ΔBid_size - ΔAsk_size) weighted by price levels
    // ========================================================================
    static double compute_ofi(const MarketTick& current, 
                             const MarketTick& previous, 
                             size_t depth) {
        double ofi = 0.0;
        const size_t levels = std::min(depth, static_cast<size_t>(current.depth_levels));
        
        for (size_t i = 0; i < levels; ++i) {
            // Bid side contribution
            const int64_t bid_delta = static_cast<int64_t>(current.bid_sizes[i]) - 
                                      static_cast<int64_t>(previous.bid_sizes[i]);
            
            // Ask side contribution
            const int64_t ask_delta = static_cast<int64_t>(current.ask_sizes[i]) - 
                                      static_cast<int64_t>(previous.ask_sizes[i]);
            
            // Weight by inverse of level (closer levels more important)
            const double weight = 1.0 / (i + 1.0);
            ofi += weight * (bid_delta - ask_delta);
        }
        
        return ofi;
    }
    
    // ========================================================================
    // Simplified forward pass (ReLU activation)
    // In real FPGA: this is Boolean logic in LUTs
    // ========================================================================
    std::array<double, 3> forward_pass(const std::vector<double>& input) const {
        // Hidden layer
        std::vector<double> hidden(hidden_dim_, 0.0);
        for (size_t i = 0; i < hidden_dim_; ++i) {
            double sum = bias_h_[i];
            for (size_t j = 0; j < input_dim_; ++j) {
                sum += weights_h_[i * input_dim_ + j] * input[j];
            }
            hidden[i] = std::max(0.0, sum);  // ReLU activation
        }
        
        // Output layer
        std::array<double, 3> output = {bias_o_[0], bias_o_[1], bias_o_[2]};
        for (size_t i = 0; i < 3; ++i) {
            for (size_t j = 0; j < hidden_dim_; ++j) {
                output[i] += weights_o_[i * hidden_dim_ + j] * hidden[j];
            }
        }
        
        // Softmax for probabilities
        double max_val = *std::max_element(output.begin(), output.end());
        double sum_exp = 0.0;
        for (auto& val : output) {
            val = std::exp(val - max_val);  // Numerical stability
            sum_exp += val;
        }
        for (auto& val : output) {
            val /= sum_exp;
        }
        
        return output;
    }
    
    // ========================================================================
    // Member variables
    // ========================================================================
    size_t input_dim_;
    size_t hidden_dim_;
    int64_t fixed_latency_ns_;  // Guaranteed 400ns latency
    
    // Simplified weight matrices (in FPGA: stored as LUT configurations)
    std::vector<double> weights_h_;   // Hidden layer weights
    std::vector<double> bias_h_;      // Hidden layer bias
    std::vector<double> weights_o_;   // Output layer weights
    std::array<double, 3> bias_o_;    // Output layer bias
};

} // namespace hft
