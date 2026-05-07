#pragma once

#include "common_types.hpp"
#include <complex>
#include <array>
#include <cmath>
#include <vector>

namespace hft {
namespace spectral {

/**
 * Spectral Microstructure Analysis (The "Hidden Frequency" Domain)
 * 
 * Concept (Industry First?):
 * Traditional HFT analyzes the Market in the "Time Domain" (Price(t)).
 * We analyze the Market in the "Frequency Domain" (Amplitude(f)).
 * 
 * Mechanism:
 * 1. Maintain a rolling buffer of Mid-Price deltas.
 * 2. Apply a Slide-Window DFT (Discrete Fourier Transform) or Goertzel Algorithm.
 * 3. Extract "Market Resonance": Is there a hidden 50Hz algothm operating?
 * 4. Detect "Icebergs": Periodic reloadings appear as strong spikes in the magnitude spectrum.
 * 
 * Physics Analog:
 * We treat the Limit Order Book as a vibrating string. We trade the harmonics.
 */
class SpectralAnalyzer {
public:
    static constexpr int WINDOW_SIZE = 32; // Small window for ultra-low latency
    using Complex = std::complex<double>;

    SpectralAnalyzer() : cursor_(0) {
        history_.fill(0.0);
        // Precompute Twiddle Factors (Roots of Unity)
        for (int k = 0; k < WINDOW_SIZE; ++k) {
            for (int n = 0; n < WINDOW_SIZE; ++n) {
                double angle = -2.0 * M_PI * k * n / WINDOW_SIZE;
                twiddles_[k][n] = Complex(std::cos(angle), std::sin(angle));
            }
        }
    }

    // Input: High-frequency price updates (e.g. every 100us)
    inline void push_sample(double price_delta) {
        history_[cursor_] = price_delta;
        cursor_ = (cursor_ + 1) & (WINDOW_SIZE - 1);
    }

    /**
     * Compute "Spectral Entropy"
     * 
     * Low Entropy = Market is governed by a single dominant algo (Periodic).
     * High Entropy = Market is random noise (White noise).
     * 
     * Strategy:
     * - If Low Entropy: Counter-trade the dominant frequency (Mean Reversion).
     * - If High Entropy: Stay out or Trend Follow.
     */
    inline double compute_spectral_entropy() {
        std::array<double, WINDOW_SIZE> magnitudes;
        double total_energy = 0.0;

        // Compute Spectrum (Naive O(N^2) for small N=32 is faster than FFT overload)
        // Or O(N) if we only check specific frequencies.
        // Let's do full DFT for N=32 using precomputed matrix.
        
        for (int k = 0; k < WINDOW_SIZE / 2; ++k) { // Nyquist
            Complex sum(0, 0);
            
            // Vectorize this inner loop?
            for (int n = 0; n < WINDOW_SIZE; ++n) {
                // Ring buffer access: index (cursor + n) % size
                int hist_idx = (cursor_ + n) & (WINDOW_SIZE - 1);
                sum += history_[hist_idx] * twiddles_[k][n];
            }
            
            double mag = std::norm(sum); // mag^2
            magnitudes[k] = mag;
            total_energy += mag;
        }

        // Normalize -> Probability Distribution
        if (total_energy < 1e-9) return 1.0; // Max entropy (flat)

        double entropy = 0.0;
        for (int k = 0; k < WINDOW_SIZE / 2; ++k) {
            double p = magnitudes[k] / total_energy;
            if (p > 1e-9) {
                entropy -= p * std::log(p);
            }
        }
        
        // Normalize entropy to [0, 1]
        // Max entropy for N bins is ln(N)
        constexpr double max_entropy = 2.77258872224; // ln(16)
        
        return entropy / max_entropy;
    }

    // Detect "Pinger" Frequency
    // Returns index of dominant frequency bin (0..15)
    inline int detect_dominant_frequency() {
        double max_mag = -1.0;
        int dominant_k = 0;
        
        for (int k = 1; k < WINDOW_SIZE / 2; ++k) { // Skip DC (k=0)
            Complex sum(0, 0);
            for (int n = 0; n < WINDOW_SIZE; ++n) {
                int hist_idx = (cursor_ + n) & (WINDOW_SIZE - 1);
                sum += history_[hist_idx] * twiddles_[k][n];
            }
            double mag = std::norm(sum);
            if (mag > max_mag) {
                max_mag = mag;
                dominant_k = k;
            }
        }
        return dominant_k;
    }

private:
    std::array<double, WINDOW_SIZE> history_;
    size_t cursor_;
    
    // LUT for DFT
    std::array<std::array<Complex, WINDOW_SIZE>, WINDOW_SIZE> twiddles_;
};

}
}
