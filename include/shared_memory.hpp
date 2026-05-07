#pragma once

#include "common_types.hpp"
#include <atomic>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

namespace hft {
namespace ipc {

/**
 * Lock-Free Shared Memory Ring Buffer
 * 
 * Purpose: Broadcast market data / logs to external processes (Python/Node) without slow sockets.
 * Structure:
 * [ Header (Head/Tail atomic) ]
 * [ Data Buffer (Power of 2) ]
 * 
 * Writers: Single (The HFT Engine)
 * Readers: Multiple (Monitoring, Logs, Risk UI)
 */
template<typename T, size_t Size = 1024 * 1024>
class SharedMemoryRing {
    struct alignas(64) Header {
        std::atomic<uint64_t> write_pos; // Monotonic
        std::atomic<uint64_t> min_read_pos; // Flow control
        uint32_t magic;
        uint32_t version;
    };

public:
    SharedMemoryRing() : mem_ptr_(nullptr), fd_(-1) {}

    bool create(const std::string& name) {
        fd_ = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd_ < 0) return false;

        size_t total_size = sizeof(Header) + Size * sizeof(T);
        ftruncate(fd_, total_size);

        mem_ptr_ = (uint8_t*)mmap(0, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mem_ptr_ == MAP_FAILED) return false;

        // Initialize header
        Header* h = reinterpret_cast<Header*>(mem_ptr_);
        h->write_pos.store(0);
        h->magic = 0xDEADBEEF;
        
        buffer_ = reinterpret_cast<T*>(mem_ptr_ + sizeof(Header));
        return true;
    }

    // Zero-Copy Push
    // Returns pointer to slot where data was written (for logging)
    // T must be trivially copyable
    inline void push(const T& item) {
        Header* h = reinterpret_cast<Header*>(mem_ptr_);
        
        uint64_t pos = h->write_pos.load(std::memory_order_relaxed);
        uint64_t idx = pos & (Size - 1);
        
        buffer_[idx] = item; // Copy
        
        // Commit write
        h->write_pos.store(pos + 1, std::memory_order_release);
    }

    ~SharedMemoryRing() {
        // cleanup? usually shared memory persists unless explicitly unlinked
    }

private:
    int fd_;
    uint8_t* mem_ptr_; // Base pointer
    T* buffer_;        // Typed buffer start
};

}
}