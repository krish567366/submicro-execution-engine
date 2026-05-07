#pragma once

#include <cstdint>
#include <cmath>

namespace hft {
namespace math {

/**
 * 64-bit Fixed Point Math (Scale 1e9)
 * 
 * Bypasses FPU and provides nanosecond-range precision for alpha math.
 * 1.0 = 1,000,000,000
 */
class FixedPoint {
public:
    static constexpr int64_t SCALE = 1000000000;
    
    FixedPoint() : raw_(0) {}
    explicit FixedPoint(double d) : raw_(static_cast<int64_t>(d * SCALE)) {}
    explicit FixedPoint(int64_t raw, bool) : raw_(raw) {}

    static inline FixedPoint from_double(double d) { return FixedPoint(d); }
    
    inline FixedPoint operator+(const FixedPoint& other) const { return FixedPoint(raw_ + other.raw_, true); }
    inline FixedPoint operator-(const FixedPoint& other) const { return FixedPoint(raw_ - other.raw_, true); }
    
    inline FixedPoint operator*(const FixedPoint& other) const {
        // (A * B) / SCALE
        __int128_t res = (__int128_t)raw_ * other.raw_;
        return FixedPoint(static_cast<int64_t>(res / SCALE), true);
    }
    
    inline FixedPoint operator/(const FixedPoint& other) const {
        // (A * SCALE) / B
        __int128_t res = (__int128_t)raw_ * SCALE / other.raw_;
        return FixedPoint(static_cast<int64_t>(res), true);
    }

    double to_double() const { return static_cast<double>(raw_) / SCALE; }
    int64_t raw() const { return raw_; }

private:
    int64_t raw_;
};

} // namespace math
} // namespace hft
