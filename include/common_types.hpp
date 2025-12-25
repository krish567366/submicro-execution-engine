#pragma once

#include <cstdint>
#include <chrono>
#include <array>
#include <atomic>

namespace hft {

// ============================================================================
// Timestamp Utilities (Nanosecond precision)
// ============================================================================

using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using Duration = std::chrono::nanoseconds;

inline int64_t to_nanos(const Timestamp& tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

inline Timestamp now() {
    return std::chrono::steady_clock::now();
}

// ============================================================================
// Order Side Enum
// ============================================================================

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// ============================================================================
// Market Data Structures (Cache-line aligned for zero-copy)
// ============================================================================

struct alignas(64) MarketTick {
    Timestamp timestamp;
    double bid_price;
    double ask_price;
    double mid_price;
    uint64_t bid_size;
    uint64_t ask_size;
    uint64_t trade_volume;
    Side trade_side;
    uint32_t asset_id;       // For cross-asset tracking
    uint8_t depth_levels;    // Number of LOB levels available
    uint8_t padding[7];      // Explicit padding for alignment
    
    // Deep order book imbalance features (up to 10 levels)
    std::array<double, 10> bid_prices;
    std::array<double, 10> ask_prices;
    std::array<uint64_t, 10> bid_sizes;
    std::array<uint64_t, 10> ask_sizes;
    
    MarketTick() : timestamp(now()), bid_price(0.0), ask_price(0.0), 
                   mid_price(0.0), bid_size(0), ask_size(0), trade_volume(0),
                   trade_side(Side::BUY), asset_id(0), depth_levels(0) {
        bid_prices.fill(0.0);
        ask_prices.fill(0.0);
        bid_sizes.fill(0);
        ask_sizes.fill(0);
    }
};

// ============================================================================
// Order Structure
// ============================================================================

struct alignas(64) Order {
    uint64_t order_id;
    uint32_t asset_id;
    Side side;
    double price;
    uint64_t quantity;
    Timestamp submit_time;
    uint8_t venue_id;        // Exchange routing
    bool is_active;
    uint8_t padding[6];
    
    Order() : order_id(0), asset_id(0), side(Side::BUY), price(0.0),
              quantity(0), submit_time(now()), venue_id(0), is_active(false) {}
    
    Order(uint64_t id, uint32_t asset, Side s, double p, uint64_t q)
        : order_id(id), asset_id(asset), side(s), price(p), quantity(q),
          submit_time(now()), venue_id(0), is_active(true) {}
};

// ============================================================================
// Quote Pair (Bid/Ask)
// ============================================================================

struct QuotePair {
    double bid_price;
    double ask_price;
    double bid_size;
    double ask_size;
    double spread;
    double mid_price;
    Timestamp generated_at;
    
    QuotePair() : bid_price(0.0), ask_price(0.0), bid_size(0.0), 
                  ask_size(0.0), spread(0.0), mid_price(0.0),
                  generated_at(now()) {}
};

// ============================================================================
// Trading Event (for Hawkes Process)
// ============================================================================

struct TradingEvent {
    Timestamp arrival_time;
    Side event_type;         // Buy or Sell event
    uint32_t asset_id;
    double intensity;        // Current intensity at event time
    
    TradingEvent() : arrival_time(now()), event_type(Side::BUY), 
                     asset_id(0), intensity(0.0) {}
    
    TradingEvent(Timestamp t, Side type, uint32_t asset)
        : arrival_time(t), event_type(type), asset_id(asset), intensity(0.0) {}
};

// ============================================================================
// Risk Regime
// ============================================================================

enum class MarketRegime : uint8_t {
    NORMAL = 0,
    ELEVATED_VOLATILITY = 1,
    HIGH_STRESS = 2,
    HALTED = 3
};

} // namespace hft
