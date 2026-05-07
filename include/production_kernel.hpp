#pragma once

#include "common_types.hpp"
#include "fast_lob.hpp"
#include "alpha_yield_engine.hpp"
#include "risk_control.hpp"
#include "solarflare_efvi.hpp"
#include "jitter_profiler.hpp"
#include "metrics_collector.hpp"
#include "production_logging.hpp"
#include "inventory_warehouse.hpp"
#include "hawkes_engine.hpp"
#include "micro_price.hpp"
#include "toxicity_radar.hpp"
#include "simd_fix_parser.hpp"
#include "preserialized_orders.hpp"

#include <atomic>
#include <thread>

namespace hft {

// Type Aliases for Production
using EfviDriver = networking::solarflare::EfviDriver;
using SimdFixParser = parsing::SimdFixParser;
using OrderTemplater = networking::PreserializedOrderTemplates;

/**
 * PRODUCTION TRADING KERNEL
 */
class ProductionKernel {
public:
    struct Config {
        int cpu_core_id;
        std::string interface_name;
        mm::AlphaYieldEngine::Params mm_params;
        // Risk params from RiskControl
    };

    ProductionKernel(const Config& config)
        : config_(config),
          lob_("PROD_LOB"),
          yield_engine_(config.mm_params),
          risk_(1000, 50000.0, 1000000.0),
          buy_intensity_(0.1, 0.5, 2.0),
          sell_intensity_(0.1, 0.5, 2.0),
          running_(false)
    {}

    void run() {
        pin_to_core(config_.cpu_core_id);
        nic_driver_.init(config_.interface_name.c_str());
        warmup();
        
        running_.store(true);
        std::cout << "[HOT PATH] Kernel Active on Core " << config_.cpu_core_id << std::endl;

        while (running_.load(std::memory_order_relaxed)) {
            // Hot path logic using the unified driver callback or polling
            // For now, using a polling style as drafted
            
            // Note: Efficient polling requires special handling of the driver
            // Here we use a simplified poll to match intended flow
            nic_driver_.poll([&](char* packet_ptr, int packet_len) {
                hft::profiling::JitterProbe probe("TickToTrade");
                
                MarketTick tick;
                if (HFT_UNLIKELY(!fix_parser_.parse_simd(packet_ptr, packet_len, tick))) {
                    return; 
                }

                // Correct LOB update (using MarketTick members)
                lob_.process_update(0, (uint8_t)tick.trade_side, tick.asset_id, tick.bid_price, (double)tick.bid_size, true);
                
                // Update Alpha Signals
                bool is_trade = (tick.trade_volume > 0);
                if (is_trade) {
                    uint64_t ns = hft::to_nanos(tick.timestamp);
                    if (tick.trade_side == Side::BUY) buy_intensity_.update(ns);
                    else sell_intensity_.update(ns);
                }
                
                double micro = mm::MicroPrice::calculate(tick.bid_price, tick.ask_price, (double)tick.bid_size, (double)tick.ask_size);
                double toxicity = vpin_radar_.update_trade(tick.mid_price, tick.trade_volume, tick.trade_side == Side::BUY).vpin;
                
                auto quote = yield_engine_.calculate_quotes(
                    micro, 
                    0.02, 
                    warehouse_.get_stats().position,
                    0.0, // OFI
                    buy_intensity_.current_value(hft::to_nanos(tick.timestamp)),
                    sell_intensity_.current_value(hft::to_nanos(tick.timestamp)),
                    toxicity
                );

                if (HFT_LIKELY(risk_.check_pre_trade_limits({quote.bid_qty, quote.ask_qty}, 0))) {
                    char* order_packet = nullptr;
                    size_t order_len = 0;
                    order_templater_.prepare_buy(quote.bid_px, (uint64_t)quote.bid_qty, &order_packet, &order_len);
                    nic_driver_.send_pio(order_packet, (int)order_len);
                }
            });

            if (HFT_UNLIKELY(should_do_housekeeping())) {
                do_housekeeping();
            }
        }
    }

    void stop() {
        running_.store(false);
    }

private:
    void pin_to_core(int core) {
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
        (void)core;
    }

    void warmup() { /* ... */ }

    bool should_do_housekeeping() {
        static uint64_t last_hk = 0;
        uint64_t now_tsc = hft::timing::read_tsc();
        if (now_tsc - last_hk > 3000000) { 
            last_hk = now_tsc;
            return true;
        }
        return false;
    }

    void do_housekeeping() { /* ... */ }

    Config config_;
    hft::FastLOBReconstructor lob_;
    mm::AlphaYieldEngine yield_engine_;
    RiskControl risk_;
    mm::InventoryWarehouse warehouse_;
    quant::HawkesIntensity buy_intensity_;
    quant::HawkesIntensity sell_intensity_;
    mm::ToxicityRadar vpin_radar_;
    
    EfviDriver nic_driver_;
    SimdFixParser fix_parser_;
    OrderTemplater order_templater_;

    std::atomic<bool> running_;
};

} // namespace hft
