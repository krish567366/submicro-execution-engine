#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <type_traits>

namespace hft {

// ============================================================================
// Lock-Free Single-Producer Single-Consumer (SPSC) Ring Buffer
// Optimized for zero-copy, cache-friendly market data ingestion
// ============================================================================

template<typename T, size_t Capacity>
class LockFreeQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, 
                  "Capacity must be power of 2 for fast modulo");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for zero-copy semantics");
    
public:
    LockFreeQueue() : head_(0), tail_(0) {}
    
    // Disable copy/move
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    
    // ========================================================================
    // Producer: Push (non-blocking if full, returns false)
    // ========================================================================
    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(current_tail);
        
        // Check if queue is full
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        // Zero-copy placement
        buffer_[current_tail] = item;
        
        // Publish the item
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // ========================================================================
    // Producer: Emplace (construct in-place)
    // ========================================================================
    template<typename... Args>
    bool emplace(Args&&... args) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = increment(current_tail);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Construct directly in buffer (true zero-copy)
        new (&buffer_[current_tail]) T(std::forward<Args>(args)...);
        
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // ========================================================================
    // Consumer: Pop (non-blocking if empty, returns false)
    // ========================================================================
    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Check if queue is empty
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }
        
        // Zero-copy read
        item = buffer_[current_head];
        
        // Advance head
        head_.store(increment(current_head), std::memory_order_release);
        return true;
    }
    
    // ========================================================================
    // Consumer: Peek (read without removing)
    // ========================================================================
    const T* peek() const {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return nullptr;  // Queue empty
        }
        
        return &buffer_[current_head];
    }
    
    // ========================================================================
    // Status queries
    // ========================================================================
    bool empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : (Capacity - h + t);
    }
    
    size_t capacity() const { return Capacity - 1; }  // One slot reserved
    
private:
    // Fast modulo using bitwise AND (requires power-of-2 capacity)
    static constexpr size_t increment(size_t idx) {
        return (idx + 1) & (Capacity - 1);
    }
    
    // ========================================================================
    // Data Members (cache-line aligned to prevent false sharing)
    // ========================================================================
    alignas(64) std::atomic<size_t> head_;  // Consumer index
    alignas(64) std::atomic<size_t> tail_;  // Producer index
    alignas(64) std::array<T, Capacity> buffer_;
};

} // namespace hft
