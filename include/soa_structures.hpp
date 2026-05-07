#pragma once

#include "common_types.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <algorithm>

// x86 SIMD Intinsics
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

namespace hft {
namespace memory {

/**
 * Struct-of-Arrays (SoA) Order Book Storage
 * 
 * Why:
 * - Traditional (AoS): [Price, Qty, Count], [Price, Qty, Count]...
 *   - Loading prices require stride loads or gathers (slow).
 * - SoA: [Price, Price...], [Qty, Qty...], [Count, Count...]
 *   - Loading 8 prices is a single _mm512_load_pd (fast).
 *   - Perfect for AVX-512 feature extraction.
 */
template<size_t MaxDepth>
class SoAOrderBook {
public:
    static constexpr size_t ALIGNMENT = 64; // Cache Line / AVX-512 alignment

    SoAOrderBook() : count_(0) {
        // Zero out memory
        std::memset(prices_, 0, sizeof(prices_));
        std::memset(quantities_, 0, sizeof(quantities_));
        std::memset(order_counts_, 0, sizeof(order_counts_));
    }

    // Insert/Update Level (Scalar) - slightly slower than AoS due to cache locality of separate arrows,
    // BUT we optimize for READS (feature generation), which happens much more often or needs to be faster.
    inline void update_level(double price, double qty, uint32_t cnt) {
        // 1. Find index (linear scan is fine for small depth)
        // Optimization: Use SIMD to find price index?
        // Let's implement Scalar update for safety, SIMD read.
        
        ssize_t found_idx = -1;
        for (size_t i = 0; i < count_; ++i) {
            if (std::abs(prices_[i] - price) < 1e-9) {
                found_idx = i;
                break;
            }
        }

        if (found_idx != -1) {
            // Update existng
            if (qty <= 0) {
                // Delete: Shift left
                // Memmove is fast
                size_t remainder = count_ - found_idx - 1;
                if (remainder > 0) {
                    std::memmove(&prices_[found_idx], &prices_[found_idx+1], remainder * sizeof(double));
                    std::memmove(&quantities_[found_idx], &quantities_[found_idx+1], remainder * sizeof(double));
                    std::memmove(&order_counts_[found_idx], &order_counts_[found_idx+1], remainder * sizeof(uint32_t));
                }
                count_--;
            } else {
                quantities_[found_idx] = qty;
                order_counts_[found_idx] = cnt;
            }
        } else {
            // Insert New
            if (qty > 0 && count_ < MaxDepth) {
                // Find insertion point based on side
                size_t i = 0;
                if (is_bid_) {
                    // Bids: Descending order (highest first)
                    while (i < count_ && prices_[i] > price) i++;
                } else {
                    // Asks: Ascending order (lowest first)
                    while (i < count_ && prices_[i] < price) i++;
                }
                
                // Shift Right
                size_t remainder = count_ - i;
                if (remainder > 0) {
                    std::memmove(&prices_[i+1], &prices_[i], remainder * sizeof(double));
                    std::memmove(&quantities_[i+1], &quantities_[i], remainder * sizeof(double));
                    std::memmove(&order_counts_[i+1], &order_counts_[i], remainder * sizeof(uint32_t));
                }
                
                prices_[i] = price;
                quantities_[i] = qty;
                order_counts_[i] = cnt;
                count_++;
            }
        }
    }

    // Constructor now takes side parameter
    PriceLevelArray(bool is_bid = true) : count_(0), is_bid_(is_bid) {
        prices_.fill(0.0);
        quantities_.fill(0.0);
        order_counts_.fill(0);
    }

    // ==== SIMD Accessors (The Real Benefit) ====

#if defined(__AVX512F__)
    /**
     * Load top 8 prices into AVX-512 register
     * Zero overhead load.
     */
    inline __m512d load_prices_simd() const {
        // Unsafe load if MaxDepth < 8? We padded the arrays so it's safe.
        return _mm512_load_pd(prices_); 
    }

    inline __m512d load_qtys_simd() const {
        return _mm512_load_pd(quantities_);
    }
#elif defined(__AVX2__)
    inline __m256d load_prices_simd() const {
        return _mm256_load_pd(prices_);
    }
    inline __m256d load_qtys_simd() const {
        return _mm256_load_pd(quantities_);
    }
#endif

    // Raw pointer access for legacy code
    const double* prices() const { return prices_; }
    const double* quantities() const { return quantities_; }
    size_t count() const { return count_; }

private:
    // Padded to ensure we can always do full vector loads even at tail
    static constexpr size_t PADDED_SIZE = (MaxDepth + 15) & ~15; 

    alignas(ALIGNMENT) double prices_[PADDED_SIZE];
    alignas(ALIGNMENT) double quantities_[PADDED_SIZE];
    alignas(ALIGNMENT) uint32_t order_counts_[PADDED_SIZE];
    
    size_t count_;
    bool is_bid_;  // true = bids (descending), false = asks (ascending)
};

} // namespace memory
} // namespace hft
