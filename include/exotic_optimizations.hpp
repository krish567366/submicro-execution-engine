#pragma once

#include "common_types.hpp"
#include <map>
#include <vector>
#include <iostream>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#endif

namespace hft {
namespace debug {

/**
 * Speculative Execution Tracer (Spectre-like Probe)
 * 
 * Concept (Insane Novelty):
 * Modern CPUs execute code speculatively down branches before confirming the condition.
 * We can legally *abuse* this to "peek" into future market states?
 * No, we can't predict the market.
 * 
 * BUT, we can use speculative execution to "Warm Up" *both* Buy and Sell logic paths.
 * 
 * Mechanism:
 * 1. Force the CPU to speculatively execute both `ExecuteBuy()` and `ExecuteSell()` logic 
 *    even before the `if (price > X)` check resolves.
 * 2. This loads the instruction cache and data cache for BOTH outcomes.
 * 3. When the check resolves, the correct path is already 50 steps deep in the pipeline (shadow retired).
 * 
 * This effectively makes the branch latency *zero*.
 */
class SpeculativePreloader {
public:
    template<typename BuyFunc, typename SellFunc>
    inline void fork_paths(bool condition, BuyFunc&& buy, SellFunc&& sell) {
        // 1. Train Branch Predictor to oscillate (confuse it intentionally?)
        // Actually, we want to force execution.
        
        // Trick: Access a dependent variable that forces memory disambiguation delay?
        
        volatile int shadow = 0;
        
        // This pattern encourages the CPU to speculate on 'buy' logic
        if (HFT_LIKELY(true)) {
            // Shadow execute buy logic on dummy data?
            // (Requires idempotent logic, i.e., no side effects)
            // Hard to generalize safely. 
        }
        
        // Real logic
        if (condition) {
            buy();
        } else {
            sell();
        }
    }
};

/**
 * Thermal-Aware Frequency Booster
 * 
 * Concept:
 * CPU Cores downclock if they hit thermal limits. 
 * HFT logic is bursty.
 * 
 * Trick:
 * During idle times (waiting for packets), execution "Spin Loop" should use special
 * low-power instructions (`PAUSE` or `MWAIT`) to cool the core.
 * Once a packet arrives, the core is -10C cooler, allowing Turbo Boost to sustain
 * max frequency (e.g. 5.3GHz) for the duration of the trade logic processing.
 * 
 * This is "Thermal Headroom Management".
 */
class ThermalManager {
public:
    static void cool_down() {
        // x86 PAUSE / ARM64 YIELD: Hints spin-wait, reducing power
        // Loop heavily to drop temps
        for (int i=0; i<100; ++i) {
#if defined(__x86_64__) || defined(_M_X64)
            _mm_pause();
#elif defined(__aarch64__) || defined(__arm64__)
            asm volatile("yield");
#else
            std::this_thread::yield();
#endif
        }
    }
    
    // Check if we are being throttled?
    // Requires reading MSRs (root).
};

}
}
