#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>
#include <algorithm>

// AVX-512 Detection
#if defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
    #if defined(__AVX512F__)
        #define USE_AVX512_NN 1
    #endif
#endif

namespace hft {
namespace ml {

/**
 * Nanosecond-Scale Inference Engine
 * 
 * Purpose: Run small MLPs (e.g., 64x32x1) inside the tick-to-trade loop.
 * Latency Budget: < 200ns.
 * Constraints: No heap allocation, weights pinned in registers/L1.
 * 
 * Optimization:
 * - Weights are stored in Struct-of-Arrays (blocks of 16 floats/doubles).
 * - Matrix-Vector multiplication is unrolled and vectorized.
 * - Activation functions use fast approximations.
 */

// Fast Sigmoid Approximation: 1 / (1 + |x|) or Polynomial
// AVX-512: Use _mm512_exp2_ps for actual sigmoid?
// Fast exp approximation is usually sufficient.
// Sigmoid(x) = 0.5 * (x / (1 + |x|) + 1)  [Hard Sigmoid-ish]
// Standard: 1 / (1 + exp(-x))
template<typename T>
inline T fast_sigmoid(T x) {
    return 1.0 / (1.0 + std::exp(-x)); 
}

// AVX512 Fast Sigmoid
#if defined(USE_AVX512_NN)
inline __m512 fast_sigmoid_avx512(__m512 x) {
    // 1.0f
    __m512 ones = _mm512_set1_ps(1.0f);
    // -x
    __m512 neg_x = _mm512_sub_ps(_mm512_setzero_ps(), x);
    // exp(-x) approximate
    // _mm512_exp2_ps is base 2. e^x = 2^(x * log2(e))
    __m512 log2e = _mm512_set1_ps(1.442695f);
    __m512 e_x = _mm512_exp2_ps(_mm512_mul_ps(neg_x, log2e));
    // 1 + exp(-x)
    __m512 den = _mm512_add_ps(ones, e_x);
    // 1 / den
    return _mm512_div_ps(ones, den);
}
#endif

// Dense Layer: Inputs -> Outputs
// InputSize and OutputSize must be multiples of 16 for AVX-512 efficiency (padding required otherwise)
template<size_t InputSize, size_t OutputSize>
class DenseLayer {
public:
    static constexpr size_t ALIGNMENT = 64;
    
    DenseLayer() {
        std::fill(weights_.begin(), weights_.end(), 0.0f);
        std::fill(biases_.begin(), biases_.end(), 0.0f);
    }

    void set_weights(const std::vector<float>& w, const std::vector<float>& b) {
        if (w.size() != InputSize * OutputSize || b.size() != OutputSize) return;
        
        // Store weights in column-major order for efficient SIMD dot product?
        // Actually, for matrix-vector (W * x), row-major is better if we broadcast x[i].
        // Or column-major if we acculate output vectors.
        // Let's use Column-Major: W = [Col0, Col1, ... ColInput]
        // Each Col is size OutputSize.
        // Output[j] += x[i] * W[i][j]
        // This allows us to load a vector of W (OutputSize) and FMA it with broadcast(x[i]).
        
        for (size_t i = 0; i < InputSize; ++i) {
            for (size_t j = 0; j < OutputSize; ++j) {
                // Input i, Output j
                size_t idx = i * OutputSize + j; // flat index in our internal storage
                weights_[idx] = w[i * OutputSize + j]; // Assuming input w is row-major? 
                // Let's assume input 'w' is standard row-major (Input x Output)
                // We store row-major internally too.
            }
        }
        
        for (size_t j = 0; j < OutputSize; ++j) biases_[j] = b[j];
    }

    // Inference: x is input array, y is output array
    inline void forward(const float* __restrict__ input, float* __restrict__ output) const {
#if defined(USE_AVX512_NN) && (OutputSize % 16 == 0)
        // AVX-512 Path
        
        // Initialize output with biases
        for (size_t j = 0; j < OutputSize; j += 16) {
            __m512 b = _mm512_load_ps(&biases_[j]);
            _mm512_store_ps(&output[j], b);
        }
        
        // MatMul: y = W^T * x + b
        // Logic: For each input i:
        //   Broadcast x[i] to vector
        //   Load W[row i] (vector of size OutputSize)
        //   FMA into output accumulator
        
        for (size_t i = 0; i < InputSize; ++i) {
            __m512 in_val = _mm512_set1_ps(input[i]);
            
            for (size_t j = 0; j < OutputSize; j += 16) {
                // Weight index: Row i, Col j -> i * OutputSize + j
                __m512 w = _mm512_load_ps(&weights_[i * OutputSize + j]);
                __m512 out_acc = _mm512_load_ps(&output[j]); // Load current partial sum
                
                // out += in * w
                out_acc = _mm512_fmadd_ps(in_val, w, out_acc);
                
                _mm512_store_ps(&output[j], out_acc);
            }
        }
        
        // Activation (ReLU)
        __m512 zeros = _mm512_setzero_ps();
        for (size_t j = 0; j < OutputSize; j += 16) {
            __m512 y = _mm512_load_ps(&output[j]);
            y = _mm512_max_ps(y, zeros); // ReLU
            _mm512_store_ps(&output[j], y);
        }

#else
        // Scalar Fallback
        for (size_t j = 0; j < OutputSize; ++j) output[j] = biases_[j];
        
        for (size_t i = 0; i < InputSize; ++i) {
            float in_val = input[i];
            for (size_t j = 0; j < OutputSize; ++j) {
                output[j] += in_val * weights_[i * OutputSize + j];
            }
        }
        
        // ReLU
        for (size_t j = 0; j < OutputSize; ++j) {
            if (output[j] < 0.0f) output[j] = 0.0f;
        }
#endif
    }

private:
    alignas(ALIGNMENT) std::array<float, InputSize * OutputSize> weights_;
    alignas(ALIGNMENT) std::array<float, OutputSize> biases_;
};

// Fixed Topology MLP: 64 Input -> 32 Hidden -> 1 Output (Signal)
class TradingMLP {
public:
    // Padded to 16-element boundary for AVX-512
    static constexpr size_t L1_IN = 64;
    static constexpr size_t L1_OUT = 32;
    static constexpr size_t L2_OUT = 16; // Padded from 1? No, let's output 16 logits (e.g. classes or buckets)
    // Or simpler: Single output scalar value. Dealing with <16 vector is annoying.
    // Let's make final layer 16 outputs, use 0th as signal.
    
    DenseLayer<L1_IN, L1_OUT> layer1;
    DenseLayer<L1_OUT, L2_OUT> layer2;
    
    // Intermediate buffer (HOT memory)
    alignas(64) float hidden_buf[L1_OUT];
    alignas(64) float output_buf[L2_OUT];

    inline float predict(const float* input_features) {
        // L1 Forward
        layer1.forward(input_features, hidden_buf);
        
        // L2 Forward
        layer2.forward(hidden_buf, output_buf);
        
        // Fast Sigmoid on logic 0
        return fast_sigmoid(output_buf[0]);
    }
};

} // namespace ml
} // namespace hft