// smart_order_router.hpp
// Latency-Aware Smart Order Router (SOR)
//
// PURPOSE:
// - Route orders to optimal venue based on price, liquidity, AND latency
// - Integrate HJB latency cost with real-time network performance
// - Prevent routing to venues experiencing transient network spikes
// - Maximize fill probability while respecting latency budget
//
// KEY INNOVATION:
// Traditional SORs only consider price and liquidity. This SOR adds a third
// dimension: LATENCY BUDGET CHECK. Before routing, we verify that:
//
//   network_latency(venue) < latency_budget(HJB model)
//
// This ensures theoretical latency costs (from Avellaneda-Stoikov) align
// with real-world network conditions, making execution robust to:
// - Network congestion spikes
// - Venue-specific routing issues
// - Cross-datacenter latency variations
// - Market volatility-driven urgency changes
//
// LATENCY BUDGET CALCULATION:
// The HJB/Avellaneda-Stoikov model computes a latency budget based on:
// - Expected profit from spread capture
// - Current volatility (higher vol = tighter budget)
// - Position inventory (larger position = more urgency)
// - Market regime (stressed markets = faster execution needed)
//
// Formula: latency_budget = (expected_profit / volatility) / urgency_multiplier
//
// NETWORK LATENCY MEASUREMENT:
// - Heartbeat packets sent to each venue every 100ms
// - Round-trip time (RTT) measured with microsecond precision
// - Exponential moving average (EMA) for noise reduction
// - Spike detection: reject venues with >2σ latency increase

#pragma once

#include "common_types.hpp"
#include "avellaneda_stoikov.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <optional>
#include <algorithm>
#include <cmath>

// Use hft namespace types
using hft::Timestamp;
using hft::Duration;
using hft::MarketRegime;
using hft::now;
using hft::to_nanos;

// Use Dynamic Market Making Strategy from hft namespace
using hft::DynamicMMStrategy;

// Trading venue information
struct VenueInfo {
    std::string venue_id;           // "BINANCE", "COINBASE", "KRAKEN", etc.
    std::string venue_name;         // Human-readable name
    bool is_active;                 // Venue accepting orders?
    
    // Network connectivity
    std::string endpoint;           // IP:port or hostname
    double baseline_latency_us;     // Expected RTT under normal conditions
    
    // Venue characteristics
    double maker_fee_bps;           // Maker fee (basis points, e.g., -2.5 = rebate)
    double taker_fee_bps;           // Taker fee (basis points, e.g., 7.5)
    double min_order_size;          // Minimum order size
    double max_order_size;          // Maximum order size
    
    // Liquidity statistics (updated periodically)
    double typical_bid_depth;       // Average bid liquidity (contracts)
    double typical_ask_depth;       // Average ask liquidity (contracts)
    double fill_rate;               // Historical fill rate (0.0 - 1.0)
};

// Real-time venue state (network measurements)
struct VenueState {
    Timestamp last_heartbeat_sent;
    Timestamp last_heartbeat_received;
    
    // Latency measurements (microseconds)
    double current_rtt_us;          // Most recent round-trip time
    double ema_rtt_us;              // Exponential moving average
    double std_dev_rtt_us;          // Standard deviation (for spike detection)
    
    // Health indicators
    bool is_connected;              // Received heartbeat in last 1 second?
    uint64_t consecutive_timeouts;  // Consecutive missed heartbeats
    uint64_t total_heartbeats_sent;
    uint64_t total_heartbeats_received;
    
    // Order execution statistics
    uint64_t orders_sent;
    uint64_t orders_filled;
    uint64_t orders_rejected;
    uint64_t orders_timeout;
};

// Order routing decision
struct RoutingDecision {
    std::string selected_venue;
    double expected_latency_us;
    double latency_budget_us;
    double price_quality;           // How good is the price? (0.0 - 1.0)
    double latency_quality;         // How good is the latency? (0.0 - 1.0)
    double liquidity_quality;       // How good is the liquidity? (0.0 - 1.0)
    double composite_score;         // Weighted score for decision
    std::string rejection_reason;   // If no venue selected
};

// Routing strategy configuration
struct RoutingConfig {
    // Latency budget parameters
    double latency_safety_margin;   // Multiply budget by this (e.g., 0.8 = 80% of budget)
    double latency_spike_threshold; // Reject if latency > EMA + (N * std_dev)
    
    // Scoring weights (must sum to 1.0)
    double price_weight;            // Weight for price quality (0.0 - 1.0)
    double latency_weight;          // Weight for latency quality (0.0 - 1.0)
    double liquidity_weight;        // Weight for liquidity quality (0.0 - 1.0)
    
    // Venue selection thresholds
    double min_fill_rate;           // Minimum acceptable fill rate (e.g., 0.85)
    double min_composite_score;     // Minimum score to route (e.g., 0.6)
    
    // Heartbeat configuration
    int64_t heartbeat_interval_ms;  // How often to send heartbeats (e.g., 100ms)
    int64_t heartbeat_timeout_ms;   // Declare timeout after this (e.g., 1000ms)
    double rtt_ema_alpha;           // EMA smoothing factor (e.g., 0.2)
};

// ============================================================================
// Smart Order Router with Latency Budget Integration
// ============================================================================

class SmartOrderRouter {
public:
    // ========================================================================
    // Construction & Initialization
    // ========================================================================
    
    explicit SmartOrderRouter(const RoutingConfig& config = default_config())
        : config_(config)
        , as_model_(nullptr)
    {
    }
    
    // Initialize with Dynamic MM Strategy for latency budget calculation
    bool initialize(DynamicMMStrategy* as_model) {
        if (!as_model) {
            return false;
        }
        
        as_model_ = as_model;
        
        // Initialize venues (would be loaded from config in production)
        initialize_venues();
        
        return true;
    }
    
    // ========================================================================
    // Venue Management
    // ========================================================================
    
    // Add venue to routing pool
    void add_venue(const VenueInfo& venue) {
        venues_[venue.venue_id] = venue;
        
        // Initialize state
        VenueState state;
        state.last_heartbeat_sent = Timestamp{};
        state.last_heartbeat_received = Timestamp{};
        state.current_rtt_us = venue.baseline_latency_us;
        state.ema_rtt_us = venue.baseline_latency_us;
        state.std_dev_rtt_us = venue.baseline_latency_us * 0.1;  // 10% baseline variance
        state.is_connected = true;  // Assume connected initially
        state.consecutive_timeouts = 0;
        state.total_heartbeats_sent = 0;
        state.total_heartbeats_received = 0;
        state.orders_sent = 0;
        state.orders_filled = 0;
        state.orders_rejected = 0;
        state.orders_timeout = 0;
        
        venue_states_[venue.venue_id] = state;
    }
    
    // Remove venue from routing pool
    void remove_venue(const std::string& venue_id) {
        venues_.erase(venue_id);
        venue_states_.erase(venue_id);
    }
    
    // Get all active venues
    std::vector<std::string> get_active_venues() const {
        std::vector<std::string> active;
        for (const auto& [venue_id, venue] : venues_) {
            if (venue.is_active && venue_states_.at(venue_id).is_connected) {
                active.push_back(venue_id);
            }
        }
        return active;
    }
    
    // ========================================================================
    // Network Latency Monitoring (Heartbeat System)
    // ========================================================================
    
    // Send heartbeat to venue (call periodically, e.g., every 100ms)
    void send_heartbeat(const std::string& venue_id, Timestamp now) {
        auto it = venue_states_.find(venue_id);
        if (it == venue_states_.end()) {
            return;
        }
        
        VenueState& state = it->second;
        state.last_heartbeat_sent = now;
        state.total_heartbeats_sent++;
        
        // In production: actually send heartbeat packet to venue
        // Example: send_udp_packet(venue.endpoint, heartbeat_payload);
    }
    
    // Process heartbeat response from venue
    void receive_heartbeat(const std::string& venue_id, Timestamp sent_time, Timestamp received_time) {
        auto it = venue_states_.find(venue_id);
        if (it == venue_states_.end()) {
            return;
        }
        
        VenueState& state = it->second;
        state.last_heartbeat_received = received_time;
        state.total_heartbeats_received++;
        state.consecutive_timeouts = 0;  // Reset timeout counter
        state.is_connected = true;
        
        // Calculate round-trip time (microseconds)
        const double rtt_ns = static_cast<double>((received_time - sent_time).count());
        const double rtt_us = rtt_ns / 1000.0;
        state.current_rtt_us = rtt_us;
        
        // Update exponential moving average
        const double alpha = config_.rtt_ema_alpha;
        state.ema_rtt_us = alpha * rtt_us + (1.0 - alpha) * state.ema_rtt_us;
        
        // Update standard deviation (simplified online calculation)
        const double delta = rtt_us - state.ema_rtt_us;
        state.std_dev_rtt_us = std::sqrt(
            alpha * delta * delta + (1.0 - alpha) * state.std_dev_rtt_us * state.std_dev_rtt_us
        );
    }
    
    // Check for heartbeat timeouts (call periodically)
    void check_heartbeat_timeouts(Timestamp now) {
        const int64_t timeout_ns = config_.heartbeat_timeout_ms * 1'000'000;
        
        for (auto& [venue_id, state] : venue_states_) {
            if (state.last_heartbeat_sent == Timestamp{}) {
                continue;  // Never sent heartbeat yet
            }
            
            const Duration time_since_sent_duration = now - state.last_heartbeat_sent;
            const int64_t time_since_sent = std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_sent_duration).count();
            
            if (time_since_sent > timeout_ns && state.is_connected) {
                // Timeout detected
                state.consecutive_timeouts++;
                
                if (state.consecutive_timeouts >= 3) {
                    // Mark as disconnected after 3 consecutive timeouts
                    state.is_connected = false;
                }
            }
        }
    }
    
    // ========================================================================
    // Latency Budget Calculation (HJB/Avellaneda-Stoikov Integration)
    // ========================================================================
    
    // Calculate latency budget based on market conditions and HJB model
    double calculate_latency_budget(
        double mid_price,
        double current_volatility,
        int32_t current_position,
        int32_t order_size,
        MarketRegime regime
    ) const {
        if (!as_model_) {
            // No AS model, use conservative default (1ms)
            return 1000.0;
        }
        
        // Get optimal quotes from Dynamic MM Strategy
        // This internally calculates expected profit vs. latency cost tradeoff
        auto quotes = as_model_->calculate_quotes(
            mid_price,
            current_position,
            600.0,  // time_remaining_seconds (10 minutes)
            0.0     // latency_cost_per_trade (calculated below)
        );
        
        // Calculate latency cost from AS model
        const double latency_cost = as_model_->calculate_latency_cost(
            current_volatility,
            mid_price
        );
        
        // Expected profit from spread capture (half-spread per side)
        const double bid_spread = mid_price - quotes.bid_price;
        const double ask_spread = quotes.ask_price - mid_price;
        const double expected_profit = (order_size > 0) ? ask_spread : bid_spread;
        
        // Latency budget: time we can afford to wait before profit erodes
        // Formula: budget = (profit / volatility) / urgency
        
        // Urgency multiplier based on market regime
        double urgency_multiplier = 1.0;
        switch (regime) {
            case MarketRegime::NORMAL:
                urgency_multiplier = 1.0;   // Normal urgency
                break;
            case MarketRegime::ELEVATED_VOLATILITY:
                urgency_multiplier = 1.5;   // 50% more urgent
                break;
            case MarketRegime::HIGH_STRESS:
                urgency_multiplier = 3.0;   // 3x more urgent
                break;
            case MarketRegime::HALTED:
                urgency_multiplier = 10.0;  // Extremely urgent (or don't trade)
                break;
        }
        
        // Position urgency: larger positions need faster execution
        const double position_ratio = static_cast<double>(current_position) / 1000.0;
        const double position_urgency = 1.0 + std::abs(position_ratio);
        urgency_multiplier *= position_urgency;
        
        // Calculate budget (microseconds)
        double latency_budget_us;
        
        if (expected_profit > latency_cost * 1.1) {
            // Profitable after latency cost, calculate time budget
            const double profit_margin = expected_profit - latency_cost;
            
            // Convert profit margin to time budget
            // Higher volatility = less time available
            // Formula: budget ∝ profit / (volatility * urgency)
            latency_budget_us = (profit_margin / current_volatility) * 
                               (1000.0 / urgency_multiplier);
            
            // Clamp to reasonable range [100us, 10ms]
            latency_budget_us = std::clamp(latency_budget_us, 100.0, 10000.0);
        } else {
            // Not profitable after latency cost, use minimal budget
            latency_budget_us = 100.0;  // 100 microseconds minimum
        }
        
        // Apply safety margin (e.g., use 80% of theoretical budget)
        latency_budget_us *= config_.latency_safety_margin;
        
        return latency_budget_us;
    }
    
    // ========================================================================
    // Smart Order Routing Decision
    // ========================================================================
    
    // Route order to optimal venue with latency budget check
    RoutingDecision route_order(
        double mid_price,
        double current_volatility,
        int32_t current_position,
        int32_t order_size,
        MarketRegime regime,
        const std::unordered_map<std::string, double>& venue_prices  // venue_id -> best_bid/ask
    ) {
        RoutingDecision decision;
        decision.selected_venue = "";
        decision.rejection_reason = "";
        
        // Step 1: Calculate latency budget from HJB model
        decision.latency_budget_us = calculate_latency_budget(
            mid_price,
            current_volatility,
            current_position,
            order_size,
            regime
        );
        
        // Step 2: Filter venues by latency budget and connectivity
        std::vector<std::string> candidate_venues;
        
        for (const auto& [venue_id, venue] : venues_) {
            if (!venue.is_active) {
                continue;  // Venue not active
            }
            
            const auto& state = venue_states_.at(venue_id);
            
            // Check 1: Is venue connected?
            if (!state.is_connected) {
                continue;
            }
            
            // Check 2: Does venue meet latency budget?
            const double venue_latency = state.ema_rtt_us;
            if (venue_latency > decision.latency_budget_us) {
                continue;  // Exceeds latency budget
            }
            
            // Check 3: Is latency stable (no spikes)?
            const double spike_threshold = state.ema_rtt_us + 
                                          (config_.latency_spike_threshold * state.std_dev_rtt_us);
            if (state.current_rtt_us > spike_threshold) {
                continue;  // Current latency is spiking
            }
            
            // Check 4: Does venue have sufficient fill rate?
            const double fill_rate = (state.orders_sent > 0) ? 
                static_cast<double>(state.orders_filled) / state.orders_sent : 
                venue.fill_rate;  // Use historical if no recent data
            
            if (fill_rate < config_.min_fill_rate) {
                continue;  // Fill rate too low
            }
            
            // Check 5: Does venue support order size?
            const double abs_order_size = std::abs(static_cast<double>(order_size));
            if (abs_order_size < venue.min_order_size || 
                abs_order_size > venue.max_order_size) {
                continue;  // Order size out of range
            }
            
            // Venue passes all filters
            candidate_venues.push_back(venue_id);
        }
        
        // Step 3: No candidates? Return rejection
        if (candidate_venues.empty()) {
            decision.rejection_reason = "No venues meet latency budget (" + 
                                       std::to_string(decision.latency_budget_us) + 
                                       " us) and connectivity requirements";
            return decision;
        }
        
        // Step 4: Score each candidate venue
        std::vector<std::pair<std::string, double>> scored_venues;
        
        for (const auto& venue_id : candidate_venues) {
            const auto& venue = venues_.at(venue_id);
            const auto& state = venue_states_.at(venue_id);
            
            // Price quality: how close to best price?
            double price_quality = 0.0;
            if (venue_prices.count(venue_id) > 0) {
                const double venue_price = venue_prices.at(venue_id);
                
                // Find best price across all venues
                double best_price = venue_price;
                for (const auto& [vid, price] : venue_prices) {
                    if (order_size > 0) {
                        best_price = std::min(best_price, price);  // Buying: lower is better
                    } else {
                        best_price = std::max(best_price, price);  // Selling: higher is better
                    }
                }
                
                // Quality: 1.0 if best price, 0.0 if 1% worse than best
                const double price_diff = (order_size > 0) ? 
                    (venue_price - best_price) / best_price :
                    (best_price - venue_price) / best_price;
                
                price_quality = std::max(0.0, 1.0 - (price_diff * 100.0));  // 1% = 0.0 quality
            } else {
                price_quality = 0.5;  // No price data, neutral quality
            }
            
            // Latency quality: how fast compared to budget?
            const double latency_ratio = state.ema_rtt_us / decision.latency_budget_us;
            const double latency_quality = std::max(0.0, 1.0 - latency_ratio);
            
            // Liquidity quality: sufficient depth?
            const double required_liquidity = std::abs(static_cast<double>(order_size));
            const double available_liquidity = (order_size > 0) ? 
                venue.typical_ask_depth : venue.typical_bid_depth;
            const double liquidity_ratio = std::min(1.0, available_liquidity / required_liquidity);
            const double liquidity_quality = liquidity_ratio;
            
            // Composite score (weighted average)
            const double composite_score = 
                config_.price_weight * price_quality +
                config_.latency_weight * latency_quality +
                config_.liquidity_weight * liquidity_quality;
            
            scored_venues.push_back({venue_id, composite_score});
        }
        
        // Step 5: Select venue with highest composite score
        auto best_venue = std::max_element(
            scored_venues.begin(),
            scored_venues.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        );
        
        if (best_venue == scored_venues.end() || best_venue->second < config_.min_composite_score) {
            decision.rejection_reason = "No venues meet minimum composite score (" + 
                                       std::to_string(config_.min_composite_score) + ")";
            return decision;
        }
        
        // Step 6: Populate decision
        decision.selected_venue = best_venue->first;
        decision.composite_score = best_venue->second;
        decision.expected_latency_us = venue_states_.at(best_venue->first).ema_rtt_us;
        
        // Recalculate individual quality metrics for selected venue
        const auto& venue = venues_.at(best_venue->first);
        const auto& state = venue_states_.at(best_venue->first);
        
        if (venue_prices.count(best_venue->first) > 0) {
            const double venue_price = venue_prices.at(best_venue->first);
            double best_price = venue_price;
            for (const auto& [vid, price] : venue_prices) {
                if (order_size > 0) {
                    best_price = std::min(best_price, price);
                } else {
                    best_price = std::max(best_price, price);
                }
            }
            const double price_diff = (order_size > 0) ? 
                (venue_price - best_price) / best_price :
                (best_price - venue_price) / best_price;
            decision.price_quality = std::max(0.0, 1.0 - (price_diff * 100.0));
        }
        
        decision.latency_quality = std::max(0.0, 1.0 - (state.ema_rtt_us / decision.latency_budget_us));
        
        const double required_liquidity = std::abs(static_cast<double>(order_size));
        const double available_liquidity = (order_size > 0) ? 
            venue.typical_ask_depth : venue.typical_bid_depth;
        decision.liquidity_quality = std::min(1.0, available_liquidity / required_liquidity);
        
        return decision;
    }
    
    // ========================================================================
    // Order Execution Feedback (for statistics)
    // ========================================================================
    
    // Record order execution result for statistics
    void record_order_result(const std::string& venue_id, bool filled, bool timeout) {
        auto it = venue_states_.find(venue_id);
        if (it == venue_states_.end()) {
            return;
        }
        
        VenueState& state = it->second;
        state.orders_sent++;
        
        if (filled) {
            state.orders_filled++;
        } else if (timeout) {
            state.orders_timeout++;
        } else {
            state.orders_rejected++;
        }
    }
    
    // ========================================================================
    // Monitoring & Diagnostics
    // ========================================================================
    
    // Get venue state for monitoring
    std::optional<VenueState> get_venue_state(const std::string& venue_id) const {
        auto it = venue_states_.find(venue_id);
        if (it != venue_states_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // Get all venue states
    const std::unordered_map<std::string, VenueState>& get_all_venue_states() const {
        return venue_states_;
    }

private:
    // ========================================================================
    // Default Configuration
    // ========================================================================
    
    static RoutingConfig default_config() {
        RoutingConfig config;
        
        // Latency budget parameters
        config.latency_safety_margin = 0.8;      // Use 80% of theoretical budget
        config.latency_spike_threshold = 2.0;    // Reject if >2σ above EMA
        
        // Scoring weights (must sum to 1.0)
        config.price_weight = 0.5;               // 50% weight on price
        config.latency_weight = 0.3;             // 30% weight on latency
        config.liquidity_weight = 0.2;           // 20% weight on liquidity
        
        // Venue selection thresholds
        config.min_fill_rate = 0.85;             // 85% minimum fill rate
        config.min_composite_score = 0.6;        // 0.6 minimum composite score
        
        // Heartbeat configuration
        config.heartbeat_interval_ms = 100;      // Send heartbeat every 100ms
        config.heartbeat_timeout_ms = 1000;      // Timeout after 1 second
        config.rtt_ema_alpha = 0.2;              // EMA smoothing (20% new, 80% old)
        
        return config;
    }
    
    // ========================================================================
    // Venue Initialization (Example Venues)
    // ========================================================================
    
    void initialize_venues() {
        // In production, load from configuration file or database
        
        // Example: Binance
        VenueInfo binance;
        binance.venue_id = "BINANCE";
        binance.venue_name = "Binance";
        binance.is_active = true;
        binance.endpoint = "api.binance.com:443";
        binance.baseline_latency_us = 500.0;     // 500us baseline
        binance.maker_fee_bps = -1.0;            // -0.01% maker rebate
        binance.taker_fee_bps = 4.0;             // 0.04% taker fee
        binance.min_order_size = 0.001;
        binance.max_order_size = 10000.0;
        binance.typical_bid_depth = 5000.0;
        binance.typical_ask_depth = 5000.0;
        binance.fill_rate = 0.95;
        add_venue(binance);
        
        // Example: Coinbase
        VenueInfo coinbase;
        coinbase.venue_id = "COINBASE";
        coinbase.venue_name = "Coinbase Pro";
        coinbase.is_active = true;
        coinbase.endpoint = "api.pro.coinbase.com:443";
        coinbase.baseline_latency_us = 800.0;    // 800us baseline
        coinbase.maker_fee_bps = 0.0;            // 0% maker fee
        coinbase.taker_fee_bps = 5.0;            // 0.05% taker fee
        coinbase.min_order_size = 0.01;
        coinbase.max_order_size = 5000.0;
        coinbase.typical_bid_depth = 3000.0;
        coinbase.typical_ask_depth = 3000.0;
        coinbase.fill_rate = 0.90;
        add_venue(coinbase);
        
        // Example: Kraken
        VenueInfo kraken;
        kraken.venue_id = "KRAKEN";
        kraken.venue_name = "Kraken";
        kraken.is_active = true;
        kraken.endpoint = "api.kraken.com:443";
        kraken.baseline_latency_us = 1200.0;     // 1.2ms baseline
        kraken.maker_fee_bps = 0.0;              // 0% maker fee
        kraken.taker_fee_bps = 6.0;              // 0.06% taker fee
        kraken.min_order_size = 0.01;
        kraken.max_order_size = 3000.0;
        kraken.typical_bid_depth = 2000.0;
        kraken.typical_ask_depth = 2000.0;
        kraken.fill_rate = 0.88;
        add_venue(kraken);
    }
    
    // ========================================================================
    // Member Variables
    // ========================================================================
    
    RoutingConfig config_;
    DynamicMMStrategy* as_model_;  // For latency budget calculation
    
    // Venue information and state
    std::unordered_map<std::string, VenueInfo> venues_;
    std::unordered_map<std::string, VenueState> venue_states_;
};

// ============================================================================
// Integration Example (for main.cpp)
// ============================================================================
//
// // Initialize Avellaneda-Stoikov model
// AvellanedaStoikov as_model(0.1, 0.5, 1.5);
//
// // Initialize Smart Order Router with AS integration
// SmartOrderRouter sor;
// sor.initialize(&as_model);
//
// // Heartbeat loop (every 100ms)
// std::thread heartbeat_thread([&]() {
//     while (running) {
//         Timestamp now = current_timestamp();
//         
//         for (const auto& venue_id : sor.get_active_venues()) {
//             sor.send_heartbeat(venue_id, now);
//         }
//         
//         sor.check_heartbeat_timeouts(now);
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// });
//
// // Trading loop
// while (running) {
//     // Get market data
//     double mid_price = 50000.0;
//     double volatility = 0.5;
//     int32_t position = 100;
//     int32_t order_size = 10;
//     MarketRegime regime = MarketRegime::NORMAL;
//     
//     // Get best prices from each venue
//     std::unordered_map<std::string, double> venue_prices;
//     venue_prices["BINANCE"] = 50001.0;
//     venue_prices["COINBASE"] = 50002.5;
//     venue_prices["KRAKEN"] = 50003.0;
//     
//     // Route order with latency budget check
//     auto decision = sor.route_order(
//         mid_price,
//         volatility,
//         position,
//         order_size,
//         regime,
//         venue_prices
//     );
//     
//     if (!decision.selected_venue.empty()) {
//         std::cout << "Routing to: " << decision.selected_venue << std::endl;
//         std::cout << "  Latency Budget: " << decision.latency_budget_us << " us" << std::endl;
//         std::cout << "  Expected Latency: " << decision.expected_latency_us << " us" << std::endl;
//         std::cout << "  Composite Score: " << decision.composite_score << std::endl;
//         
//         // Send order to venue
//         bool filled = send_order(decision.selected_venue, order_size);
//         sor.record_order_result(decision.selected_venue, filled, false);
//     } else {
//         std::cout << "Order rejected: " << decision.rejection_reason << std::endl;
//     }
// }

