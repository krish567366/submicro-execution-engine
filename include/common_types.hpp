#pragma once

// ============================================================================
// PLATFORM REQUIREMENTS FOR PRODUCTION HFT SYSTEM
// ============================================================================
// This system is designed for x86-64 CPUs with AVX2/AVX-512 support.
// For optimal performance, build and run on:
//   - Intel Xeon (Cascade Lake or newer) 
//   - AMD EPYC (Zen 2 or newer)
//   - Linux kernel 5.x+ with real-time patches
//
// ARM64 builds are NOT optimized and lack critical features:
//   - SIMD order book operations
//   - Hardware transactional memory (TSX)
//   - Direct NIC register access
//   - Sub-microsecond timing precision
//
// To build on this platform anyway (development/testing only), add:
//   -DALLOW_NON_OPTIMAL_BUILD
// ============================================================================

#if !defined(__x86_64__) && !defined(_M_X64) && !defined(ALLOW_NON_OPTIMAL_BUILD)
    #error "==================================================================" \
           "PRODUCTION HFT BUILD REQUIRES x86-64 ARCHITECTURE" \
           "" \
           "This codebase uses Intel/AMD-specific optimizations:" \
           "  • AVX-512 for parallel strategy evaluation" \
           "  • TSX for lock-free transactions" \
           "  • RDTSC/RDPMC for nanosecond timing" \
           "  • Direct PCIe MMIO for kernel bypass" \
           "" \
           "Current platform detected: ARM64 or other" \
           "" \
           "SOLUTIONS:" \
           "  1. Build on Linux x86-64 server (RECOMMENDED)" \
           "  2. For development testing only, add: -DALLOW_NON_OPTIMAL_BUILD" \
           "     (WARNING: Will be 10-100x slower, missing critical features)" \
           "=================================================================="
#endif

#include <cstdint>
#include <chrono>
#include <array>
#include <atomic>

// Cache-line and Prefetch Macros
#define HFT_CACHE_LINE 64
#define HFT_PREFETCH(addr, rw, loc) __builtin_prefetch(addr, rw, loc)
#define HFT_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HFT_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace hft {

using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using Duration = std::chrono::nanoseconds;

inline int64_t to_nanos(const Timestamp& tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
}

inline Timestamp now() {
    return std::chrono::steady_clock::now();
}

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

struct alignas(64) MarketTick {
    Timestamp timestamp;
    double bid_price;
    double ask_price;
    double mid_price;
    uint64_t bid_size;
    uint64_t ask_size;
    uint64_t trade_volume;
    Side trade_side;
    uint32_t asset_id;
    uint8_t depth_levels;
    uint8_t padding[7];

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

struct alignas(64) Order {
    uint64_t order_id;
    uint32_t asset_id;
    Side side;
    double price;
    uint64_t quantity;
    Timestamp submit_time;
    uint8_t venue_id;
    bool is_active;
    uint8_t padding[6];

    Order() : order_id(0), asset_id(0), side(Side::BUY), price(0.0),
              quantity(0), submit_time(now()), venue_id(0), is_active(false) {}

    Order(uint64_t id, uint32_t asset, Side s, double p, uint64_t q)
        : order_id(id), asset_id(asset), side(s), price(p), quantity(q),
          submit_time(now()), venue_id(0), is_active(true) {}
};

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

struct TradingEvent {
    Timestamp arrival_time;
    Side event_type;
    uint32_t asset_id;
    double intensity;

    TradingEvent() : arrival_time(now()), event_type(Side::BUY),
                     asset_id(0), intensity(0.0) {}

    TradingEvent(Timestamp t, Side type, uint32_t asset)
        : arrival_time(t), event_type(type), asset_id(asset), intensity(0.0) {}
};

enum class MarketRegime : uint8_t {
    NORMAL = 0,
    ELEVATED_VOLATILITY = 1,
    HIGH_STRESS = 2,
    HALTED = 3
};

}