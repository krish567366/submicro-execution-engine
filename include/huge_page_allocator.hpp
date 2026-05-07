#pragma once

#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <cstdint>

#if defined(__linux__)
    #ifndef MAP_HUGETLB
        #define MAP_HUGETLB 0x40000
    #endif
#elif defined(__APPLE__)
    #include <mach/vm_statistics.h>
#endif

namespace hft {
namespace memory {

/**
 * Huge Page Allocator (2MB TLB Targeting)
 * 
 * Purpose:
 * Minimizes Translation Lookaside Buffer (TLB) misses. Standard 4KB pages
 * require many levels of page table walks. 2MB HugePages stay in TLB cache.
 */
class HugePageAllocator {
public:
    static void* alloc(size_t size) {
        void* ptr = nullptr;
        
#if defined(__linux__)
        // Try Linux HugeTLB
        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        
        if (ptr == MAP_FAILED) {
            // Fallback to standard allocation if HugePages aren't reserved in kernel
            ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
#elif defined(__APPLE__)
        // macOS Superpages (2MB) - requires alignment and special flag
        size_t aligned_size = (size + (2 * 1024 * 1024) - 1) & ~((2 * 1024 * 1024) - 1);
        ptr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        // Note: Actual superpage promotion on macOS is transparent if size/alignment match
        // but VM_FLAGS_SUPERPAGE_SIZE_2MB can be used with mach_vm_allocate.
#else
        ptr = malloc(size);
#endif
        return ptr;
    }

    static void dealloc(void* ptr, size_t size) {
        if (!ptr) return;
#if defined(__linux__) || defined(__APPLE__)
        munmap(ptr, size);
#else
        free(ptr);
#endif
    }
};

} // namespace memory
} // namespace hft
