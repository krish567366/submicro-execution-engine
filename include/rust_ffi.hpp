#pragma once

#include "common_types.hpp"
#include "shared_memory.hpp"
#include <cstdint>

// ============================================================================
// C++/Rust FFI Bridge
// Provides seamless integration between C++ core and Rust safety features
// ============================================================================

namespace hft {
namespace rust_ffi {

// ============================================================================
// Opaque Rust handles (managed by Rust)
// ============================================================================

struct RustMarketMaker;
struct RustRiskControl;
struct RustLockFreeQueue;

// ============================================================================
// C++ -> Rust FFI Functions
// ============================================================================

extern "C" {
    // Rust Market Maker
    RustMarketMaker* rust_market_maker_new(double risk_aversion, 
                                           double volatility, 
                                           double tick_size);
    void rust_market_maker_free(RustMarketMaker* mm);
    void rust_market_maker_generate_quotes(RustMarketMaker* mm,
                                          const MarketTick* tick,
                                          int64_t inventory,
                                          double* bid_out,
                                          double* ask_out);
    
    // Rust Risk Control
    RustRiskControl* rust_risk_control_new(int64_t max_position);
    void rust_risk_control_free(RustRiskControl* rc);
    bool rust_risk_control_check_pre_trade(RustRiskControl* rc,
                                           const Order* order,
                                           int64_t current_position);
    void rust_risk_control_trigger_kill_switch(RustRiskControl* rc);
    bool rust_risk_control_is_halted(RustRiskControl* rc);
    
    // Rust Lock-Free Queue
    RustLockFreeQueue* rust_queue_new(size_t capacity);
    void rust_queue_free(RustLockFreeQueue* queue);
    bool rust_queue_push(RustLockFreeQueue* queue, const MarketTick* tick);
    bool rust_queue_pop(RustLockFreeQueue* queue, MarketTick* tick);
    bool rust_queue_is_empty(RustLockFreeQueue* queue);
    size_t rust_queue_size(RustLockFreeQueue* queue);
    
    // Benchmarking
    void rust_benchmark_queue_throughput();
}

// ============================================================================
// C++ Wrappers for Rust Components (RAII)
// ============================================================================

class RustMarketMakerWrapper {
public:
    RustMarketMakerWrapper(double risk_aversion, double volatility, double tick_size)
        : handle_(rust_market_maker_new(risk_aversion, volatility, tick_size)) {}
    
    ~RustMarketMakerWrapper() {
        if (handle_) {
            rust_market_maker_free(handle_);
        }
    }
    
    // Delete copy, allow move
    RustMarketMakerWrapper(const RustMarketMakerWrapper&) = delete;
    RustMarketMakerWrapper& operator=(const RustMarketMakerWrapper&) = delete;
    RustMarketMakerWrapper(RustMarketMakerWrapper&& other) noexcept 
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    QuotePair generate_quotes(const MarketTick& tick, int64_t inventory) const {
        QuotePair quotes;
        rust_market_maker_generate_quotes(handle_, &tick, inventory,
                                         &quotes.bid_price, &quotes.ask_price);
        quotes.spread = quotes.ask_price - quotes.bid_price;
        quotes.mid_price = (quotes.bid_price + quotes.ask_price) / 2.0;
        quotes.generated_at = now();
        return quotes;
    }
    
private:
    RustMarketMaker* handle_;
};

class RustRiskControlWrapper {
public:
    explicit RustRiskControlWrapper(int64_t max_position)
        : handle_(rust_risk_control_new(max_position)) {}
    
    ~RustRiskControlWrapper() {
        if (handle_) {
            rust_risk_control_free(handle_);
        }
    }
    
    RustRiskControlWrapper(const RustRiskControlWrapper&) = delete;
    RustRiskControlWrapper& operator=(const RustRiskControlWrapper&) = delete;
    
    bool check_pre_trade(const Order& order, int64_t current_position) const {
        return rust_risk_control_check_pre_trade(handle_, &order, current_position);
    }
    
    void trigger_kill_switch() const {
        rust_risk_control_trigger_kill_switch(handle_);
    }
    
    bool is_halted() const {
        return rust_risk_control_is_halted(handle_);
    }
    
private:
    RustRiskControl* handle_;
};

// ============================================================================
// Rust -> C++ FFI Functions (exported from C++)
// ============================================================================

extern "C" {
    // Shared memory operations
    bool shm_write_tick(const char* name, const MarketTick* tick);
    bool shm_read_tick(const char* name, MarketTick* tick);
    
    // Hawkes engine integration
    void cpp_hawkes_update(void* engine, const MarketTick* tick);
    
    // FPGA inference integration
    void cpp_fpga_predict(void* engine, const double* features, double* output);
}

} // namespace rust_ffi
} // namespace hft
