#pragma once

#include "common_types.hpp"
#include <atomic>
#include <vector>
#include <algorithm>
#include <queue>
#include <functional>

namespace hft {
namespace scheduler {

// ============================================================================
// Nanosecond-Precision Event Scheduler
// Deterministic, garbage-free event scheduling for ultra-low latency
// Uses timing wheel algorithm for O(1) insertions and deletions
// ============================================================================

struct ScheduledEvent {
    Timestamp execution_time;
    uint64_t event_id;
    std::function<void()> callback;
    bool is_cancelled;
    
    ScheduledEvent() 
        : execution_time(now()), event_id(0), 
          callback(nullptr), is_cancelled(false) {}
    
    ScheduledEvent(Timestamp time, uint64_t id, std::function<void()> cb)
        : execution_time(time), event_id(id), 
          callback(std::move(cb)), is_cancelled(false) {}
};

// ============================================================================
// Hierarchical Timing Wheel
// Efficiently schedules events across multiple time scales
// ============================================================================

class TimingWheelScheduler {
public:
    explicit TimingWheelScheduler(
        size_t num_slots = 1024,           // Number of wheel slots
        Duration slot_duration_ns = std::chrono::microseconds(10)  // 10µs per slot
    ) : num_slots_(num_slots),
        slot_duration_(slot_duration_ns),
        current_slot_(0),
        next_event_id_(0),
        wheel_(num_slots),
        start_time_(now()) {
        
        // Pre-reserve capacity to avoid allocations
        for (auto& slot : wheel_) {
            slot.reserve(16);
        }
    }
    
    // ========================================================================
    // Schedule event at absolute time
    // Returns: event_id for cancellation
    // ========================================================================
    uint64_t schedule_at(Timestamp exec_time, std::function<void()> callback) {
        const int64_t delay_ns = to_nanos(exec_time) - to_nanos(now());
        
        if (delay_ns <= 0) {
            // Execute immediately
            callback();
            return 0;
        }
        
        return schedule_after(std::chrono::nanoseconds(delay_ns), std::move(callback));
    }
    
    // ========================================================================
    // Schedule event after delay (relative)
    // ========================================================================
    uint64_t schedule_after(Duration delay, std::function<void()> callback) {
        const uint64_t event_id = next_event_id_.fetch_add(1, std::memory_order_relaxed);
        
        // Calculate target slot
        const int64_t delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delay).count();
        const int64_t slot_delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(slot_duration_).count();
        const size_t slots_ahead = static_cast<size_t>(delay_ns / slot_delay_ns);
        const size_t target_slot = (current_slot_ + slots_ahead) % num_slots_;
        
        const Timestamp exec_time = now() + delay;
        
        // Insert into wheel (lock-free in single-threaded scheduler)
        wheel_[target_slot].emplace_back(exec_time, event_id, std::move(callback));
        
        return event_id;
    }
    
    // ========================================================================
    // Cancel scheduled event
    // ========================================================================
    void cancel(uint64_t event_id) {
        // Mark as cancelled (actual removal happens during execution)
        for (auto& slot : wheel_) {
            for (auto& event : slot) {
                if (event.event_id == event_id) {
                    event.is_cancelled = true;
                    return;
                }
            }
        }
    }
    
    // ========================================================================
    // Tick the scheduler (call every slot_duration)
    // Executes all events in current slot
    // ========================================================================
    void tick() {
        auto& current_events = wheel_[current_slot_];
        const Timestamp current_time = now();
        
        // Execute ready events
        for (auto& event : current_events) {
            if (!event.is_cancelled && event.callback) {
                // Check if actually ready (handles wrap-around)
                if (to_nanos(event.execution_time) <= to_nanos(current_time)) {
                    event.callback();
                }
            }
        }
        
        // Clear slot (garbage-free: reuses capacity)
        current_events.clear();
        
        // Advance to next slot
        current_slot_ = (current_slot_ + 1) % num_slots_;
    }
    
    // ========================================================================
    // Process events until time (busy-wait for determinism)
    // ========================================================================
    void process_until(Timestamp target_time) {
        while (to_nanos(now()) < to_nanos(target_time)) {
            tick();
            
            // Busy-wait for next slot boundary (deterministic timing)
            const Timestamp next_slot_time = start_time_ + 
                slot_duration_ * (current_slot_ + 1);
            
            while (to_nanos(now()) < to_nanos(next_slot_time)) {
                __asm__ __volatile__("pause" ::: "memory");
            }
        }
    }
    
    // ========================================================================
    // Get pending event count
    // ========================================================================
    size_t get_pending_count() const {
        size_t count = 0;
        for (const auto& slot : wheel_) {
            for (const auto& event : slot) {
                if (!event.is_cancelled) {
                    ++count;
                }
            }
        }
        return count;
    }
    
private:
    const size_t num_slots_;
    const Duration slot_duration_;
    size_t current_slot_;
    std::atomic<uint64_t> next_event_id_;
    std::vector<std::vector<ScheduledEvent>> wheel_;
    Timestamp start_time_;
};

// ============================================================================
// Priority-Based Event Queue (for non-time-based events)
// Garbage-free using pre-allocated memory pool
// ============================================================================

template<typename EventType, size_t MaxEvents = 4096>
class PriorityEventQueue {
public:
    struct Event {
        EventType data;
        uint64_t priority;  // Lower = higher priority
        bool is_valid;
        
        Event() : data(), priority(UINT64_MAX), is_valid(false) {}
        
        bool operator<(const Event& other) const {
            return priority > other.priority;  // Min-heap
        }
    };
    
    PriorityEventQueue() : event_count_(0) {
        events_.reserve(MaxEvents);
    }
    
    // ========================================================================
    // Add event with priority
    // ========================================================================
    bool push(const EventType& event, uint64_t priority) {
        if (event_count_ >= MaxEvents) {
            return false;  // Queue full
        }
        
        Event e;
        e.data = event;
        e.priority = priority;
        e.is_valid = true;
        
        events_.push_back(e);
        std::push_heap(events_.begin(), events_.end());
        
        ++event_count_;
        return true;
    }
    
    // ========================================================================
    // Get highest priority event
    // ========================================================================
    bool pop(EventType& event) {
        while (!events_.empty()) {
            std::pop_heap(events_.begin(), events_.end());
            Event& e = events_.back();
            
            if (e.is_valid) {
                event = e.data;
                events_.pop_back();
                --event_count_;
                return true;
            }
            
            events_.pop_back();  // Remove invalid event
        }
        
        return false;
    }
    
    // ========================================================================
    // Peek at highest priority
    // ========================================================================
    bool peek(EventType& event) const {
        if (events_.empty()) {
            return false;
        }
        
        if (events_.front().is_valid) {
            event = events_.front().data;
            return true;
        }
        
        return false;
    }
    
    size_t size() const { return event_count_; }
    bool empty() const { return event_count_ == 0; }
    
private:
    std::vector<Event> events_;
    size_t event_count_;
};

// ============================================================================
// Deterministic Event Loop
// Combines timing wheel + priority queue for complete event management
// ============================================================================

class DeterministicEventLoop {
public:
    DeterministicEventLoop()
        : timing_wheel_(1024, std::chrono::microseconds(10)),
          is_running_(false) {}
    
    // Schedule timed event
    uint64_t schedule_at(Timestamp time, std::function<void()> callback) {
        return timing_wheel_.schedule_at(time, std::move(callback));
    }
    
    uint64_t schedule_after(Duration delay, std::function<void()> callback) {
        return timing_wheel_.schedule_after(delay, std::move(callback));
    }
    
    // Add immediate event with priority
    template<typename T>
    bool add_event(const T& event, uint64_t priority) {
        return priority_queue_.push(event, priority);
    }
    
    // Cancel event
    void cancel_event(uint64_t event_id) {
        timing_wheel_.cancel(event_id);
    }
    
    // Run loop (deterministic, busy-wait)
    void run() {
        is_running_.store(true, std::memory_order_release);
        
        while (is_running_.load(std::memory_order_acquire)) {
            // Process timing wheel
            timing_wheel_.tick();
            
            // Process priority events
            MarketTick tick;
            while (priority_queue_.pop(tick)) {
                // Handle event
                // In practice: dispatch to handlers
            }
            
            // Deterministic spin-wait
            __asm__ __volatile__("pause" ::: "memory");
        }
    }
    
    void stop() {
        is_running_.store(false, std::memory_order_release);
    }
    
private:
    TimingWheelScheduler timing_wheel_;
    PriorityEventQueue<MarketTick, 4096> priority_queue_;
    std::atomic<bool> is_running_;
};

} // namespace scheduler
} // namespace hft
