#pragma once

#include "common_types.hpp"
#include <cstdint>
#include <unistd.h>
#include <sys/syscall.h>
#ifdef __linux__
    #include <linux/perf_event.h>
    typedef struct perf_event_mmap_page perf_mmap_page_t;
#else
    // Dummy type for non-Linux platforms
    struct perf_mmap_page_t { uint32_t lock; uint32_t index; uint64_t offset; };
#endif
#include <sys/mman.h>
#include <vector>
#include <cstring>
#include <iostream>

namespace hft {
namespace profiling {

/**
 * RDPMC (Read Performance Monitoring Counters) Wrapper
 * 
 * "State of the Art" Profiling:
 * Standard profiling (e.g. valid 'perf') uses interrupts, which cause latency spikes (~1500ns).
 * We use `perf_event_open` to configure the CPU counters, then map them to userspace.
 * Finally, we use the `rdpmc` assembly instruction to read LLC Cache Misses / Mispredicted Branches
 * in < 30 cycles, without ANY system calls or interrupts.
 * 
 * NOTE: On macOS, hardware counters are not supported via perf_event. Methods will return 0.
 */
class HardwareCounter {
public:
    enum class Type {
        INSTRUCTIONS,
        CYCLES,
        CACHE_MISSES,
        BRANCH_MISSES
    };

    HardwareCounter() : fd_(-1), perf_page_(nullptr) {}

    bool init(Type type) {
#ifdef __linux__
        struct perf_event_attr pe;
        std::memset(&pe, 0, sizeof(pe));
        pe.size = sizeof(pe);
        pe.disabled = 1; // Start disabled
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        switch (type) {
            case Type::INSTRUCTIONS: 
                pe.type = PERF_TYPE_HARDWARE; 
                pe.config = PERF_COUNT_HW_INSTRUCTIONS; 
                break;
            case Type::CYCLES:
                pe.type = PERF_TYPE_HARDWARE;
                pe.config = PERF_COUNT_HW_CPU_CYCLES;
                break;
            case Type::CACHE_MISSES:
                pe.type = PERF_TYPE_HARDWARE;
                pe.config = PERF_COUNT_HW_CACHE_MISSES;
                break;
            case Type::BRANCH_MISSES:
                pe.type = PERF_TYPE_HARDWARE;
                pe.config = PERF_COUNT_HW_BRANCH_MISSES; // Critical for HFT logic checks
                break;
        }

        // Open perf event for current thread (pid=0), current cpu (-1 = any? no, pin to cpu usually)
        // For accurate HFT, we assume thread is already pinned.
        fd_ = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
        if (fd_ == -1) {
            // Permission denied is common (check /proc/sys/kernel/perf_event_paranoid)
            return false;
        }

        // Map the page for RDPMC support
        perf_page_ = (perf_mmap_page_t*)mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED, fd_, 0);
        if (perf_page_ == MAP_FAILED) {
            close(fd_);
            return false;
        }

        // Resume counting
        ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
        return true;
#else
        // macOS: Hardware counters not supported via perf_event
        (void)type;  // Suppress unused parameter warning
        return false;
#endif
    }

    ~HardwareCounter() {
        if (fd_ != -1) close(fd_);
    }

    // ULTRA-FAST READ (~10-30 cycles)
    inline uint64_t read() const {
        if (!perf_page_) return 0;

#ifdef __linux__
        uint32_t seq, index;
        uint64_t count;

        // Sequence lock to prevent reading during update
        do {
            seq = perf_page_->lock;
            std::atomic_thread_fence(std::memory_order_acquire);
            index = perf_page_->index;
            uint64_t offset = perf_page_->offset;
            
            if (index == 0) return 0; // Not running

#if defined(__x86_64__) || defined(_M_X64)
            // rdpmc instruction (x86 only)
            // ecx = index - 1
            uint32_t pmc_idx = index - 1;
            uint64_t pmc_val;
            __asm__ volatile("rdpmc" : "=A" (pmc_val) : "c" (pmc_idx));
            
            count = offset + pmc_val;
#else
            // ARM64 or other: cannot use rdpmc, return offset only
            count = offset;
#endif
            std::atomic_thread_fence(std::memory_order_acquire);

        } while (perf_page_->lock != seq);

        return count;
#else
        return 0; // Non-Linux platforms
#endif
    }

private:
    int fd_;
    perf_mmap_page_t* perf_page_;
};

class MetricsCollector {
public:
    static MetricsCollector& instance() {
        static MetricsCollector inst;
        return inst;
    }

    void init() {
        l1_missers_.init(HardwareCounter::Type::CACHE_MISSES);
        branch_missers_.init(HardwareCounter::Type::BRANCH_MISSES);
    }

    // Checkpoint: call this at start of hot loop
    inline void start_frame() {
        start_cache_ = l1_missers_.read();
        start_branch_ = branch_missers_.read();
    }

    // Checkpoint: call this at end
    inline void end_frame() {
        uint64_t misses = l1_missers_.read() - start_cache_;
        uint64_t branches = branch_missers_.read() - start_branch_;
        
        if (misses > 100 || branches > 100) {
            // Log anomaly?
            // "High micro-architectural pressure detected"
        }
    }

private:
    HardwareCounter l1_missers_;
    HardwareCounter branch_missers_;
    uint64_t start_cache_ = 0;
    uint64_t start_branch_ = 0;
};

} // namespace profiling
} // namespace hft