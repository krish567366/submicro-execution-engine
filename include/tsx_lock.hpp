#pragma once

#include "common_types.hpp"
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define HAS_TSX 1
#else
#define HAS_TSX 0
#endif

namespace hft {
namespace concurrency {

/**
 * Hardware Lock Elision (Portable Fallback)
 */
class RestrictedTransactionLock {
public:
    RestrictedTransactionLock() {
        lock_.store(0, std::memory_order_release);
    }

    inline void acquire() {
#if HAS_TSX
        for (int i = 0; i < 3; ++i) {
            unsigned status = _xbegin();
            if (status == _XBEGIN_STARTED) {
                if (lock_.load(std::memory_order_relaxed) == 0) return;
                _xabort(0xFF); 
            }
        }
#endif
        // Fallback: Standard Spinlock
        while (lock_.exchange(1, std::memory_order_acquire) == 1) {
#if HAS_TSX
            _mm_pause();
#else
            // No-op or yield
            asm volatile("yield" ::: "memory");
#endif
        }
    }

    inline void release() {
#if HAS_TSX
        if (_xtest()) {
            _xend(); return;
        }
#endif
        lock_.store(0, std::memory_order_release);
    }

private:
    alignas(64) std::atomic<uint32_t> lock_;
};

} }
