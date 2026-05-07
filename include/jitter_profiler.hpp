#pragma once

#include "common_types.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

// Unified TSC reading function for all platforms
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <x86intrin.h>
    namespace hft { namespace timing {
        inline uint64_t read_tsc() { return __rdtsc(); }
    }}
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    // ARM64: Use CNTVCT_EL0 (Virtual Count register)
    namespace hft { namespace timing {
        inline uint64_t read_tsc() {
            uint64_t val;
            asm volatile("mrs %0, cntvct_el0" : "=r"(val));
            return val;
        }
    }}
#else
    #error "Unsupported architecture - need cycle counter implementation"
#endif

namespace hft {
namespace profiling {

/**
 * Nano-Scale Jitter Profiler
 * 
 * Measures "Inter-Instruction Latency".
 * Used to detect:
 * 1. CPU throttling / frequency scaling changes
 * 2. Unlucky cache line evictions (LLC miss = 50ns spike)
 * 3. OS scheduler preemption (1000ns+ spike)
 * 
 * Usage:
 * {
 *    auto scope = JitterProbe("OrderLogic");
 *    // ... code ...
 * }
 */
class JitterProfiler {
public:
    static constexpr size_t MAX_SAMPLES = 100000;

    static JitterProfiler& instance() {
        static JitterProfiler inst;
        return inst;
    }

    void record(const char* tag, uint64_t cycles) {
        if (s_idx_ < MAX_SAMPLES) {
            samples_[s_idx_++] = {tag, cycles};
        }
    }

    void dump_report() {
        // Group by tag and print stats
        // ... (simplified)
        std::cout << "JITTER REPORT (Cycles):" << std::endl;
        // Logic to compute simple stats
        // In prod this would be dumping binary to disk
    }

private:
    struct Sample {
        const char* tag;
        uint64_t cycles;
    };
    
    std::vector<Sample> samples_; // Pre-allocated in constructor
    size_t s_idx_ = 0;

    JitterProfiler() {
        samples_.resize(MAX_SAMPLES);
    }
};

class JitterProbe {
public:
    inline JitterProbe(const char* name) : name_(name) {
        start_ = hft::timing::read_tsc();
    }

    inline ~JitterProbe() {
        uint64_t delta = hft::timing::read_tsc() - start_;
        // Only record if it looks like an outlier (> 200 cycles?)
        // Or record everything for histogram
        JitterProfiler::instance().record(name_, delta);
    }

private:
    const char* name_;
    uint64_t start_;
};

}
}
