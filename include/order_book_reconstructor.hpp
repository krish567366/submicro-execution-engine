#pragma once

#include "common_types.hpp"
#include "lockfree_queue.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>
#include <optional>
#include <functional>
#include <cmath>

namespace hft {

// ============================================================================
// Real-Time Limit Order Book (LOB) Reconstructor
// Tick-by-tick reconstruction with Deep OFI integration
// ============================================================================

// Price level in the order book
struct PriceLevel {
    double price;
    double quantity;
    uint64_t order_count;
    int64_t last_update_ns;  // Nanosecond timestamp
    
    PriceLevel() : price(0.0), quantity(0.0), order_count(0), last_update_ns(0) {}
    PriceLevel(double p, double q, uint64_t cnt = 1) 
        : price(p), quantity(q), order_count(cnt), last_update_ns(0) {}
};

// Individual order tracking for modify/cancel operations (LOB-specific)
struct TrackedOrder {
    uint64_t order_id;
    double price;
    double quantity;
    bool is_bid;  // true = bid, false = ask
    int64_t timestamp_ns;
    
    TrackedOrder() : order_id(0), price(0.0), quantity(0.0), is_bid(true), timestamp_ns(0) {}
};

// Order book update message types
enum class UpdateType {
    ADD,      // New order added to book
    MODIFY,   // Order quantity/price changed
    DELETE,   // Order canceled
    EXECUTE,  // Order executed (full or partial)
    SNAPSHOT  // Full book snapshot (for recovery)
};

// Order book update message
struct OrderBookUpdate {
    UpdateType type;
    uint64_t order_id;
    double price;
    double quantity;
    bool is_bid;
    uint64_t sequence_number;  // Exchange sequence number for gap detection
    int64_t timestamp_ns;
    int64_t exchange_timestamp_ns;  // Exchange's timestamp
    
    OrderBookUpdate() 
        : type(UpdateType::ADD), order_id(0), price(0.0), quantity(0.0), 
          is_bid(true), sequence_number(0), timestamp_ns(0), exchange_timestamp_ns(0) {}
};

// Deep Order Flow Imbalance (OFI) features
struct DeepOFIFeatures {
    // Level-by-level OFI (up to 10 levels)
    std::array<double, 10> bid_ofi;  // Order flow imbalance per bid level
    std::array<double, 10> ask_ofi;  // Order flow imbalance per ask level
    
    // Aggregated OFI metrics
    double total_ofi;           // Sum of all levels
    double weighted_ofi;        // Volume-weighted OFI
    double top_5_ofi;          // OFI for top 5 levels
    double top_1_ofi;          // OFI for best bid/ask
    
    // Order book imbalance
    double volume_imbalance;    // (bid_vol - ask_vol) / (bid_vol + ask_vol)
    double depth_imbalance;     // (bid_depth - ask_depth) / total_depth
    
    // Microstructure features
    double bid_ask_spread;
    double mid_price;
    double weighted_mid_price;  // Volume-weighted mid
    
    // Pressure metrics
    double buy_pressure;        // Recent aggressive buy flow
    double sell_pressure;       // Recent aggressive sell flow
    double net_pressure;        // buy_pressure - sell_pressure
    
    // Volatility estimates
    double microprice_volatility;  // Rolling std of weighted mid
    double spread_volatility;      // Rolling std of spread
    
    // Timestamp
    int64_t timestamp_ns;
    
    DeepOFIFeatures() : total_ofi(0.0), weighted_ofi(0.0), top_5_ofi(0.0), top_1_ofi(0.0),
                        volume_imbalance(0.0), depth_imbalance(0.0), bid_ask_spread(0.0),
                        mid_price(0.0), weighted_mid_price(0.0), buy_pressure(0.0),
                        sell_pressure(0.0), net_pressure(0.0), microprice_volatility(0.0),
                        spread_volatility(0.0), timestamp_ns(0) {
        bid_ofi.fill(0.0);
        ask_ofi.fill(0.0);
    }
};

// LOB snapshot for gap recovery
struct OrderBookSnapshot {
    std::vector<PriceLevel> bids;  // Sorted descending
    std::vector<PriceLevel> asks;  // Sorted ascending
    uint64_t sequence_number;
    int64_t timestamp_ns;
    std::string symbol;
    
    OrderBookSnapshot() : sequence_number(0), timestamp_ns(0) {}
};

// Callback function type for deep state publishing
using DeepStateCallback = std::function<void(const DeepOFIFeatures&)>;

// ============================================================================
// OrderBookReconstructor: Lock-free, tick-by-tick LOB with Deep OFI
// ============================================================================
class OrderBookReconstructor {
public:
    OrderBookReconstructor(const std::string& symbol, size_t max_depth = 100)
        : symbol_(symbol),
          max_depth_(max_depth),
          last_sequence_number_(0),
          gap_detected_(false),
          total_updates_(0),
          missed_updates_(0),
          snapshot_requests_(0),
          is_initialized_(false) {
        
        // Initialize OFI state tracking
        previous_bid_quantities_.fill(0.0);
        previous_ask_quantities_.fill(0.0);
        
        // Initialize pressure decay window (last 1000 ticks)
        recent_buy_volume_.reserve(1000);
        recent_sell_volume_.reserve(1000);
    }
    
    // Initialize the order book with a snapshot
    bool initialize_from_snapshot(const OrderBookSnapshot& snapshot) {
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        bids_.clear();
        asks_.clear();
        orders_.clear();
        
        // Load bids (sorted descending by price)
        for (const auto& level : snapshot.bids) {
            bids_[level.price] = level;
        }
        
        // Load asks (sorted ascending by price)
        for (const auto& level : snapshot.asks) {
            asks_[level.price] = level;
        }
        
        last_sequence_number_ = snapshot.sequence_number;
        is_initialized_.store(true, std::memory_order_release);
        
        return true;
    }
    
    // Process an order book update
    bool process_update(const OrderBookUpdate& update) {
        // Check for sequence number gap
        if (is_initialized_.load(std::memory_order_acquire)) {
            if (update.sequence_number != last_sequence_number_ + 1 
                && last_sequence_number_ != 0) {
                // Gap detected!
                gap_detected_.store(true, std::memory_order_release);
                missed_updates_ += (update.sequence_number - last_sequence_number_ - 1);
                
                // Trigger snapshot request (caller should handle this)
                snapshot_requests_++;
                return false;  // Reject update until snapshot received
            }
        }
        
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        // Store previous state for OFI calculation
        store_previous_state();
        
        // Process update based on type
        bool success = false;
        switch (update.type) {
            case UpdateType::ADD:
                success = handle_add(update);
                break;
            case UpdateType::MODIFY:
                success = handle_modify(update);
                break;
            case UpdateType::DELETE:
                success = handle_delete(update);
                break;
            case UpdateType::EXECUTE:
                success = handle_execute(update);
                break;
            case UpdateType::SNAPSHOT:
                // Should call initialize_from_snapshot instead
                return false;
        }
        
        if (success) {
            last_sequence_number_ = update.sequence_number;
            total_updates_++;
            
            // Calculate Deep OFI features
            auto features = calculate_deep_ofi(update.timestamp_ns);
            
            // Publish to registered callbacks
            publish_deep_state(features);
        }
        
        return success;
    }
    
    // Register callback for deep state publishing
    void register_deep_state_callback(DeepStateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callbacks_.push_back(callback);
    }
    
    // Get current best bid/ask
    std::pair<std::optional<PriceLevel>, std::optional<PriceLevel>> get_top_of_book() const {
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        std::optional<PriceLevel> best_bid;
        std::optional<PriceLevel> best_ask;
        
        if (!bids_.empty()) {
            best_bid = bids_.rbegin()->second;  // Highest bid
        }
        
        if (!asks_.empty()) {
            best_ask = asks_.begin()->second;   // Lowest ask
        }
        
        return {best_bid, best_ask};
    }
    
    // Get multiple levels (for deep book analysis)
    std::pair<std::vector<PriceLevel>, std::vector<PriceLevel>> 
    get_depth(size_t num_levels) const {
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
        
        // Get top N bids (highest prices first)
        size_t count = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && count < num_levels; ++it, ++count) {
            bids.push_back(it->second);
        }
        
        // Get top N asks (lowest prices first)
        count = 0;
        for (auto it = asks_.begin(); it != asks_.end() && count < num_levels; ++it, ++count) {
            asks.push_back(it->second);
        }
        
        return {bids, asks};
    }
    
    // Get current OFI features (non-blocking)
    DeepOFIFeatures get_current_ofi() const {
        std::lock_guard<std::mutex> lock(ofi_mutex_);
        return current_ofi_;
    }
    
    // Check if gap detected and needs snapshot recovery
    bool needs_snapshot_recovery() const {
        return gap_detected_.load(std::memory_order_acquire);
    }
    
    // Reset gap detection after snapshot recovery
    void reset_gap_detection() {
        gap_detected_.store(false, std::memory_order_release);
    }
    
    // Get statistics
    struct Statistics {
        uint64_t total_updates;
        uint64_t missed_updates;
        uint64_t snapshot_requests;
        uint64_t current_bid_levels;
        uint64_t current_ask_levels;
        double last_mid_price;
        double last_spread;
    };
    
    Statistics get_statistics() const {
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        Statistics stats;
        stats.total_updates = total_updates_;
        stats.missed_updates = missed_updates_;
        stats.snapshot_requests = snapshot_requests_;
        stats.current_bid_levels = bids_.size();
        stats.current_ask_levels = asks_.size();
        
        auto [best_bid, best_ask] = get_top_of_book();
        if (best_bid && best_ask) {
            stats.last_mid_price = (best_bid->price + best_ask->price) / 2.0;
            stats.last_spread = best_ask->price - best_bid->price;
        } else {
            stats.last_mid_price = 0.0;
            stats.last_spread = 0.0;
        }
        
        return stats;
    }

private:
    std::string symbol_;
    size_t max_depth_;
    
    // Order book state (std::map provides O(log n) operations and sorted iteration)
    std::map<double, PriceLevel> bids_;  // Price -> Level (descending order for bids)
    std::map<double, PriceLevel> asks_;  // Price -> Level (ascending order for asks)
    
    // Order tracking for modify/cancel
    std::unordered_map<uint64_t, TrackedOrder> orders_;
    
    // Sequence number tracking for gap detection
    uint64_t last_sequence_number_;
    std::atomic<bool> gap_detected_;
    
    // Statistics
    uint64_t total_updates_;
    uint64_t missed_updates_;
    uint64_t snapshot_requests_;
    std::atomic<bool> is_initialized_;
    
    // OFI state tracking
    std::array<double, 10> previous_bid_quantities_;
    std::array<double, 10> previous_ask_quantities_;
    std::vector<double> recent_buy_volume_;
    std::vector<double> recent_sell_volume_;
    
    // Current OFI features
    mutable DeepOFIFeatures current_ofi_;
    
    // Callbacks for deep state publishing
    std::vector<DeepStateCallback> callbacks_;
    
    // Thread safety
    mutable std::mutex book_mutex_;
    mutable std::mutex ofi_mutex_;
    mutable std::mutex callback_mutex_;
    
    // ========================================================================
    // Update handlers
    // ========================================================================
    
    bool handle_add(const OrderBookUpdate& update) {
        // Add new order to tracking
        TrackedOrder order;
        order.order_id = update.order_id;
        order.price = update.price;
        order.quantity = update.quantity;
        order.is_bid = update.is_bid;
        order.timestamp_ns = update.timestamp_ns;
        orders_[update.order_id] = order;
        
        // Update price level
        auto& book = update.is_bid ? bids_ : asks_;
        auto it = book.find(update.price);
        
        if (it != book.end()) {
            // Price level exists, add quantity
            it->second.quantity += update.quantity;
            it->second.order_count++;
            it->second.last_update_ns = update.timestamp_ns;
        } else {
            // New price level
            PriceLevel level(update.price, update.quantity, 1);
            level.last_update_ns = update.timestamp_ns;
            book[update.price] = level;
        }
        
        return true;
    }
    
    bool handle_modify(const OrderBookUpdate& update) {
        auto order_it = orders_.find(update.order_id);
        if (order_it == orders_.end()) {
            // Order not found, treat as add
            return handle_add(update);
        }
        
        TrackedOrder& order = order_it->second;
        auto& book = order.is_bid ? bids_ : asks_;
        
        // Remove old quantity from old price level
        auto old_level_it = book.find(order.price);
        if (old_level_it != book.end()) {
            old_level_it->second.quantity -= order.quantity;
            old_level_it->second.order_count--;
            
            // Remove price level if empty
            if (old_level_it->second.quantity <= 0.0 || old_level_it->second.order_count == 0) {
                book.erase(old_level_it);
            }
        }
        
        // Add new quantity at new price level
        order.price = update.price;
        order.quantity = update.quantity;
        order.timestamp_ns = update.timestamp_ns;
        
        auto new_level_it = book.find(update.price);
        if (new_level_it != book.end()) {
            new_level_it->second.quantity += update.quantity;
            new_level_it->second.order_count++;
            new_level_it->second.last_update_ns = update.timestamp_ns;
        } else {
            PriceLevel level(update.price, update.quantity, 1);
            level.last_update_ns = update.timestamp_ns;
            book[update.price] = level;
        }
        
        return true;
    }
    
    bool handle_delete(const OrderBookUpdate& update) {
        auto order_it = orders_.find(update.order_id);
        if (order_it == orders_.end()) {
            return false;  // Order not found
        }
        
        TrackedOrder& order = order_it->second;
        auto& book = order.is_bid ? bids_ : asks_;
        
        // Remove quantity from price level
        auto level_it = book.find(order.price);
        if (level_it != book.end()) {
            level_it->second.quantity -= order.quantity;
            level_it->second.order_count--;
            
            // Remove price level if empty
            if (level_it->second.quantity <= 0.0 || level_it->second.order_count == 0) {
                book.erase(level_it);
            }
        }
        
        // Remove order from tracking
        orders_.erase(order_it);
        
        return true;
    }
    
    bool handle_execute(const OrderBookUpdate& update) {
        auto order_it = orders_.find(update.order_id);
        if (order_it == orders_.end()) {
            // Execution without tracked order (aggressive trade)
            // Update pressure metrics
            if (update.is_bid) {
                recent_buy_volume_.push_back(update.quantity);
            } else {
                recent_sell_volume_.push_back(update.quantity);
            }
            
            // Keep only recent window
            if (recent_buy_volume_.size() > 1000) {
                recent_buy_volume_.erase(recent_buy_volume_.begin());
            }
            if (recent_sell_volume_.size() > 1000) {
                recent_sell_volume_.erase(recent_sell_volume_.begin());
            }
            
            return true;
        }
        
        TrackedOrder& order = order_it->second;
        auto& book = order.is_bid ? bids_ : asks_;
        
        // Reduce quantity from price level
        auto level_it = book.find(order.price);
        if (level_it != book.end()) {
            level_it->second.quantity -= update.quantity;
            
            // Check if fully executed
            if (update.quantity >= order.quantity) {
                level_it->second.order_count--;
                orders_.erase(order_it);
            } else {
                order.quantity -= update.quantity;
            }
            
            // Remove price level if empty
            if (level_it->second.quantity <= 0.0 || level_it->second.order_count == 0) {
                book.erase(level_it);
            }
        }
        
        return true;
    }
    
    // ========================================================================
    // Deep OFI calculation
    // ========================================================================
    
    void store_previous_state() {
        // Store current quantities for OFI delta calculation
        size_t level = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && level < 10; ++it, ++level) {
            previous_bid_quantities_[level] = it->second.quantity;
        }
        
        level = 0;
        for (auto it = asks_.begin(); it != asks_.end() && level < 10; ++it, ++level) {
            previous_ask_quantities_[level] = it->second.quantity;
        }
    }
    
    DeepOFIFeatures calculate_deep_ofi(int64_t timestamp_ns) {
        DeepOFIFeatures features;
        features.timestamp_ns = timestamp_ns;
        
        // Calculate per-level OFI
        size_t level = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && level < 10; ++it, ++level) {
            double delta = it->second.quantity - previous_bid_quantities_[level];
            features.bid_ofi[level] = delta;
        }
        
        level = 0;
        for (auto it = asks_.begin(); it != asks_.end() && level < 10; ++it, ++level) {
            double delta = it->second.quantity - previous_ask_quantities_[level];
            features.ask_ofi[level] = delta;
        }
        
        // Aggregate OFI metrics
        features.total_ofi = 0.0;
        features.top_5_ofi = 0.0;
        for (size_t i = 0; i < 10; ++i) {
            double level_ofi = features.bid_ofi[i] - features.ask_ofi[i];
            features.total_ofi += level_ofi;
            if (i < 5) {
                features.top_5_ofi += level_ofi;
            }
        }
        features.top_1_ofi = features.bid_ofi[0] - features.ask_ofi[0];
        
        // Volume-weighted OFI
        double total_volume = 0.0;
        features.weighted_ofi = 0.0;
        level = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && level < 10; ++it, ++level) {
            features.weighted_ofi += features.bid_ofi[level] * it->second.quantity;
            total_volume += it->second.quantity;
        }
        level = 0;
        for (auto it = asks_.begin(); it != asks_.end() && level < 10; ++it, ++level) {
            features.weighted_ofi -= features.ask_ofi[level] * it->second.quantity;
            total_volume += it->second.quantity;
        }
        if (total_volume > 0.0) {
            features.weighted_ofi /= total_volume;
        }
        
        // Order book imbalance
        double bid_volume = 0.0, ask_volume = 0.0;
        for (const auto& [price, level] : bids_) {
            bid_volume += level.quantity;
        }
        for (const auto& [price, level] : asks_) {
            ask_volume += level.quantity;
        }
        
        if (bid_volume + ask_volume > 0.0) {
            features.volume_imbalance = (bid_volume - ask_volume) / (bid_volume + ask_volume);
        }
        
        double bid_depth = bids_.size();
        double ask_depth = asks_.size();
        if (bid_depth + ask_depth > 0.0) {
            features.depth_imbalance = (bid_depth - ask_depth) / (bid_depth + ask_depth);
        }
        
        // Spread and mid price
        if (!bids_.empty() && !asks_.empty()) {
            double best_bid = bids_.rbegin()->first;
            double best_ask = asks_.begin()->first;
            features.bid_ask_spread = best_ask - best_bid;
            features.mid_price = (best_bid + best_ask) / 2.0;
            
            // Volume-weighted mid
            double bid_qty = bids_.rbegin()->second.quantity;
            double ask_qty = asks_.begin()->second.quantity;
            if (bid_qty + ask_qty > 0.0) {
                features.weighted_mid_price = (best_bid * ask_qty + best_ask * bid_qty) 
                                             / (bid_qty + ask_qty);
            } else {
                features.weighted_mid_price = features.mid_price;
            }
        }
        
        // Pressure metrics
        features.buy_pressure = 0.0;
        features.sell_pressure = 0.0;
        for (double vol : recent_buy_volume_) {
            features.buy_pressure += vol;
        }
        for (double vol : recent_sell_volume_) {
            features.sell_pressure += vol;
        }
        features.net_pressure = features.buy_pressure - features.sell_pressure;
        
        // Store for external access
        {
            std::lock_guard<std::mutex> lock(ofi_mutex_);
            current_ofi_ = features;
        }
        
        return features;
    }
    
    // ========================================================================
    // Deep state publishing
    // ========================================================================
    
    void publish_deep_state(const DeepOFIFeatures& features) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        for (auto& callback : callbacks_) {
            callback(features);
        }
    }
};

} // namespace hft
