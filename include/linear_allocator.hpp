#pragma once

#include "common_types.hpp"
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <cstring>
#include <vector>

#include "huge_page_allocator.hpp"

namespace hft {
namespace memory {

/**
 * Resetting Linear Allocator (HugePage Arena)
 */
template<size_t SizeBytes = 2 * 1024 * 1024> 
class LinearAllocator {
public:
    LinearAllocator() : offset_(0) {
        // Allocate via HugePage back-end
        base_ptr_ = static_cast<uint8_t*>(HugePageAllocator::alloc(SizeBytes));
        if (!base_ptr_) {
             throw std::runtime_error("Failed to allocate HugePage arena");
        }
    }

    ~LinearAllocator() {
        HugePageAllocator::dealloc(base_ptr_, SizeBytes);
    }

    // Reset for next tick - O(1)
    inline void reset() {
        offset_ = 0;
    }

    // Allocate raw bytes
    inline void* alloc(size_t size, size_t alignment = 8) {
        // Calculate alignment padding
        uintptr_t current_addr = reinterpret_cast<uintptr_t>(base_ptr_ + offset_);
        uintptr_t aligned_addr = (current_addr + (alignment - 1)) & ~(alignment - 1);
        size_t padding = aligned_addr - current_addr;
        
        if (offset_ + padding + size > SizeBytes) {
            // Buffer overflow - in prod this should just return nullptr or throw specific error
            // For HFT, we usually dimension so this NEVER happens.
            return nullptr; 
        }

        offset_ += padding;
        void* ptr = base_ptr_ + offset_;
        offset_ += size;
        return ptr;
    }

    // Allocator concept for STL containers
    template<typename T, typename... Args>
    inline T* make(Args&&... args) {
        void* ptr = alloc(sizeof(T), alignof(T));
        if (!ptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // Array allocation (standard C-style)
    template<typename T>
    inline T* alloc_array(size_t count) {
        void* ptr = alloc(sizeof(T) * count, alignof(T));
        return static_cast<T*>(ptr);
    }

    // RAII-style scope watermark (for nested calls)
    struct Scope {
        LinearAllocator& allocator;
        size_t saved_offset;
        
        Scope(LinearAllocator& a) : allocator(a), saved_offset(a.offset_) {}
        ~Scope() { allocator.offset_ = saved_offset; }
    };

private:
    uint8_t* base_ptr_;
    size_t offset_;
};

// Global thread-local instance for implicit usage
// extern thread_local LinearAllocator<> tls_frame_allocator;

} // namespace memory
} // namespace hft
