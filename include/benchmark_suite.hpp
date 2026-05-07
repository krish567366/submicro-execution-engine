#pragma once

#include "common_types.hpp"
#include "spin_loop_engine.hpp"
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>

namespace hft {
namespace bench {

/**
 * Micro-Benchmark Suite
 * 
 * Measures the "Time to React" for various components.
 */
class Benchmark {
public:
    static void run_latencies() {
        std::cout << "==== HFT BENCHMARK SUITE ====" << std::endl;
        
        bench_rdtsc();
        bench_spin_loop();
        bench_math_log();
    }

private:
    static inline uint64_t rdtsc() {
        #if defined(__x86_64__) || defined(_M_X64)
        unsigned int lo, hi;
        __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
        #else
        return 0; // ARM needs cntvct_el0
        #endif
    }

    static void bench_rdtsc() {
        constexpr int ITERS = 1000000;
        uint64_t start = rdtsc();
        for(int i=0; i<ITERS; ++i) {
            uint64_t t = rdtsc();
            (void)t;
        }
        uint64_t end = rdtsc();
        double ns_per_call = (double)(end - start) / ITERS / 3.0; // Assuming 3GHz
        std::cout << "RDTSC Overhead: " << ns_per_call << " ns (approx)" << std::endl; 
    }

    static void bench_math_log() {
        constexpr int ITERS = 1000000;
        double sum = 0;
        
        auto& lut = hft::spin_loop::get_ln_lut();
        
        uint64_t start = rdtsc();
        for(int i=0; i<ITERS; ++i) {
            sum += lut.lookup(1.0 + (i * 0.00001));
        }
        uint64_t end = rdtsc();
        
        std::cout << "Fast Log LUT:   " << (end - start)/ITERS << " cycles/call. Sum: " << sum << std::endl;
    }
    
    static void bench_spin_loop() {
        // Measure inter-call latency?
        // ...
    }
};

}
}