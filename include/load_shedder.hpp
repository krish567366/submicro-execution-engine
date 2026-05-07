#pragma once

#include "common_types.hpp"
#include <atomic>
#include <cmath>

namespace hft {
namespace reliability {

/**
 * CoDEL (Controlled Delay) Load Shedder
 * 
 * Concept:
 * When the strategy lags the market, processing old packets is worse than dropping them.
 * This class implements an Active Queue Management (AQM) algorithm inspired by CoDEL.
 * 
 * Logic:
 * - Monitor "Sojourn Time" (Latency = Dequeue Time - Enqueue Time).
 * - If Latency > Target (e.g., 5us) for longer than Interval (e.g., 100us), enter "Drop Mode".
 * - In Drop Mode, drop orders/packets aggressively to drain the queue.
 */
class AdaptiveLoadShedder {
public:
    AdaptiveLoadShedder(uint64_t target_latency_ns = 5000, uint64_t interval_ns = 100000)
        : target_(target_latency_ns), interval_(interval_ns) {
        state_.dropping = false;
        state_.first_above_time = 0;
        state_.next_drop_time = 0;
    }

    // Call on Dequeue
    // returns TRUE if packet should be dropped
    bool should_drop(uint64_t packet_timestamp_ns, uint64_t now_ns) {
        uint64_t sojourn = now_ns - packet_timestamp_ns;
        
        bool fast_enough = (sojourn < target_);
        
        if (fast_enough) {
            state_.dropping = false;
            state_.first_above_time = 0;
            return false;
        }
        
        // Too slow
        if (state_.first_above_time == 0) {
            state_.first_above_time = now_ns + interval_; // Trigger dropping in future
        }
        else if (now_ns >= state_.first_above_time) {
            state_.dropping = true;
            state_.next_drop_time = now_ns + 1000; // Drop schedule
            return true; 
        }
        
        return state_.dropping; 
    }

private:
    uint64_t target_;
    uint64_t interval_;
    
    struct State {
        bool dropping;
        uint64_t first_above_time;
        uint64_t next_drop_time;
    } state_;
};

}
}
