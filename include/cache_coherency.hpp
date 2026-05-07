#pragma once

#include "common_types.hpp"
#include "spin_loop_engine.hpp" // for pin_to_cpu
#include <atomic>

// x86-specific intrinsics only on x86 platforms
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define HAS_X86_CACHE_INTRINSICS 1
#else
    #define HAS_X86_CACHE_INTRINSICS 0
#endif

namespace hft {
namespace hardware {

/**
 * Cache Coherence Optimizer (The "Polite" Neighbor)
 * 
 * Concept:
 * On high-core-count CPUs (Xeon/Epyc), the "Mesh/Ring Interconnect" gets saturated 
 * if execution threads fight for cache line ownership (MESI protocol).
 * 
 * Solutions:
 * 1. Store Buffer Forwarding Enhancer: Pad writes to prevent False Sharing.
 * 2. CLFLUSHOPT: Explicitly flush lines to memory when modifying shared flags 
 *    so the listener sees it immediately without waiting for directory snoops.
 * 3. Non-Temporal Stores (MOVNT): Write directly to DRAM, bypassing cache for Log Ring Buffers 
 *    (preventing pollution of L3 cache with log trash).
 */
class CacheOptimizer {
public:
    // Force cache line flush (Invalidate in all other cores)
    static inline void flush_line(void* ptr) {
#if HAS_X86_CACHE_INTRINSICS
        _mm_clflushopt(ptr);
#elif defined(__aarch64__) || defined(__arm64__)
        // ARM64: DC CVAC (Data Cache Clean by Virtual Address to PoC)
        asm volatile("dc cvac, %0" : : "r"(ptr) : "memory");
#else
        // Generic: Use memory barrier
        std::atomic_thread_fence(std::memory_order_seq_cst);
        (void)ptr;
#endif
    }

    // Non-Temporal Copy (Zero Cache Pollution memcpy)
    // Use for Logging to Ring Buffer
    static inline void stream_copy(void* dst, const void* src, size_t len) {
#if HAS_X86_CACHE_INTRINSICS
        // Assume 64-byte aligned
        const __m256i* s = reinterpret_cast<const __m256i*>(src);
        __m256i* d = reinterpret_cast<__m256i*>(dst);
        
        size_t n = len / 32;
        for (size_t i = 0; i < n; ++i) {
            __m256i v = _mm256_load_si256(&s[i]);
            _mm256_stream_si256(&d[i], v); // MOVNTDQ
        }
        _mm_sfence();
#else
        // Fallback: regular memcpy
        std::memcpy(dst, src, len);
#endif
    }

    // Software Prefetch for "Next Likely" cache line
    static inline void prefetch_l1(void* ptr) {
#if HAS_X86_CACHE_INTRINSICS
        _mm_prefetch((const char*)ptr, _MM_HINT_T0);
#elif defined(__aarch64__) || defined(__arm64__)
        // ARM64: PRFM (Prefetch Memory) to L1
        asm volatile("prfm pldl1keep, [%0]" : : "r"(ptr));
#else
        (void)ptr;
#endif
    }
};

/**
 * CPU Core Parking Management
 * 
 * Problem:
 * OS Scheduler interrupts (context switches) destroy L1/L2 cache locality.
 * 
 * Solution:
 * "Park" the thread in a tight loop on an isolated core.
 * This class ensures the thread *never* yields, keeping the OS scheduler away.
 */
class CoreKeeper {
public:
    CoreKeeper(int core_id) {
        hft::spin_loop::pin_to_cpu(core_id);
    }

    // Anti-SMI (System Management Interrupt) Detector
    // SMI takes 100us+. We can't stop it (BIOS/HW), but we can verify it happened
    // by checking TSC jumps.
    bool detect_latency_spike(uint64_t threshold_cycles = 5000) {
        uint64_t start = hft::timing::read_tsc();
        // Tight loop
        for(int i=0; i<100; ++i) {
#if HAS_X86_CACHE_INTRINSICS
            _mm_pause();
#elif defined(__aarch64__) || defined(__arm64__)
            // ARM64: YIELD instruction
            asm volatile("yield");
#endif
        }
        uint64_t end = hft::timing::read_tsc();
        
        return (end - start) > threshold_cycles;
    }
};

}
}
