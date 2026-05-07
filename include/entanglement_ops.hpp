#pragma once

#include "common_types.hpp"
#include <atomic>

// x86-specific intrinsics only on x86 platforms
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define HAS_X86_MONITOR_WAIT 1
#else
    #define HAS_X86_MONITOR_WAIT 0
#endif

namespace hft {
namespace quantum {

/**
 * Entanglement Protocol (Hardware Monitor/Wait)
 * 
 * Concept:
 * "Spooky Action at a Distance" for inter-thread signaling.
 * Standard mutex/condvar involves Syscalls (~1000ns) and OS Scheduler.
 * Spinlocks burn CPU/Power.
 * 
 * Solution:
 * Use Intel `MONITOR` / `MWAIT` (or user-mode `UMONITOR`/`UMWAIT` on Tremont/Alder Lake+).
 * Mechanism:
 * 1. Thread A "Monitors" an address (cache line).
 * 2. Thread A enters low-power sleep (C-State).
 * 3. Thread B "Touches" the address (Store).
 * 4. The CPU Hardware *instantly* wakes Thread A (Latency ~100 cycles vs ~3000 for OS wake).
 * 
 * This creates a practically instant, zero-power link between Strategy and Risk threads.
 */
class EntangledSpinlock {
public:
    EntangledSpinlock() {
        // Clear state
        state_.store(0, std::memory_order_release);
    }

    // Coherence Wait (Receiver)
    // "Collapses" when the entangled bit flips
    inline void wait_for_signal() {
        uint32_t expected = 0;
        
        while (state_.load(std::memory_order_acquire) == expected) {
            // New Intel Instructions (WAITPKG)
            // _umonitor(ptr) -> Sets up hardware address monitoring
            // _umwait(control, timeout) -> Sleeps until write or timeout
            
            #if defined(__WAITPKG__) && HAS_X86_MONITOR_WAIT
                void* ptr = (void*)&state_;
                _umonitor(ptr);
                
                // Re-check to avoid race condition (missed store between load and monitor)
                if (state_.load(std::memory_order_relaxed) != expected) break;
                
                // State 0: C0.2 (Light sleep), Time Limit: 10000 cycles
                uint64_t timeout = hft::timing::read_tsc() + 10000;
                _umwait(0, timeout); 
            #elif defined(__aarch64__) || defined(__arm64__)
                // ARM64: WFE (Wait For Event) - low-power wait
                asm volatile("wfe");
            #else
                // Fallback: just yield
                std::this_thread::yield();
            #endif
        }
    }

    // Spooky Action (Sender)
    inline void signal_change() {
        // Just a store triggers the hardware wake-up
        state_.store(1, std::memory_order_release);
    }
    
    inline void reset() {
        state_.store(0, std::memory_order_release);
    }

private:
    alignas(64) std::atomic<uint32_t> state_;
};

} // namespace quantum
} // namespace hft
