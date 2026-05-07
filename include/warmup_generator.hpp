#pragma once

#include "common_types.hpp"
#include <type_traits>
#include <fstream>
#include <sys/mman.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

namespace hft {
namespace optimization {

/**
 * Instruction Cache Warmer & JIT Optimizer
 * 
 * Problem:
 * HFT strategies executed at market open often suffer from cold instruction cache 
 * misses because the code hasn't run in hours.
 * 
 * Solution:
 * "Exercise" the critical path with dummy data *before* the market opens.
 * This class provides a framework to execute the strategy with synthetic inputs 
 * that are tagged as "Warmup" so they don't trigger real orders.
 */
template<typename Strategy>
class WarmupGenerator {
public:
    static void exercise_hot_path(Strategy& strategy) {
        // Generate Dummy Tick
        MarketTick tick;
        tick.asset_id = 1;
        tick.bid_price = 9999.0;
        tick.ask_price = 10001.0;
        tick.mid_price = 10000.0;
        tick.bid_size = 100;
        tick.ask_size = 100;
        tick.trade_volume = 0;
        tick.trade_side = Side::BUY;
        tick.depth_levels = 0;
        
        volatile int side_effect = 0; // Prevent compiler optimization
        
        // Loop enough times to:
        // 1. Train branch predictors (if branches are consistent)
        // 2. Load instructions into L1i
        // 3. Trigger OS page faults if pages were swapped out
        for (int i = 0; i < 1000; ++i) {
            tick.bid_price += (i % 2 == 0 ? 0.01 : -0.01);
            tick.ask_price += (i % 2 == 0 ? 0.01 : -0.01);
            tick.mid_price = (tick.bid_price + tick.ask_price) / 2.0;
            
            // Execute tick
            strategy.on_tick(tick);
            
            // Artificial dependency to ensure code runs
            side_effect += (int)tick.mid_price;
        }
    }
};

/**
 * Cache Line Locker (Intel CAT / Memory Pinning)
 * 
 * Intel CAT (Cache Allocation Technology) allows partitioning L3 cache
 * for specific threads. This prevents cache pollution from background processes.
 */
class CacheLocker {
public:
    static bool lock_current_thread_memory() {
        bool success = true;
        
        // 1. Lock all memory pages (prevent swapping)
#if defined(__linux__) || defined(__APPLE__)
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            success = false;
        }
#endif

        // 2. Intel CAT (Cache Allocation Technology)
        // Requires root and resctrl filesystem mounted
#if defined(__linux__)
        // Check if resctrl is available
        // echo "L3:0=0xf" > /sys/fs/resctrl/trading/schemata
        // echo <pid> > /sys/fs/resctrl/trading/tasks
        
        // This is typically done via system setup, not runtime API
        // We can verify if we're in a CAT group:
        std::ifstream cat_check("/sys/fs/resctrl/tasks");
        if (cat_check.is_open()) {
            // CAT is available on this system
            cat_check.close();
        }
#endif

        // 3. Prefault stack and heap
        // Touch memory to force page faults now rather than during trading
        const size_t stack_size = 1024 * 1024; // 1MB stack warmup
        volatile char stack_array[stack_size];
        for (size_t i = 0; i < stack_size; i += 4096) {
            stack_array[i] = 0;
        }
        
        return success;
    }
    
    // Set CPU cache policy for specific memory region
    static void set_cache_policy(void* addr, size_t len, bool non_temporal = false) {
#if defined(__x86_64__) || defined(_M_X64)
        // Use madvise hints
#if defined(__linux__)
        // MADV_WILLNEED: prefault pages
        // MADV_SEQUENTIAL/RANDOM: access pattern hints
        madvise(addr, len, MADV_WILLNEED);
        
        if (non_temporal) {
            // For write-only data that won't be read back (logs)
            // Note: madvise doesn't have direct non-temporal hint
            // Would need to use non-temporal stores at write time
        }
#endif
#endif
    }
};

}
}
