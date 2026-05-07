#pragma once

#include "common_types.hpp"
#include <cstring>
#include <cstdint>
#include <type_traits>

namespace hft {
namespace zerocopy {

#pragma pack(push, 1)

// Standard 64-bit aligned header for register loading
struct BinaryMessageHeader {
    uint32_t sequence_number;
    uint16_t message_type;
    uint16_t message_length;
    uint64_t timestamp_ns;
};

// 64-byte Cache-Line Aligned Messages (Padded)
struct alignas(64) BinaryOrderBookUpdate {
    BinaryMessageHeader header;
    uint64_t order_id;
    uint32_t symbol_id;
    uint8_t side;
    uint8_t update_type;
    uint16_t padding;
    double price;
    double quantity;
};

struct alignas(64) BinaryTradeMessage {
    BinaryMessageHeader header;
    uint64_t trade_id;
    uint32_t symbol_id;
    uint8_t aggressor_side;
    uint8_t padding[3];
    double price;
    double quantity;
};

#pragma pack(pop)

/**
 * Template-Specialized Zero-Copy Decoder
 * 
 * Techniques:
 * 1. reinterpret_cast: Costs 0 cycles.
 * 2. __builtin_assume_aligned: Helps compiler vectorize.
 * 3. Static offsets: No dynamic lookups.
 * 4. Register-based loading: Loads fields into CPU registers immediately.
 */
class ZeroCopyDecoder {
public:
    // Force inline to embed decoder directly into the hot loop
    template<typename T>
    [[gnu::always_inline]] 
    static inline const T* decode(const void* buffer) {
        return reinterpret_cast<const T*>(buffer);
    }

    // Fast unsafe field accessors (assumes validity checked by ring buffer)
    
    [[gnu::always_inline]]
    static inline uint64_t get_timestamp(const void* buffer) {
        // Offset 8 bytes (seq + type + len)
        return *reinterpret_cast<const uint64_t*>(static_cast<const char*>(buffer) + 8);
    }

    [[gnu::always_inline]]
    static inline uint16_t get_type(const void* buffer) {
        // Offset 4 bytes (seq)
        return *reinterpret_cast<const uint16_t*>(static_cast<const char*>(buffer) + 4);
    }

    // Specialized high-speed trade parser
    // Direct register extraction - simulates what a hardware decoder does
    struct UnsafeTradeRegs {
        uint32_t sym;
        double px;
        double qty;
    };

    /**
     * Extracts ONLY the fields needed for strategy decision in one go.
     * Uses compile-time offsets.
     */
    [[gnu::always_inline]]
    static inline UnsafeTradeRegs fast_extract_trade(const void* buffer) {
        const char* b = static_cast<const char*>(buffer);
        // Header(16) + TradeID(8) = 24 bytes offset to SymbolID
        // SymbolID(4) + Side(1) + Pad(3) = 8 bytes -> Price at 32
        // Price(8) -> Qty at 40
        
        return UnsafeTradeRegs{
            *reinterpret_cast<const uint32_t*>(b + 24),
            *reinterpret_cast<const double*>(b + 32),
            *reinterpret_cast<const double*>(b + 40)
        };
    }
    
    /**
     * SSE2 Optimization: Prefetch header and first data cacheline
     */
    static inline void prefetch_packet(const void* buffer) {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(buffer, 0, 3);
            __builtin_prefetch(static_cast<const char*>(buffer) + 64, 0, 3);
        #endif
    }
};

} // namespace zerocopy
} // namespace hft