#pragma once

#include "common_types.hpp"
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <unordered_map>

namespace hft {
namespace zerocopy {

// ============================================================================
// Zero-Copy Protocol Decoders
// Direct memory-mapped parsing from NIC ring buffer (no intermediate copies)
// Target: Reduce packet parsing from 100ns to 50ns
// ============================================================================

// Packed binary protocol structures (aligned with exchange formats)
// These structs overlay directly on network packet bytes

#pragma pack(push, 1)  // Force byte-alignment (no padding)

// FIX-like binary protocol header
struct BinaryMessageHeader {
    uint32_t sequence_number;
    uint16_t message_type;
    uint16_t message_length;
    uint64_t timestamp_ns;  // Exchange timestamp
};

// Market data update (ADD/MODIFY/DELETE/EXECUTE)
struct BinaryOrderBookUpdate {
    BinaryMessageHeader header;
    uint64_t order_id;
    uint32_t symbol_id;      // Pre-mapped symbol integer
    uint8_t side;            // 0=bid, 1=ask
    uint8_t update_type;     // 0=ADD, 1=MODIFY, 2=DELETE, 3=EXECUTE
    uint16_t padding;
    double price;            // IEEE 754 double
    double quantity;
};

// Trade execution message
struct BinaryTradeMessage {
    BinaryMessageHeader header;
    uint64_t trade_id;
    uint32_t symbol_id;
    uint8_t aggressor_side;  // 0=buy, 1=sell
    uint8_t padding[3];
    double price;
    double quantity;
};

// Top-of-book quote (BBO)
struct BinaryQuoteMessage {
    BinaryMessageHeader header;
    uint32_t symbol_id;
    uint32_t padding;
    double bid_price;
    double bid_quantity;
    double ask_price;
    double ask_quantity;
};

#pragma pack(pop)  // Restore default packing

// ============================================================================
// Zero-Copy Decoder: Parse directly from ring buffer memory
// ============================================================================
class ZeroCopyDecoder {
public:
    ZeroCopyDecoder() = default;
    
    // Parse order book update (zero-copy, ~50ns)
    // Returns pointer cast - NO MEMORY COPY
    static inline const BinaryOrderBookUpdate* parse_order_book_update(const void* buffer) {
        return reinterpret_cast<const BinaryOrderBookUpdate*>(buffer);
    }
    
    // Parse trade message (zero-copy, ~50ns)
    static inline const BinaryTradeMessage* parse_trade(const void* buffer) {
        return reinterpret_cast<const BinaryTradeMessage*>(buffer);
    }
    
    // Parse quote message (zero-copy, ~50ns)
    static inline const BinaryQuoteMessage* parse_quote(const void* buffer) {
        return reinterpret_cast<const BinaryQuoteMessage*>(buffer);
    }
    
    // Validate message header (basic sanity checks, ~20ns)
    static inline bool validate_header(const void* buffer, size_t buffer_size) {
        if (buffer_size < sizeof(BinaryMessageHeader)) {
            return false;
        }
        
        const auto* header = reinterpret_cast<const BinaryMessageHeader*>(buffer);
        
        // Check message length fits in buffer
        if (header->message_length > buffer_size) {
            return false;
        }
        
        // Check message type is valid (0-255 range)
        if (header->message_type > 255) {
            return false;
        }
        
        return true;
    }
    
    // Extract basic fields without full conversion
    // Use this when working directly with binary format
    static inline uint64_t get_order_id(const BinaryOrderBookUpdate* binary) {
        return binary->order_id;
    }
    
    static inline double get_price(const BinaryOrderBookUpdate* binary) {
        return binary->price;
    }
    
    static inline double get_quantity(const BinaryOrderBookUpdate* binary) {
        return binary->quantity;
    }
    
    static inline bool is_bid_side(const BinaryOrderBookUpdate* binary) {
        return (binary->side == 0);
    }
    
    static inline uint64_t get_sequence_number(const BinaryOrderBookUpdate* binary) {
        return binary->header.sequence_number;
    }
};

// ============================================================================
// Symbol ID Mapping (for zero-copy symbol lookup)
// Pre-computed hash map: symbol_string -> symbol_id
// ============================================================================
class SymbolMapper {
public:
    SymbolMapper() {
        // Pre-populate common symbols
        add_symbol("BTCUSD", 1);
        add_symbol("ETHUSD", 2);
        add_symbol("SOLUSD", 3);
        add_symbol("BNBUSD", 4);
        add_symbol("XRPUSD", 5);
    }
    
    void add_symbol(const std::string& symbol, uint32_t id) {
        symbol_to_id_[symbol] = id;
        id_to_symbol_[id] = symbol;
    }
    
    uint32_t get_id(const std::string& symbol) const {
        auto it = symbol_to_id_.find(symbol);
        return (it != symbol_to_id_.end()) ? it->second : 0;
    }
    
    std::string get_symbol(uint32_t id) const {
        auto it = id_to_symbol_.find(id);
        return (it != id_to_symbol_.end()) ? it->second : "";
    }
    
private:
    std::unordered_map<std::string, uint32_t> symbol_to_id_;
    std::unordered_map<uint32_t, std::string> id_to_symbol_;
};

} // namespace zerocopy
} // namespace hft
