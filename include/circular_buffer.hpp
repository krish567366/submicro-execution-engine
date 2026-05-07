#pragma once

#include <cstddef>
#include <array>
#include <atomic>

namespace hft {
namespace container {

/**
 * Lock-Free Static Circular Buffer (Zero Allocation)
 * 
 * Optimized for HFT sliding windows (Indicators, Alpha history).
 */
template<typename T, size_t Capacity>
class StaticCircularBuffer {
public:
    inline void push(T value) {
        data_[head_ % Capacity] = value;
        head_++;
        if (size_ < Capacity) size_++;
    }

    inline const T& operator[](size_t i) const {
        // i=0 is oldest in window
        if (size_ < Capacity) return data_[i % size_];
        size_t start = head_ % Capacity;
        return data_[(start + i) % Capacity];
    }

    inline const T& back() const {
        return data_[(head_ - 1) % Capacity];
    }

    inline const T& get_from_back(size_t i) const {
        // i=0 is newest
        return data_[(head_ - 1 - i) % Capacity];
    }

    inline size_t size() const { return size_; }
    inline bool full() const { return size_ == Capacity; }

private:
    std::array<T, Capacity> data_;
    size_t head_ = 0;
    size_t size_ = 0;
};

} // namespace container
} // namespace hft
