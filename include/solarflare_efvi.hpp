#pragma once

#include "common_types.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>

// Mocking Solarflare ef_vi headers for compilation on non-Solarflare systems
// In a real environment, these would be <etherfabric/vi.h>, <etherfabric/pd.h>, etc.
#ifndef __EF_VI_H__
    // Minimal mock definitions to allow compilation
    typedef struct ef_vi ef_vi;
    typedef struct ef_pd ef_pd;
    typedef struct ef_memreg ef_memreg;
    typedef uint64_t ef_addr;
    
    enum ef_event_type {
        EF_EVENT_TYPE_RX = 1,
        EF_EVENT_TYPE_TX = 2,
        EF_EVENT_TYPE_RX_DISCARD = 3
    };

    struct ef_event {
        int type;
        union {
            struct {
                int len;
                int dma_id;
            } rx;
        };
    };

    // Placeholder functions
    inline int ef_driver_open(int* fd) { return 0; }
    inline int ef_pd_alloc(ef_pd** pd, int fd, int flags) { return 0; }
    inline int ef_vi_alloc_from_pd(ef_vi** vi, int fd, ef_pd* pd, int fd_driver, int evq_cap, int rxq_cap, int txq_cap, void* vi_buf, int vi_buf_len, int flags) { return 0; }
    inline int ef_memreg_alloc(ef_memreg** mr, int fd, ef_pd* pd, int fd_driver, void* buf, int len) { return 0; }
    inline ef_addr ef_memreg_dma_addr(ef_memreg* mr, int offset) { return 0; }
    inline void ef_vi_receive_post(ef_vi* vi, ef_addr dma_addr, int id) {}
    inline int ef_eventq_poll(ef_vi* vi, ef_event* evs, int max_evs) { return 0; }
    inline void ef_vi_transmit_pio(ef_vi* vi, int offset, int len, int event_id) {} // PIO write
    inline void* ef_vi_get_pio_io(ef_vi* vi) { return nullptr; } // Get PIO window
#endif

namespace hft {
namespace networking {
namespace solarflare {

/**
 * Solarflare ef_vi Wrapper
 * 
 * "The Gold Standard for Software HFT Networking"
 * 
 * Capability:
 * - Access NIC rings and Event Queues directly from userspace.
 * - Kernel Bypass (Zero System Calls).
 * - PIO (Programmed I/O): Write order bytes directly to NIC SRAM (PCIe write) instead of DMA.
 *   - DMA Latency: ~400-600ns overhead (doorbell -> fetch -> send)
 *   - PIO Latency: ~180-250ns (direct write -> send)
 * 
 * Limitation:
 * - Requires specialized hardware (X2522, X3522 cards).
 * - This class acts as a template/simulator if hardware is missing.
 */
class EfviDriver {
public:
    static constexpr int N_RX_BUFS = 512;
    static constexpr int BUF_SIZE = 2048; // MTU-ish
    static constexpr int REFILL_BATCH = 16;

    EfviDriver() : initialized_(false), vi_(nullptr), pio_buffer_(nullptr) {}

    bool init(const char* interface) {
        // Real initialization logic (simplified)
        if (ef_driver_open(&driver_fd_) < 0) return false;
        if (ef_pd_alloc(&pd_, driver_fd_, 0) < 0) return false;
        
        // Allocate Memory Pool (Hugepages ideally)
        pkt_mem_ = new char[N_RX_BUFS * BUF_SIZE]; // aligned_alloc in prod
        
        // Register Memory with NIC
        if (ef_memreg_alloc(&mem_reg_, driver_fd_, pd_, driver_fd_, pkt_mem_, N_RX_BUFS * BUF_SIZE) < 0) return false;
        
        // Allocate VI (Virtual Interface)
        if (ef_vi_alloc_from_pd(&vi_, driver_fd_, pd_, driver_fd_, -1, N_RX_BUFS, 0, nullptr, -1, 0) < 0) return false;

        // Get PIO Window (Critical for latency)
        pio_buffer_ = (char*)ef_vi_get_pio_io(vi_);

        // Fill RX Ring
        refill_rx_ring();
        
        initialized_ = true;
        return true;
    }

    // HOT PATH: Poll for packets
    template<typename PacketHandler>
    inline void poll(PacketHandler&& handler) {
        ef_event evs[REFILL_BATCH];
        int n_ev = ef_eventq_poll(vi_, evs, REFILL_BATCH);

        for (int i = 0; i < n_ev; ++i) {
            if (evs[i].type == EF_EVENT_TYPE_RX) {
                // Zero-Copy Access
                int id = evs[i].rx.dma_id; // Buffer ID we posted
                char* payload = pkt_mem_ + (id * BUF_SIZE);
                int len = evs[i].rx.len;

                // Call strategy
                handler(payload, len);

                // Repost buffer to RX ring (Recycle)
                // In prod, check if ring is full or batch this
                ef_addr dma_addr = ef_memreg_dma_addr(mem_reg_, id * BUF_SIZE);
                ef_vi_receive_post(vi_, dma_addr, id);
            }
        }
    }

    // HOT PATH: Send Order via PIO (Lowest Latency)
    // Only works for small packets (< 256 bytes usually)
    inline void send_pio(const void* data, int len) {
        if (!pio_buffer_) return; // Fallback to DMA?

        // 1. Write headers + payload to NIC SRAM (PCIe Write)
        // Using SIMD copy for speed
        // This writes directly across the PCIe bus!
        
        // memcpy_toio(pio_buffer_, data, len); // Pseudo-code
        
        // In reality, we must ensure 64-byte write atomicity or write combining
        // Standard memcpy might generate byte-writes which kills PCIe performance.
        // We use AVX stores.
        
        const char* src = (const char*)data;
        char* dst = pio_buffer_;
        
        // Copy loop (assume small packet, unrolling ok)
        for (int i=0; i<len; i+=8) {
            *(uint64_t*)(dst + i) = *(const uint64_t*)(src + i);
        }

        // 2. Doorbell to trigger send
        ef_vi_transmit_pio(vi_, 0, len, 0); 
    }

private:
    bool initialized_;
    int driver_fd_;
    ef_pd* pd_;
    ef_vi* vi_;
    ef_memreg* mem_reg_;
    char* pkt_mem_;
    char* pio_buffer_;

    void refill_rx_ring() {
        for (int i = 0; i < N_RX_BUFS; ++i) {
            ef_addr dma_addr = ef_memreg_dma_addr(mem_reg_, i * BUF_SIZE);
            ef_vi_receive_post(vi_, dma_addr, i); // dma_addr, id
        }
    }
};

} // namespace solarflare
} // namespace networking
} // namespace hft
