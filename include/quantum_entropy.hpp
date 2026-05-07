#pragma once

#include "common_types.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/random.h> // For entropy estimate? No, for RDRAND.

namespace hft {
namespace entropy {

/**
 * Quantum Entropy Injector
 * 
 * Concept (Novelty):
 * HFT strategies are deterministic. If competitors reverse-engineer your deterministic logic, they can front-run you.
 * 
 * Solution:
 * Inject "True Randomness" (Quantum Entropy) into the execution timing or size.
 * Uses `RDRAND` (Intel Secure Key) which is seeded by on-chip thermal noise (quantum effects).
 * 
 * Capability:
 * - "Jitter" the order submission by 0-50ns randomly.
 * - "Shred" the order size by +/- 1 lot.
 * 
 * This makes the strategy footprint "fuzzy" and mathematically harder to detect by Predatory Algos.
 */
class QuantumMasquerade {
public:
    static inline uint64_t get_quantum_random() {
        uint64_t val;
        // Intel RDRAND instruction
        // Returns 1 on success (entropy available), 0 on underflow
        // We retry in loop? No, that stalls. We fallback to RDTSC.
        
        unsigned char ok;
        #if defined(__x86_64__) || defined(_M_X64)
            __asm__ volatile("rdrand %0; setc %1" : "=r" (val), "=qm" (ok));
        #else
            ok = 0;
        #endif
        
        if (ok) return val;
        
        // Fallback: TSC mixing
        // Not quantum, but fast and unpredictable enough for micro-timing
        #if defined(__x86_64__) || defined(_M_X64)
            return __rdtsc(); 
        #else
            return 0;
        #endif
    }

    // Delay execution by random [0..MaxNanoseconds]
    // Uses spin-wait to preserve CPU cache warmth (vs sleep)
    template<int MaxNanoseconds = 100>
    static inline void jitter_execution() {
        if constexpr (MaxNanoseconds == 0) return;
        
        uint64_t rand = get_quantum_random();
        uint32_t delay_cycles = rand % (MaxNanoseconds * 3); // Approx 3GHz
        
        // Spin
        uint64_t start = __rdtsc();
        while ((__rdtsc() - start) < delay_cycles) {
            _mm_pause(); 
        }
    }
    
    // Obfuscate size: 100 -> 99 or 101
    static inline uint32_t shred_size(uint32_t size) {
        if (size <= 1) return size;
        uint64_t r = get_quantum_random();
        // 50% chance to change, +/- 1
        if (r & 1) {
            return (r & 2) ? size + 1 : size - 1;
        }
        return size;
    }
};

}
}
