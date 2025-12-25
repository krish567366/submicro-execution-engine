#pragma once

#include "common_types.hpp"
#include "lockfree_queue.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <cstring>

namespace hft {

class KernelBypassNIC {
public:
    // 
    // Constructor
    // 
    explicit KernelBypassNIC(size_t queue_capacity = 16384)  // Must be power of 2
        : market_data_queue_(),
          is_running_(false),
          total_packets_received_(0),
          total_bytes_received_(0) {
        
        static_assert((16384 & (16384 - 1)) == 0, 
                     "Queue capacity must be power of 2");
    }
    
    // 
    // Destructor
    // 
    ~KernelBypassNIC() {
        stop();
    }
    
    // Disable copy/move
    KernelBypassNIC(const KernelBypassNIC&) = delete;
    KernelBypassNIC& operator=(const KernelBypassNIC&) = delete;
    
    // 
    // Start the NIC (begins receiving data)
    // In production: would initialize DPDK, map NIC memory, setup RX rings
    // 
    void start() {
        if (is_running_.exchange(true, std::memory_order_acquire)) {
            return;  // Already running
        }
        
        // In production: 
        // - Initialize DPDK EAL
        // - Bind to NIC port
        // - Configure RX/TX queues
        // - Setup memory pools (hugepages)
        // - Register packet processing callbacks
        
        // For simulation: just mark as running
        // Real data would come from exchange multicast feeds
    }
    
    // 
    // Stop the NIC
    // 
    void stop() {
        is_running_.store(false, std::memory_order_release);
    }
    
    // 
    // Get next market tick (zero-copy, non-blocking)
    // Returns: true if tick was available, false if queue empty
    // 
    // This is the critical path function - must be extremely fast
    // 
    bool get_next_tick(MarketTick& tick) {
        // Attempt to pop from lock-free queue
        // This is a zero-copy operation - no kernel involvement
        return market_data_queue_.pop(tick);
    }
    
    // 
    // Peek at next tick without removing (for pre-processing)
    // 
    const MarketTick* peek_next_tick() const {
        return market_data_queue_.peek();
    }
    
    // 
    // Check if data is available
    // 
    bool has_data() const {
        return !market_data_queue_.empty();
    }
    
    // 
    // Simulate receiving a packet from exchange (producer side)
    // In production: called from NIC interrupt handler or poll-mode driver
    // 
    bool inject_market_data(const MarketTick& tick) {
        if (!is_running_.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Zero-copy push to ring buffer
        const bool success = market_data_queue_.push(tick);
        
        if (success) {
            total_packets_received_.fetch_add(1, std::memory_order_relaxed);
            total_bytes_received_.fetch_add(sizeof(MarketTick), 
                                           std::memory_order_relaxed);
        }
        
        return success;
    }
    
    // 
    // Batch injection for market data bursts (higher throughput)
    // 
    size_t inject_batch(const MarketTick* ticks, size_t count) {
        if (!is_running_.load(std::memory_order_acquire)) {
            return 0;
        }
        
        size_t injected = 0;
        for (size_t i = 0; i < count; ++i) {
            if (market_data_queue_.push(ticks[i])) {
                ++injected;
            } else {
                break;  // Queue full
            }
        }
        
        if (injected > 0) {
            total_packets_received_.fetch_add(injected, std::memory_order_relaxed);
            total_bytes_received_.fetch_add(injected * sizeof(MarketTick),
                                           std::memory_order_relaxed);
        }
        
        return injected;
    }
    
    // 
    // Simulate raw packet reception (closest to DPDK model)
    // In production: would parse exchange-specific binary protocol
    // 
    template<typename ExchangeProtocol>
    bool receive_raw_packet(const uint8_t* packet_data, size_t packet_size) {
        // In production: 
        // 1. DMA transfers packet directly to pre-allocated hugepage memory
        // 2. No kernel memcpy - NIC writes directly to userspace buffer
        // 3. Parse binary protocol (e.g., ITCH, FAST, SBE)
        // 4. Construct MarketTick in-place
        
        MarketTick tick;
        
        // Simulate protocol parsing (zero-copy in production)
        if (packet_size >= sizeof(MarketTick)) {
            // Direct memory interpretation (zero-copy)
            std::memcpy(&tick, packet_data, sizeof(MarketTick));
            tick.timestamp = now();  // Kernel-bypass timestamp at NIC
            
            return market_data_queue_.emplace(std::move(tick));
        }
        
        return false;
    }
    
    // 
    // Get NIC statistics
    // 
    struct NICStats {
        uint64_t packets_received;
        uint64_t bytes_received;
        size_t queue_size;
        size_t queue_capacity;
        double utilization;  // Queue fullness percentage
    };
    
    NICStats get_stats() const {
        NICStats stats;
        stats.packets_received = total_packets_received_.load(std::memory_order_relaxed);
        stats.bytes_received = total_bytes_received_.load(std::memory_order_relaxed);
        stats.queue_size = market_data_queue_.size();
        stats.queue_capacity = market_data_queue_.capacity();
        stats.utilization = (stats.queue_capacity > 0) ?
            (100.0 * stats.queue_size / stats.queue_capacity) : 0.0;
        return stats;
    }
    
    // 
    // Reset statistics
    // 
    void reset_stats() {
        total_packets_received_.store(0, std::memory_order_release);
        total_bytes_received_.store(0, std::memory_order_release);
    }
    
    // 
    // Check if NIC is running
    // 
    bool is_running() const {
        return is_running_.load(std::memory_order_acquire);
    }
    
    // 
    // Configure receive buffer affinity (CPU pinning for NUMA optimization)
    // In production: pins processing thread to same NUMA node as NIC
    // 
    void set_cpu_affinity(int cpu_core) {
#ifdef __linux__
        // In production: would use pthread_setaffinity_np
        // to pin to specific CPU core for minimal latency
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_core, &cpuset);
        
        // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    }
    
private:
    // 
    // Lock-free ring buffer (zero-copy queue)
    // Sized as power-of-2 for fast modulo operations
    // 
    LockFreeQueue<MarketTick, 16384> market_data_queue_;
    
    // 
    // State variables
    // 
    std::atomic<bool> is_running_;
    std::atomic<uint64_t> total_packets_received_;
    std::atomic<uint64_t> total_bytes_received_;
};

// Market Data Feed Simulator
// Simulates exchange multicast feed for testing

class MarketDataSimulator {
public:
    explicit MarketDataSimulator(KernelBypassNIC& nic)
        : nic_(nic),
          is_running_(false),
          sim_thread_() {}
    
    ~MarketDataSimulator() {
        stop();
    }
    
    // Start simulation thread
    void start(double update_frequency_hz = 10000.0) {
        if (is_running_.exchange(true)) {
            return;
        }
        
        sim_thread_ = std::thread([this, update_frequency_hz]() {
            const auto sleep_duration = std::chrono::nanoseconds(
                static_cast<int64_t>(1e9 / update_frequency_hz));
            
            uint64_t seq = 0;
            while (is_running_.load(std::memory_order_acquire)) {
                // Generate simulated market tick
                MarketTick tick = generate_synthetic_tick(seq++);
                
                // Inject into NIC queue
                nic_.inject_market_data(tick);
                
                // Sleep to simulate exchange update rate
                std::this_thread::sleep_for(sleep_duration);
            }
        });
    }
    
    void stop() {
        is_running_.store(false, std::memory_order_release);
        if (sim_thread_.joinable()) {
            sim_thread_.join();
        }
    }
    
private:
    MarketTick generate_synthetic_tick(uint64_t seq) {
        MarketTick tick;
        tick.timestamp = now();
        tick.asset_id = 0;
        
        // Simulate random walk
        static double price = 100.0;
        price += ((std::rand() % 100) - 50) * 0.001;
        
        tick.mid_price = price;
        tick.bid_price = price - 0.01;
        tick.ask_price = price + 0.01;
        tick.bid_size = 100 + (std::rand() % 900);
        tick.ask_size = 100 + (std::rand() % 900);
        tick.trade_volume = std::rand() % 100;
        tick.trade_side = (std::rand() % 2) ? Side::BUY : Side::SELL;
        tick.depth_levels = 10;
        
        // Fill LOB levels
        for (size_t i = 0; i < 10; ++i) {
            tick.bid_prices[i] = price - 0.01 * (i + 1);
            tick.ask_prices[i] = price + 0.01 * (i + 1);
            tick.bid_sizes[i] = 100 + (std::rand() % 900);
            tick.ask_sizes[i] = 100 + (std::rand() % 900);
        }
        
        return tick;
    }
    
    KernelBypassNIC& nic_;
    std::atomic<bool> is_running_;
    std::thread sim_thread_;
};

} // namespace hft
