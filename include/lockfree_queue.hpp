#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
    #define HFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define HFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define HFT_LIKELY(x)   (x)
    #define HFT_UNLIKELY(x) (x)
#endif

namespace hft {

/**
 * High-Performance Single-Producer Single-Consumer (SPSC) Queue
 * 
 * Optimization Techniques:
 * 1. Shadow Indices: Reduces atomic load contention. Producer caches 'head', Consumer caches 'tail'.
 *    - Producer only fetches 'head' (cache miss) when queue looks full.
 *    - Consumer only fetches 'tail' (cache miss) when queue looks empty.
 * 2. 128-byte Cache Line Padding: Prevents adjacent-line prefetchers from causing false sharing.
 * 3. Enforced Power-of-2 Size: Allows fast bitwise modulo operations.
 */
template<typename T, size_t N>
class LockFreeQueue {
    static_assert((N & (N - 1)) == 0, "size must be power of 2");

private:
    // Consumer-owned cache line
    struct alignas(128) ConsumerLine {
        std::atomic<size_t> head{0};
        size_t tail_cache{0}; // Shadow copy of tail
    };

    // Producer-owned cache line
    struct alignas(128) ProducerLine {
        std::atomic<size_t> tail{0};
        size_t head_cache{0}; // Shadow copy of head
    };

    ConsumerLine consumer_;
    ProducerLine producer_;
    
    // Aligned storage to avoid UB with non-trivial types
    struct Node {
        alignas(T) char data[sizeof(T)];
    };
    alignas(128) Node buf_[N]; // Buffer separate from control vars

public:
    LockFreeQueue() = default;
    
    ~LockFreeQueue() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            T item;
            while (pop(item)) {}
        }
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    /**
     * Producer: Push item
     * Only loads atomic Consumer.head if queue appears full based on local cache.
     */
    template<typename U>
    bool push(U&& item) {
        const size_t t = producer_.tail.load(std::memory_order_relaxed);
        const size_t next_t = (t + 1) & (N - 1);

        // Usage of shadow head avoids cache miss in common case
        if (HFT_UNLIKELY(next_t == producer_.head_cache)) {
            // Queue looks full, refresh shadow copy from consumer
            producer_.head_cache = consumer_.head.load(std::memory_order_acquire);
            
            // Still full?
            if (HFT_UNLIKELY(next_t == producer_.head_cache)) {
                return false;
            }
        }

        new (&buf_[t].data) T(std::forward<U>(item));
        
        // Release ordering ensures consumer sees data before seeing new tail
        producer_.tail.store(next_t, std::memory_order_release);
        return true;
    }

    /**
     * Consumer: Pop item
     * Only loads atomic Producer.tail if queue appears empty based on local cache.
     */
    bool pop(T& item) {
        const size_t h = consumer_.head.load(std::memory_order_relaxed);

        // Usage of shadow tail avoids cache miss in common case
        if (HFT_UNLIKELY(h == consumer_.tail_cache)) {
            // Queue looks empty, refresh shadow copy from producer
            consumer_.tail_cache = producer_.tail.load(std::memory_order_acquire);
            
            // Still empty?
            if (HFT_UNLIKELY(h == consumer_.tail_cache)) {
                return false;
            }
        }

        T* ptr = reinterpret_cast<T*>(&buf_[h].data);
        item = std::move(*ptr);
        ptr->~T();
        
        // Release ordering ensures producer sees free slot before seeing updated head
        consumer_.head.store((h + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    // Direct construction
    template<typename... Args>
    bool emplace(Args&&... args) {
        const size_t t = producer_.tail.load(std::memory_order_relaxed);
        const size_t next_t = (t + 1) & (N - 1);

        if (HFT_UNLIKELY(next_t == producer_.head_cache)) {
            producer_.head_cache = consumer_.head.load(std::memory_order_acquire);
            if (HFT_UNLIKELY(next_t == producer_.head_cache)) {
                return false;
            }
        }

        new (&buf_[t].data) T(std::forward<Args>(args)...);
        producer_.tail.store(next_t, std::memory_order_release);
        return true;
    }

    // Peek (read-only access to head)
    const T* peek() const {
        const size_t h = consumer_.head.load(std::memory_order_relaxed);
        
        // Use acquire here because we might need to synchronise with producer
        // Note: const methods can't update shadow cache easily without mutable
        // So we do a standard check
        if (h == producer_.tail.load(std::memory_order_acquire)) {
            return nullptr;
        }

        return reinterpret_cast<const T*>(&buf_[h].data);
    }

    bool empty() const {
        return consumer_.head.load(std::memory_order_acquire) == producer_.tail.load(std::memory_order_acquire);
    }

    size_t size() const {
        auto h = consumer_.head.load(std::memory_order_acquire);
        auto t = producer_.tail.load(std::memory_order_acquire);
        
        if (t >= h) return t - h;
        return N - (h - t);
    }
    
    // Batch operations for higher throughput
    template<typename Func>
    size_t consume_all(Func&& func) {
        size_t count = 0;
        size_t h = consumer_.head.load(std::memory_order_relaxed);
        
        // Refresh tail once
        size_t t = producer_.tail.load(std::memory_order_acquire);
        consumer_.tail_cache = t;
        
        while (h != t) {
             T* ptr = reinterpret_cast<T*>(&buf_[h].data);
             func(std::move(*ptr));
             ptr->~T();
             h = (h + 1) & (N - 1);
             count++;
        }
        
        if (count > 0) {
            consumer_.head.store(h, std::memory_order_release);
        }
        return count;
    }

    size_t capacity() const { return N - 1; }
};

// Alias for compatibility
template<typename T, size_t N>
using SPSCQueue = LockFreeQueue<T, N>;

}