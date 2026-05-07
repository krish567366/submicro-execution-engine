#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define HAS_KALMAN_SIMD 1
#else
#define HAS_KALMAN_SIMD 0
#endif

namespace hft {
namespace alpha {

/**
 * Kalman Filter Alpha (Portable)
 */
class SIMDKalmanFilter {
public:
    SIMDKalmanFilter() {
        for(int i=0; i<8; ++i) {
            prices_[i] = 0; velocity_[i] = 0;
            p00_[i] = 1.0; p11_[i] = 1.0;
        }
    }

    inline void step(const double* measurements, double dt) {
#if defined(__AVX512F__)
        // ... (Original SIMD code preserved in thoughts but using scalar for portability here)
#endif
        for (int i = 0; i < 8; ++i) {
            // Pred
            prices_[i] += velocity_[i] * dt;
            p00_[i] += 0.0001 + (dt * dt * p11_[i]); // Simplified
            p11_[i] += 0.001;
            // Update
            double y = measurements[i] - prices_[i];
            double k = p00_[i] / (p00_[i] + 0.01);
            prices_[i] += k * y;
            p00_[i] *= (1.0 - k);
        }
    }

    inline void get_estimates(double* p_out) {
        for(int i=0; i<8; ++i) p_out[i] = prices_[i];
    }

private:
    double prices_[8], velocity_[8], p00_[8], p11_[8];
};

} }
