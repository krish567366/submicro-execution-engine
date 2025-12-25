#pragma once

#include "common_types.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <atomic>
#include <thread>

namespace hft {
namespace shm {

// ====
// Shared Memory Ring Buffer for IPC (C++/Rust interop)
// Zero-copy communication between processes/languages
// ====

template<typename T, size_t Capacity>
struct alignas(64) SharedMemoryHeader {
    std::atomic<uint64_t> write_seq;
    std::atomic<uint64_t> read_seq;
    std::atomic<bool> is_initialized;
    uint64_t capacity;
    uint64_t element_size;
    char name[64];
    
    SharedMemoryHeader() 
        : write_seq(0), read_seq(0), is_initialized(false), 
          capacity(Capacity), element_size(sizeof(T)) {
        std::memset(name, 0, sizeof(name));
    }
};

template<typename T, size_t Capacity>
class SharedMemoryRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, 
                  "Capacity must be power of 2");
    
public:
    // 
    // Create or attach to shared memory segment
    // 
    explicit SharedMemoryRingBuffer(const std::string& segment_name, bool create = true)
        : fd_(-1), mapped_region_(nullptr), total_size_(0) {
        
        const std::string shm_path = "/dev/shm/" + segment_name;
        total_size_ = sizeof(SharedMemoryHeader<T, Capacity>) + 
                      sizeof(T) * Capacity + 
                      4096; // Extra guard page
        
        if (create) {
            // Create shared memory segment
            fd_ = shm_open(segment_name.c_str(), 
                          O_CREAT | O_RDWR | O_EXCL, 
                          0666);
            
            if (fd_ == -1 && errno == EEXIST) {
                // Already exists, unlink and recreate
                shm_unlink(segment_name.c_str());
                fd_ = shm_open(segment_name.c_str(), 
                              O_CREAT | O_RDWR | O_EXCL, 
                              0666);
            }
            
            if (fd_ == -1) {
                throw std::runtime_error("Failed to create shared memory");
            }
            
            // Set size
            if (ftruncate(fd_, total_size_) == -1) {
                close(fd_);
                shm_unlink(segment_name.c_str());
                throw std::runtime_error("Failed to set shared memory size");
            }
        } else {
            // Attach to existing segment
            fd_ = shm_open(segment_name.c_str(), O_RDWR, 0666);
            if (fd_ == -1) {
                throw std::runtime_error("Failed to open shared memory");
            }
        }
        
        // Map memory
        mapped_region_ = mmap(nullptr, total_size_,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             fd_, 0);
        
        if (mapped_region_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("Failed to map shared memory");
        }
        
        // Lock pages in RAM (prevent swapping)
        mlock(mapped_region_, total_size_);
        
        // Setup pointers
        header_ = reinterpret_cast<SharedMemoryHeader<T, Capacity>*>(mapped_region_);
        buffer_ = reinterpret_cast<T*>(
            reinterpret_cast<char*>(mapped_region_) + 
            sizeof(SharedMemoryHeader<T, Capacity>));
        
        if (create) {
            new (header_) SharedMemoryHeader<T, Capacity>();
            std::strncpy(header_->name, segment_name.c_str(), 
                        sizeof(header_->name) - 1);
            header_->is_initialized.store(true, std::memory_order_release);
        } else {
            // Wait for initialization
            while (!header_->is_initialized.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
        
        segment_name_ = segment_name;
    }
    
    ~SharedMemoryRingBuffer() {
        if (mapped_region_ && mapped_region_ != MAP_FAILED) {
            munlock(mapped_region_, total_size_);
            munmap(mapped_region_, total_size_);
        }
        if (fd_ != -1) {
            close(fd_);
        }
    }
    
    // 
    // Producer: Write (SPSC semantics)
    // 
    bool write(const T& item) {
        const uint64_t current_write = header_->write_seq.load(std::memory_order_relaxed);
        const uint64_t current_read = header_->read_seq.load(std::memory_order_acquire);
        
        // Check if full
        if (current_write - current_read >= Capacity) {
            return false;
        }
        
        // Write data
        const size_t idx = current_write & (Capacity - 1);
        buffer_[idx] = item;
        
        // Publish
        header_->write_seq.store(current_write + 1, std::memory_order_release);
        return true;
    }
    
    // 
    // Consumer: Read (SPSC semantics)
    // 
    bool read(T& item) {
        const uint64_t current_read = header_->read_seq.load(std::memory_order_relaxed);
        const uint64_t current_write = header_->write_seq.load(std::memory_order_acquire);
        
        // Check if empty
        if (current_read >= current_write) {
            return false;
        }
        
        // Read data
        const size_t idx = current_read & (Capacity - 1);
        item = buffer_[idx];
        
        // Advance read pointer
        header_->read_seq.store(current_read + 1, std::memory_order_release);
        return true;
    }
    
    // 
    // Status
    // 
    size_t size() const {
        const uint64_t w = header_->write_seq.load(std::memory_order_acquire);
        const uint64_t r = header_->read_seq.load(std::memory_order_acquire);
        return static_cast<size_t>(w - r);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
    bool full() const {
        return size() >= Capacity;
    }
    
private:
    int fd_;
    void* mapped_region_;
    size_t total_size_;
    SharedMemoryHeader<T, Capacity>* header_;
    T* buffer_;
    std::string segment_name_;
};

// ====
// Shared Memory Market Data Feed (for multi-process architecture)
// One process handles network I/O, others consume via shared memory
// ====

using SharedMarketDataQueue = SharedMemoryRingBuffer<MarketTick, 32768>;

} // namespace shm
} // namespace hft
