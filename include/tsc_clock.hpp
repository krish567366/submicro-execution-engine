#pragma once

#include "common_types.hpp"
#include <cstdint>
#include <chrono>
#include <thread>
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace hft {
namespace clocking {

/**
 * TSC (Time Stamp Counter) High-Precision Clock
 * 
 * Problem: 
 * - `std::chrono::system_clock::now()` takes ~20-50ns (syscall or vdso).
 * - Simple `rdtsc` returns cycles, not time.
 * - Converting cycles to time via `cycles / frequency` is slow (division).
 * 
 * Solution:
 * - "Reciprocal Multiplication": Replace division with `(cycles * mult) >> shift`.
 * - Periodically sync with wall clock (NTP) to handle drift.
 * - Latency: ~6-8 cycles (instruction latency of rdtsc + mul).
 */
class TscClock {
public:
    static TscClock& instance() {
        static TscClock clock;
        return clock;
    }

    // Initialize calibration
    void calibrate(std::chrono::milliseconds duration = std::chrono::milliseconds(100)) {
        // 1. Warmup
        auto t0 = std::chrono::steady_clock::now();
        uint64_t c0 = rdtsc();
        std::this_thread::sleep_for(duration);
        uint64_t c1 = rdtsc();
        auto t1 = std::chrono::steady_clock::now();

        auto dt_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        double cycles_per_ns = static_cast<double>(c1 - c0) / dt_ns;
        
        // 2. Compute fixed-point multiplier/shift
        // target: cycles * multiplier >> shift = ns
        // ns = cycles * (1/freq)
        // multiplier = (1/freq) * 2^shift
        
        shift_ = 32; // Sufficient precision
        double mult = (1.0 / cycles_per_ns) * (1ULL << shift_);
        multiplier_ = static_cast<uint64_t>(mult + 0.5);
        
        // Base sync
        sync_wall_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        sync_tsc_ = rdtsc();
    }

    // Get current wall time in nanoseconds
    inline uint64_t now() const {
        uint64_t current_tsc = rdtsc();
        
        // Delta cycles
        uint64_t delta_cycles = current_tsc - sync_tsc_;
        
        // Fast conversion using 128-bit math (on 64-bit systems)
        unsigned __int128 prod = (unsigned __int128)delta_cycles * multiplier_;
        uint64_t delta_ns = (uint64_t)(prod >> shift_);
        
        return sync_wall_ns_ + delta_ns;
    }
    
    // Raw cycle count
    inline uint64_t cycles() const {
        return rdtsc();
    }

private:
    uint64_t multiplier_;
    uint32_t shift_;
    
    uint64_t sync_tsc_;
    uint64_t sync_wall_ns_;

    TscClock() {
        calibrate();
    }

    inline uint64_t rdtsc() const {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned int lo, hi;
        __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
        uint64_t val;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r" (val));
        return val;
#else
        return 0;
#endif
    }
};

} // namespace clocking
} // namespace hft
