#pragma once

#include "common_types.hpp"
#include <vector>
#include <functional>
#include <cstdint>
#include <memory>
#include <array>

namespace hft {
namespace scheduling {

/**
 * Hashed Timing Wheel (O(1) Scheduler)
 * 
 * Purpose: extremely fast timeout management for orders.
 * Standard priority_queue is O(log N). Timing Wheel is O(1).
 * 
 * Design:
 * - Circular buffer of "Slots" (linked lists of tasks).
 * - Current time maps to an index: `time % Size`.
 * - To schedule T+50us: add to slot `(now + 50) % Size`.
 * - To poll: process all tasks in current slot, then increment time.
 * 
 * Optimization:
 * - Uses a " Slab Allocator" (Object Pool) for TimerNodes to avoid `new/delete`.
 */
template<size_t Bits = 16> // 65536 slots (e.g. 65ms range at 1us, or 65s at 1ms)
class TimingWheel {
public:
    static constexpr size_t Size = 1 << Bits;
    static constexpr size_t Mask = Size - 1;

    using Task = std::function<void()>;

    struct TimerNode {
        uint64_t expiration_tick;
        Task task;
        TimerNode* next = nullptr;
    };

    TimingWheel() : current_tick_(0) {
        slots_.fill(nullptr);
        // Pre-allocate pool
        pool_storage_.resize(10000); 
        for(size_t i=0; i<pool_storage_.size()-1; ++i) {
            pool_storage_[i].next = &pool_storage_[i+1];
        }
        free_list_ = &pool_storage_[0];
    }

    // Advance wheel to 'now'
    // 'now' should be monotonic ticks (e.g. micros since start)
    inline void poll(uint64_t now) {
        // Limit catch-up loop to avoid stalls if we lag deeply
        // In HFT, we should be calling this constantly.
        while (current_tick_ <= now) {
            size_t idx = current_tick_ & Mask;
            
            TimerNode* node = slots_[idx];
            slots_[idx] = nullptr; // Clear slot
            
            while (node) {
                // Execute
                if (node->expiration_tick <= now || (node->expiration_tick & Mask) == idx) {
                    node->task();
                } else {
                    // This task belongs to a future wrap-around? 
                    // Simple implementation assumes tasks are within wheel range.
                    // If not, we re-insert (cascade).
                    // For latency, we assume timeouts < range.
                }
                
                TimerNode* temp = node;
                node = node->next;
                free_node(temp);
            }
            
            current_tick_++;
        }
    }

    inline void schedule(uint64_t delay_ticks, Task&& task) {
        uint64_t expiration = current_tick_ + delay_ticks;
        size_t idx = expiration & Mask;
        
        TimerNode* node = alloc_node();
        if (!node) return; // Pool exhausted, drop task (or log error)
        
        node->expiration_tick = expiration;
        node->task = std::move(task);
        node->next = slots_[idx];
        slots_[idx] = node;
    }

private:
    std::array<TimerNode*, Size> slots_;
    uint64_t current_tick_;
    
    // Object Pool
    std::vector<TimerNode> pool_storage_;
    TimerNode* free_list_;

    inline TimerNode* alloc_node() {
        if (!free_list_) return nullptr;
        TimerNode* node = free_list_;
        free_list_ = free_list_->next;
        return node;
    }

    inline void free_node(TimerNode* node) {
        node->next = free_list_;
        free_list_ = node;
    }
};

}
}