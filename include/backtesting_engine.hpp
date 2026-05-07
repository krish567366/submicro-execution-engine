#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <deque>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>

#include "common_types.hpp"
#include "venue_correlation.hpp"
#include "spectral_microstructure.hpp"
#include "micro_price.hpp"
#include "toxicity_radar.hpp"
#include "hawkes_engine.hpp"
#include "alpha_yield_engine.hpp"
#include "inventory_warehouse.hpp"
#include "risk_control.hpp"
#include "chaos_theory_lyapunov.hpp"
#include "smart_order_router.hpp"
#include "circular_buffer.hpp"
#include "queue_dynamics.hpp"

namespace hft {
namespace backtest {

class ExchangeSimulator {
public:
    struct SimulatedOrder { uint64_t order_id; double price; uint64_t qty; bool is_buy; uint64_t entry_ts; };
    struct FillReport { uint64_t order_id; double price; uint64_t qty; uint64_t time; bool is_buy; };

    ExchangeSimulator(double m, double s) : lat_dist_(m, s) { std::random_device rd; rng_.seed(rd()); }
    void set_fpga_mode(bool v) { fpga_mode_ = v; }
    void set_jitter(double n) { jitter_ns_ = n; }
    void submit(uint64_t ts, uint64_t id, double px, uint64_t qty, bool buy) {
        double lat = fpga_mode_ ? 35.0 : std::max(100.0, lat_dist_(rng_));
        lat += jitter_ns_ * (double(rand())/RAND_MAX);
        orders_.push_back({id, px, qty, buy, ts + (uint64_t)lat});
    }
    void cancel_all() { orders_.clear(); }
    std::vector<FillReport> on_tick(uint64_t ts, const MarketTick& tick) {
        std::vector<FillReport> fills;
        auto it = orders_.begin();
        while (it != orders_.end()) {
            bool filled = false;
            if (HFT_LIKELY(ts >= it->entry_ts)) {
                if (it->is_buy && tick.ask_price <= it->price) { fills.push_back({it->order_id, tick.ask_price, it->qty, ts, true}); filled = true; }
                else if (!it->is_buy && tick.bid_price >= it->price) { fills.push_back({it->order_id, tick.bid_price, it->qty, ts, false}); filled = true; }
                else if (tick.trade_volume > 0) {
                    double trade_px = (tick.trade_side == Side::BUY) ? tick.ask_price : tick.bid_price;
                    if (std::abs(trade_px - it->price) < 1e-7) {
                        uint64_t f_qty = std::min(it->qty, (uint64_t)tick.trade_volume);
                        fills.push_back({it->order_id, trade_px, f_qty, ts, it->is_buy});
                        it->qty -= f_qty; if (it->qty == 0) filled = true;
                    }
                }
            }
            if (filled) it = orders_.erase(it); else ++it;
        }
        return fills;
    }
private:
    std::list<SimulatedOrder> orders_; std::normal_distribution<> lat_dist_; std::mt19937 rng_; bool fpga_mode_ = false; double jitter_ns_ = 0;
};

class MessageGovernor {
public:
    void set_limit(uint32_t l) { limit_ = l; }
    bool allow(uint64_t ns) {
        if (HFT_UNLIKELY(ns - last_reset_ > 1000000000ULL)) { count_ = 0; last_reset_ = ns; }
        if (HFT_UNLIKELY(count_ >= limit_)) return false;
        count_++; return true;
    }
private:
    uint32_t count_ = 0; uint32_t limit_ = 5000; uint64_t last_reset_ = 0;
};

template <typename Strategy>
class BacktestEngine {
public:
    struct Results { double pnl; double rebates; int32_t pos; size_t toxic; size_t race; size_t governed; size_t fills; size_t spikes; double avg_chaos; size_t aggressive_fills; };

    BacktestEngine(const std::string& path, const mm::AlphaYieldEngine::Params& ap)
        : exchange_(5000, 1000), ay_(ap), risk_(10000), sor_(0.5, -0.2) 
    {
        mm::AlphaYieldEngine::Params p1 = ap; p1.risk_aversion *= 0.5; shadow_pool_[0] = mm::AlphaYieldEngine(p1);
        p1 = ap; p1.risk_aversion *= 2.0; shadow_pool_[1] = mm::AlphaYieldEngine(p1);
        p1 = ap; p1.inventory_cap *= 0.5; shadow_pool_[2] = mm::AlphaYieldEngine(p1);
        p1 = ap; p1.inventory_cap *= 2.0; shadow_pool_[3] = mm::AlphaYieldEngine(p1);
        shadow_pnl_.fill(0.0);
        int fd = open(path.c_str(), O_RDONLY); struct stat sb; fstat(fd, &sb);
        data_size_ = sb.st_size;
        raw_ = (const char*)mmap(NULL, data_size_, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        num_ticks_ = data_size_ / sizeof(MarketTick);
    }

    void enable_fpga_mode(bool v) { exchange_.set_fpga_mode(v); }
    void set_message_rate_limit(uint32_t l) { governor_.set_limit(l); }
    void set_jitter(double j) { exchange_.set_jitter(j); }
    void set_silent(bool s) { silent_ = s; }

    Results run(Strategy& strat) {
        const MarketTick* ticks = (const MarketTick*)raw_;
        hft::container::StaticCircularBuffer<double, 64> mid_hist;
        double last_mid = 0; uint64_t last_bid_sz = 0, last_ts_ns = 0;
        size_t toxic_cnt = 0, gov_cnt = 0, fill_cnt = 0, spike_cnt = 0, agg_fills = 0;
        double total_chaos = 0, chaos_lambda = 0;

        for (size_t i = 0; i < num_ticks_; ++i) {
            if (HFT_LIKELY(i + 1 < num_ticks_)) HFT_PREFETCH(&ticks[i+1], 0, 3);
            const MarketTick& t = ticks[i];
            uint64_t ns = hft::to_nanos(t.timestamp);
            double dt = (HFT_UNLIKELY(last_ts_ns == 0)) ? 0.001 : (double)(ns - last_ts_ns) / 1e9;
            last_ts_ns = ns;

            mid_hist.push(t.mid_price);
            spec_.push_sample(t.mid_price - last_mid);
            double entropy = spec_.compute_spectral_entropy();
            last_mid = t.mid_price;

            queue_eng_.update(t, ns);

            chaos_detector_.push(t.mid_price);
            if (HFT_UNLIKELY(i % 100 == 0)) chaos_lambda = chaos_detector_.calculate_lambda();
            total_chaos += std::max(0.0, chaos_lambda);

            if (HFT_UNLIKELY(i > 0 && i % 10000 == 0)) {
                int w = 0; for(int s=1; s<4; ++s) if(shadow_pnl_[s] > shadow_pnl_[w]) w = s;
                ay_.update_params(shadow_pool_[w].get_params());
                shadow_pnl_.fill(0.0);
            }

            double tox = vpin_.update_trade(t.mid_price, t.trade_volume, t.trade_side == Side::BUY).vpin;
            double ofi = (double)t.bid_size - (double)last_bid_sz;
            vol_.update(t.mid_price);

            for (int s = 0; s < 4; ++s) {
                auto sq = shadow_pool_[s].calculate_advanced_quotes(t.mid_price, dt, vol_.get_vol(), 0, ofi/1000.0, tox, t.bid_prices, t.bid_sizes, t.ask_prices, t.ask_sizes, 10, corr_, 0.5, ns, 0.5, entropy, chaos_lambda, queue_eng_);
                if (std::abs(t.mid_price - sq.smooth_mid) < 0.0008) shadow_pnl_[s] += 0.05;
            }

            auto fills = exchange_.on_tick(ns, t);
            for (auto& f : fills) {
                strat.on_fill(f.order_id, f.price, f.qty);
                ware_.on_fill(f.is_buy ? (int32_t)f.qty : -(int32_t)f.qty, f.price, 0.002);
                fill_cnt++;
            }

            auto q = ay_.calculate_advanced_quotes(t.mid_price, dt, vol_.get_vol(), ware_.get_stats().position, ofi/1000.0, tox, t.bid_prices, t.bid_sizes, t.ask_prices, t.ask_sizes, 10, corr_, 0.5, ns, 0.5, entropy, chaos_lambda, queue_eng_);
            if (HFT_UNLIKELY(q.spike_alert)) spike_cnt++;

            // Passive only for baseline
            if (HFT_UNLIKELY(q.toxic_cancel)) { exchange_.cancel_all(); toxic_cnt++; continue; }
            if (HFT_UNLIKELY(!governor_.allow(ns))) { gov_cnt++; continue; }
            
            if (risk_.check_pre_trade_limits({q.bid_qty, q.ask_qty}, t.mid_price)) {
                exchange_.cancel_all();
                exchange_.submit(ns, ++id_gen_, q.bid_px, (uint64_t)q.bid_qty, true);
                exchange_.submit(ns, ++id_gen_, q.ask_px, (uint64_t)q.ask_qty, false);
            }
            last_bid_sz = t.bid_size;
        }
        auto& s = ware_.get_stats();
        return {s.realized_pnl, s.rebates_earned, s.position, toxic_cnt, 0, gov_cnt, fill_cnt, spike_cnt, total_chaos / num_ticks_, agg_fills};
    }

private:
    const char* raw_; size_t data_size_, num_ticks_;
    ExchangeSimulator exchange_; mm::AlphaYieldEngine ay_; RiskControl risk_;
    mm::ToxicityRadar vpin_; mm::InventoryWarehouse ware_; mm::AdaptiveVolEstimator vol_;
    mm::MultiVenueCorrelator corr_; mm::ImpactReversionGate gate_; mm::SharpLeadLagRadar radar_;
    spectral::SpectralAnalyzer spec_; MessageGovernor governor_;
    hft::execution::SmartOrderRouter sor_;
    uint64_t id_gen_ = 0; bool silent_ = false;
    chaos::LyapunovExponent chaos_detector_;
    std::array<mm::AlphaYieldEngine, 4> shadow_pool_;
    std::array<double, 4> shadow_pnl_;
    mm::QueueDynamicsEngine queue_eng_;
};
} }