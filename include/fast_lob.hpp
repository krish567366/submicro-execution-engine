#pragma once

#include "common_types.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cmath>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

// Branch prediction & Prefetch
#if defined(__GNUC__) || defined(__clang__)
    #define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define PREFETCH_READ(addr)
    #define PREFETCH_WRITE(addr)
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

namespace hft {

// ==========================================
// 1. High-Performance Fixed-Size Flat Map
// ==========================================
// Linear probing open addressing. No allocations.
template<typename Key, typename Value, size_t Capacity, Key EmptyKey = 0>
class FlatMap {
public:
    struct Entry {
        Key key;
        Value value;
    };

    FlatMap() { clear(); }

    void clear() {
        for (auto& e : entries_) e.key = EmptyKey;
        size_ = 0;
    }

    // Insert or update. Returns pointer to value.
    Value* insert_or_assign(Key key, const Value& value) {
        size_t idx = hash(key);
        size_t start_idx = idx;

        while (true) {
            if (entries_[idx].key == EmptyKey) {
                // New entry
                entries_[idx].key = key;
                entries_[idx].value = value;
                size_++;
                return &entries_[idx].value;
            }
            if (entries_[idx].key == key) {
                // Update
                entries_[idx].value = value;
                return &entries_[idx].value;
            }
            idx = (idx + 1) % Capacity;
            if (UNLIKELY(idx == start_idx)) return nullptr; // Full
        }
    }

    Value* find(Key key) {
        size_t idx = hash(key);
        size_t start_idx = idx;

        while (entries_[idx].key != EmptyKey) {
            if (entries_[idx].key == key) return &entries_[idx].value;
            idx = (idx + 1) % Capacity;
            if (UNLIKELY(idx == start_idx)) return nullptr;
        }
        return nullptr;
    }
    
    // Lazy deletion with tombstone would be better for heavy churn, 
    // but for order book pure deletion is rare compared to modification.
    // We strictly use EmptyKey for empty. If we shift on delete it's O(cluster).
    // For simplicity & speed in HFT, we often just mark as inactive or use a tombstome 
    // if key collision is high. Here we use simple linear probe with backshift removal 
    // to keep chains contiguous.
    bool erase(Key key) {
        size_t idx = hash(key);
        size_t start_idx = idx;

        while (entries_[idx].key != EmptyKey) {
            if (entries_[idx].key == key) {
                // Found, remove it
                entries_[idx].key = EmptyKey;
                size_--;
                
                // Rehash chain
                size_t hole = idx;
                size_t next = (hole + 1) % Capacity;
                
                while (entries_[next].key != EmptyKey) {
                    size_t desired = hash(entries_[next].key);
                    // Determine if 'next' belongs in the gap between 'desired' and 'hole'
                    // Cyclical logic: (hole < desired <= next) or (next < hole < desired) or (desired <= next < hole)
                    // If true, it stays. If false, it moves to hole.
                    bool belongs_in_gap = (hole < desired && desired <= next) ||
                                          (next < hole && hole < desired) ||
                                          (desired <= next && next < hole); // Simplified check logic often tricky
                    
                    // Actually simpler: if hash(next) is NOT "cyclically between" (hole+1) and next, we can move it?
                    // Standard Python dict removal / Robin Hood logic:
                    // Just swapping empty key? No, simpler to just swap with last event or use tombstone?
                    // Speedhack: Just mark key as EmptyKey? No, breaks probe chain.
                    // Correct backshift:
                    if (!((hole <= next) ? (hole < desired && desired <= next) : (hole < desired || desired <= next))) {
                         entries_[hole] = entries_[next];
                         entries_[next].key = EmptyKey;
                         hole = next;
                    }
                    next = (next + 1) % Capacity;
                }
                return true;
            }
            idx = (idx + 1) % Capacity;
            if (UNLIKELY(idx == start_idx)) return false;
        }
        return false;
    }

private:
    std::array<Entry, Capacity> entries_;
    size_t size_;

    // Fast integer mixer
    inline size_t hash(Key k) const {
        // Multiplicative fibonacci hashing
        return (k * 11400714819323198485llu) & (Capacity - 1);
    }
};

struct FastPriceLevel {
    double price;
    double quantity;
    uint32_t order_count;
    bool is_active;
};

// ==========================================
// 2. Optimized Order Book (SoA + AVX2)
// ==========================================
template<size_t MaxLevels>
class ArrayBasedOrderBook {
public:
    // Level view struct for accessing individual levels
    struct Level {
        double price;
        double quantity;
        uint32_t count;
    };
    
    // SoA Layout: Hardware prefetcher friendly, SIMD compatible
    struct Side {
        alignas(64) std::array<double, MaxLevels> prices;
        alignas(64) std::array<double, MaxLevels> quantities;
        alignas(64) std::array<uint32_t, MaxLevels> counts;
        uint32_t count = 0;
        
        Side() {
            // Init prices to guard values? 
            // For Bids: -Inf (or 0). For Asks: +Inf.
            // But we track 'count', so uninitialized is fine if careful.
            // Zeroing is safer.
            prices.fill(0.0);
            quantities.fill(0.0);
            counts.fill(0);
        }
    };

    Side bids;
    Side asks;

    // SIMD Helper: Find index of first element where (Value OP Array[i])
    // Returns index [0, Limit) or Limit if not found
    template<bool IsBid>
    inline int find_insertion_index(double price, const std::array<double, MaxLevels>& prices, int count) {
        int i = 0;
#if defined(__AVX2__)
        __m256d v_price = _mm256_set1_pd(price);
        
        // Process 4 doubles at a time
        // Loop unrolling for MaxLevels=20 (5 iters)
        // Manual unroll or let compiler?
        // With constant MaxLevels, compiler unrolls well.
        for (; i <= count - 4; i += 4) {
            __m256d v_levels = _mm256_load_pd(&prices[i]); // Aligned? std::array alignas(64) yes.
            
            // Mask: (NewPrice > LevelPrice) for Bids (Descending)
            // Mask: (NewPrice < LevelPrice) for Asks (Ascending)
            __m256d cmp;
            if constexpr (IsBid) { // Descending arrays
                cmp = _mm256_cmp_pd(v_price, v_levels, _CMP_GT_OQ); 
            } else { // Ascending arrays
                cmp = _mm256_cmp_pd(v_price, v_levels, _CMP_LT_OQ);
            }
            
            int mask = _mm256_movemask_pd(cmp);
            if (mask != 0) {
                return i + __builtin_ctz(mask); // First set bit
            }
        }
#endif
        // Scalar tail
        for (; i < count; ++i) {
            if constexpr (IsBid) {
                if (price > prices[i]) return i;
            } else {
                if (price < prices[i]) return i;
            }
        }
        return count;
    }

    void update_bid(double price, double size, uint32_t count) {
        // 1. Check for Update/Delete (Exact Match)
        // AVX scan for equality?
        // Let's stick to scalar linear for exact match since it's likely high up.
        // Actually for N=20 scalar is fast.
        
        for(size_t i=0; i<bids.count; ++i) {
            // Fast abs diff
            if (std::abs(bids.prices[i] - price) < 1e-9) {
                if (size <= 0) {
                    // Remove: Shift all arrays left
                    size_t rem = bids.count - 1 - i;
                    if (rem > 0) {
                        std::memmove(&bids.prices[i], &bids.prices[i+1], rem * sizeof(double));
                        std::memmove(&bids.quantities[i], &bids.quantities[i+1], rem * sizeof(double));
                        std::memmove(&bids.counts[i], &bids.counts[i+1], rem * sizeof(uint32_t));
                    }
                    bids.count--;
                } else {
                    bids.quantities[i] = size;
                    bids.counts[i] = count;
                }
                return;
            }
        }
        
        // 2. Insert New
        if (size > 0 && bids.count < MaxLevels) {
            int idx = find_insertion_index<true>(price, bids.prices, bids.count);
            
            // Shift Right
            size_t rem = bids.count - idx;
            if (rem > 0) {
                std::memmove(&bids.prices[idx+1], &bids.prices[idx], rem * sizeof(double));
                std::memmove(&bids.quantities[idx+1], &bids.quantities[idx], rem * sizeof(double));
                std::memmove(&bids.counts[idx+1], &bids.counts[idx], rem * sizeof(uint32_t));
            }
            
            bids.prices[idx] = price;
            bids.quantities[idx] = size;
            bids.counts[idx] = count;
            bids.count++;
        }
    }

    void update_ask(double price, double size, uint32_t count) {
        for(size_t i=0; i<asks.count; ++i) {
            if (std::abs(asks.prices[i] - price) < 1e-9) {
                if (size <= 0) {
                    size_t rem = asks.count - 1 - i;
                    if (rem > 0) {
                        std::memmove(&asks.prices[i], &asks.prices[i+1], rem * sizeof(double));
                        std::memmove(&asks.quantities[i], &asks.quantities[i+1], rem * sizeof(double));
                        std::memmove(&asks.counts[i], &asks.counts[i+1], rem * sizeof(uint32_t));
                    }
                    asks.count--;
                } else {
                    asks.quantities[i] = size;
                    asks.counts[i] = count;
                }
                return;
            }
        }
        
        if (size > 0 && asks.count < MaxLevels) {
            int idx = find_insertion_index<false>(price, asks.prices, asks.count);
            
            size_t rem = asks.count - idx;
            if (rem > 0) {
                std::memmove(&asks.prices[idx+1], &asks.prices[idx], rem * sizeof(double));
                std::memmove(&asks.quantities[idx+1], &asks.quantities[idx], rem * sizeof(double));
                std::memmove(&asks.counts[idx+1], &asks.counts[idx], rem * sizeof(uint32_t));
            }
            
            asks.prices[idx] = price;
            asks.quantities[idx] = size;
            asks.counts[idx] = count;
            asks.count++;
        }
    }
};

// struct Level for external access via conversion or just exposing primitives
// The FastLOBReconstructor accessors need to change their return type or construct a view


// ==========================================
// 3. Zero-Allocation Fast Reconstructor
// ==========================================
struct TrackedOrder {
    uint64_t id;
    double px;
    double qty;
    bool is_bid;
};

// Power of 2 capacity for fast modulo
static constexpr size_t ORDER_MAP_CAPACITY = 32768; // 2^15
static constexpr size_t PRICE_MAP_CAPACITY = 1024; // 2^10

class FastLOBReconstructor {
public:
    FastLOBReconstructor(const std::string& symbol)
        : last_seq_(0) {
        // Symbol ignored in fast path struct
    }

    // Hot Path: 100% Allocation Free
    inline bool process_update(uint64_t seq, uint8_t type, uint64_t oid, double px, double qty, bool bid) {
        // PREFETCH_WRITE(&orders_); // Prefetch map buckets? Har to guess
        
        if (UNLIKELY(seq != last_seq_ + 1 && last_seq_ != 0)) return false;
        last_seq_ = seq;

        switch(type) {
            case 0: return on_add(oid, px, qty, bid);
            case 1: return on_mod(oid, px, qty, bid);
            case 2: return on_del(oid);
            case 3: return on_exec(oid, qty);
        }
        return false;
    }

    // Zero-copy accessors
    inline ArrayBasedOrderBook<20>::Level get_best_bid() const {
        if (book_.bids.count == 0) return {0.0, 0.0, 0};
        return {book_.bids.prices[0], book_.bids.quantities[0], book_.bids.counts[0]};
    }
    
    inline ArrayBasedOrderBook<20>::Level get_best_ask() const {
        if (book_.asks.count == 0) return {0.0, 0.0, 0};
        return {book_.asks.prices[0], book_.asks.quantities[0], book_.asks.counts[0]};
    }

private:
    uint64_t last_seq_;
    
    // Core Structures
    ArrayBasedOrderBook<20> book_; // Track top 20 levels
    
    // Hash Maps (Flat, Open Addressing)
    FlatMap<uint64_t, TrackedOrder, ORDER_MAP_CAPACITY> orders_;
    
    // Map Price -> {TotalQty, Count}
    // We Map price (double) to a struct. To hash double, cast to int64.
    // Or scale by 10000 and cast. Here we use raw bit_cast logic for speed?
    // Using simple int64 cast of price*1e4 for key?
    // Let's use internal LevelMaps
    struct LevelState { double qty; uint32_t cnt; };
    
    // Note: Double as key is risky. In prod we use int64_t price_ticks.
    // Here we cast double bits to uint64 for hashing.
    struct PxHash {
        static uint64_t k(double d) { uint64_t r; std::memcpy(&r, &d, 8); return r; }
    };
    
    // We actually need a specialized map for double keys.
    // For simplicity, let's assume we implement the helper logic inline or 
    // use the FlatMap with uint64_t keys (bit cast).
    // Let's stick to the logic:
    
    // Implementation of Add
    bool on_add(uint64_t oid, double px, double qty, bool bid) {
        TrackedOrder o = {oid, px, qty, bid};
        orders_.insert_or_assign(oid, o);
        update_level(px, qty, 1, bid);
        return true;
    }
    
    bool on_del(uint64_t oid) {
        auto* o = orders_.find(oid);
        if(!o) return false;
        update_level(o->px, -o->qty, -1, o->is_bid);
        orders_.erase(oid);
        return true;
    }
    
    bool on_mod(uint64_t oid, double px, double qty, bool bid) {
        auto* o = orders_.find(oid);
        if(!o) return on_add(oid, px, qty, bid);

        // Remove old impact
        update_level(o->px, -o->qty, -1, o->is_bid);
        
        // Update order
        o->px = px;
        o->qty = qty;
        
        // Add new impact
        update_level(px, qty, 1, bid);
        return true;
    }

    bool on_exec(uint64_t oid, double exec_qty) {
        auto* o = orders_.find(oid);
        if(!o) return false;
        
        update_level(o->px, -exec_qty, 0, o->is_bid); // Count doesn't change on partial fill?
        // Actually usually execution reduces qty. If full exec, delete comes later or implied?
        // ITCH spec: Exec message reduces qty. If falls to 0, explicit delete usually sent separately?
        // Or implied? Let's assume explicit delete comes if filled.
        // If system implies fill=delete w/o separate msg, we need logic.
        // Assuming partial fill here:
        o->qty -= exec_qty;
        if(o->qty < 1e-9) {
             update_level(o->px, 0, -1, o->is_bid); // removal of order count
             orders_.erase(oid);
        }
        return true;
    }

    void update_level(double px, double qty_delta, int32_t cnt_delta, bool bid) {
        // We maintain the bookkeeping in a flat map too?
        // Or just rely on the Book?
        // Standard LOB: We MUST track total size at level to know when level vanishes.
        // ArrayBasedOrderBook update needs absolute values.
        // So we need a Price->State map.
        
        uint64_t key; std::memcpy(&key, &px, 8);
        
        if (bid) {
            LevelState* s = bid_levels_.find(key);
            if (!s) {
                // New level
                if (qty_delta > 0) {
                    LevelState ns = {qty_delta, (uint32_t)cnt_delta};
                    bid_levels_.insert_or_assign(key, ns);
                    book_.update_bid(px, ns.qty, ns.cnt);
                }
            } else {
                s->qty += qty_delta;
                s->cnt += cnt_delta; // wraps if negative? cast to signed to add then back?
                // cnt_delta is int, s->cnt is uint.
                
                if (s->qty < 1e-9 || s->cnt == 0) {
                    bid_levels_.erase(key);
                    book_.update_bid(px, 0, 0);
                } else {
                    book_.update_bid(px, s->qty, s->cnt);
                }
            }
        } else {
            // Same for ask
           LevelState* s = ask_levels_.find(key);
            if (!s) {
                if (qty_delta > 0) {
                    LevelState ns = {qty_delta, (uint32_t)cnt_delta};
                    ask_levels_.insert_or_assign(key, ns);
                    book_.update_ask(px, ns.qty, ns.cnt);
                }
            } else {
                s->qty += qty_delta;
                s->cnt += cnt_delta;
                if (s->qty < 1e-9 || s->cnt == 0) {
                    ask_levels_.erase(key);
                    book_.update_ask(px, 0, 0);
                } else {
                    book_.update_ask(px, s->qty, s->cnt);
                }
            }
        }
    }
    
    FlatMap<uint64_t, LevelState, PRICE_MAP_CAPACITY> bid_levels_;
    FlatMap<uint64_t, LevelState, PRICE_MAP_CAPACITY> ask_levels_;
};

} // namespace hft