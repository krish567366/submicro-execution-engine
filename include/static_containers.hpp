#pragma once

#include "common_types.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace hft {
namespace containers {

/**
 * Static Vector
 * 
 * Purpose: 
 * A std::vector equivalent that NEVER allocates heap memory.
 * Capacity is fixed at compile time.
 * Ideal for "Tick Data" lists, "Order IDs", "Features" inside the L1 cache.
 * 
 * Safety:
 * Bounds checked in debug, unsafe (fast) in release.
 */
template<typename T, size_t Capacity>
class StaticVector {
public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    StaticVector() : size_(0) {}

    // Copy Constructor
    StaticVector(const StaticVector& other) {
        // Trivial copy optimisation
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data_, other.data_, other.size_ * sizeof(T));
        } else {
            for (size_t i = 0; i < other.size_; ++i) {
                new (&data_[i]) T(other[i]);
            }
        }
        size_ = other.size_;
    }

    inline void push_back(const T& value) {
        if (size_ < Capacity) {
            new (&data_[size_++]) T(value);
        }
    }

    template<typename... Args>
    inline void emplace_back(Args&&... args) {
        if (size_ < Capacity) {
            new (&data_[size_++]) T(std::forward<Args>(args)...);
        }
    }

    inline void clear() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < size_; ++i) {
                reinterpret_cast<T*>(&data_[i])->~T();
            }
        }
        size_ = 0;
    }

    inline T& operator[](size_t index) {
        return *reinterpret_cast<T*>(&data_[index]);
    }

    inline const T& operator[](size_t index) const {
        return *reinterpret_cast<const T*>(&data_[index]);
    }

    inline size_t size() const { return size_; }
    inline bool empty() const { return size_ == 0; }
    inline size_t capacity() const { return Capacity; }

    inline iterator begin() { return reinterpret_cast<T*>(&data_[0]); }
    inline iterator end() { return reinterpret_cast<T*>(&data_[size_]); }
    inline const_iterator begin() const { return reinterpret_cast<const T*>(&data_[0]); }
    inline const_iterator end() const { return reinterpret_cast<const T*>(&data_[size_]); }

private:
    alignas(T) std::array<uint8_t, sizeof(T) * Capacity> data_; // Uninitialized storage
    size_t size_;
};

/**
 * Fixed String
 * 
 * Purpose: 
 * Store symbols ("BTCUSD") or Account IDs ("ACC123") in-situ without pointers.
 * Fits in registers if Size <= 8.
 */
template<size_t N>
struct FixedString {
    char data[N];

    FixedString() {
        std::memset(data, 0, N);
    }

    FixedString(const char* src) {
        std::strncpy(data, src, N);
    }

    bool operator==(const FixedString& other) const {
        // Optimized comparison for small strings (SWAR - SIMD Within A Register)
        if constexpr (N == 4) {
            return *reinterpret_cast<const uint32_t*>(data) == *reinterpret_cast<const uint32_t*>(other.data);
        } else if constexpr (N == 8) {
            return *reinterpret_cast<const uint64_t*>(data) == *reinterpret_cast<const uint64_t*>(other.data);
        } else {
            return std::memcmp(data, other.data, N) == 0;
        }
    }
    
    // Hash support for map keys
    struct Hash {
        size_t operator()(const FixedString& s) const {
            if constexpr (N <= 8) {
                return *reinterpret_cast<const size_t*>(s.data); // Identity hash for small strings
            } else {
                // FNV-1a
                size_t hash = 14695981039346656037ULL;
                for (size_t i = 0; i < N && s.data[i]; ++i) {
                    hash ^= s.data[i];
                    hash *= 1099511628211ULL;
                }
                return hash;
            }
        }
    };
};

} // namespace containers
} // namespace hft
