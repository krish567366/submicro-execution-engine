#pragma once

#include "common_types.hpp"
#include <vector>
#include <cmath>

#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

namespace hft {
namespace neuromorphic {

/**
 * Spiking Neural Network Core (Portable)
 */
class SpikingCore {
public:
    static constexpr int N_NEURONS = 256;
    
    SpikingCore() {
        for(int i=0; i<N_NEURONS; ++i) {
            voltage_[i] = 0.0f; threshold_[i] = 1.0f; decay_[i] = 0.95f; 
        }
    }

    inline void input_current(const float* currents, uint64_t* fire_mask) {
#if defined(__AVX512F__)
        // ... (SIMD path)
#endif
        uint64_t mask = 0;
        for (int i = 0; i < N_NEURONS; ++i) {
            voltage_[i] = (voltage_[i] * decay_[i]) + currents[i];
            if (voltage_[i] > threshold_[i]) {
                voltage_[i] = 0.0f;
                if (i < 64) mask |= (1ULL << i);
            }
        }
        if (fire_mask) *fire_mask = mask;
    }

private:
    float voltage_[N_NEURONS], threshold_[N_NEURONS], decay_[N_NEURONS];
};

} }
