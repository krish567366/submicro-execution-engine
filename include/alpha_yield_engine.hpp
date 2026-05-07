#pragma once

#include "common_types.hpp"
#include "venue_correlation.hpp"
#include "kalman_alpha.hpp"
#include "neuromorphic_core.hpp"
#include "risk_control.hpp"
#include "queue_dynamics.hpp"
#include "fixed_point.hpp"
#include <cmath>
#include <algorithm>
#include <array>

namespace hft {
namespace mm {

/**
 * Alpha Yield Engine 8.0 (Quant-Ready Singularity)
 * 
 * Major Features:
 * 1. Queue Position Engineering: Refill/Decay modeling.
 * 2. Fixed-Point Conviction Math: Low-latency conviction scaling.
 * 3. Cache-Line Isolated Core.
 */
class AlphaYieldEngine {
public:
    struct Params {
        double risk_aversion = 1.0; double rebate_value = 0.002; double inventory_cap = 1000.0;
        double intensity_k = 1.0; double ofi_k = 0.5;
    };

    struct Quote {
        double bid_px; double bid_qty; double ask_px; double ask_qty;
        bool toxic_cancel = false; bool spike_alert = false;
        double smooth_mid = 0;
    };

    class alignas(HFT_CACHE_LINE) DarwinianOptimizer {
    public:
        void update(double success_rate, double volatility) {
            double target = (success_rate > 0.45) ? 0.94 : 0.75;
            threshold_ = (0.995 * threshold_) + (0.005 * target);
            threshold_ = std::clamp(threshold_, 0.65, 0.98);
        }
        double get_threshold() const { return threshold_; }
    private:
        double threshold_ = 0.88;
    };

    class alignas(HFT_CACHE_LINE) ScalarKalman {
    public:
        ScalarKalman() : x_{0, 0}, p_{{1, 0}, {0, 1}} {}
        void step(double z, double dt) {
            if (HFT_UNLIKELY(x_[0] == 0)) x_[0] = z;
            x_[0] += x_[1] * dt;
            p_[0][0] += dt * (p_[1][1] * dt + p_[0][1] + p_[1][0]) + 0.0001;
            p_[1][1] += 0.001;
            double y = z - x_[0], s = p_[0][0] + 0.01;
            double k0 = p_[0][0] / s, k1 = p_[1][0] / s;
            x_[0] += k0 * y; x_[1] += k1 * y;
            p_[0][0] *= (1.0 - k0); p_[1][1] *= 0.99;
        }
        double get_price() const { return x_[0]; }
        double get_velocity() const { return x_[1]; }
    private:
        double x_[2]; double p_[2][2];
    };

    AlphaYieldEngine() : params_({}) {}
    AlphaYieldEngine(const Params& p) : params_(p) {}
    void update_params(const Params& p) { params_ = p; }
    const Params& get_params() const { return params_; }

    template<size_t MaxLevels>
    inline Quote calculate_advanced_quotes(
        double raw_mid, double low_latency_dt, double volatility, int32_t inventory,
        double ofi, double toxicity,
        const std::array<double, MaxLevels>& bid_pxs, const std::array<uint64_t, MaxLevels>& bid_qts,
        const std::array<double, MaxLevels>& ask_pxs, const std::array<uint64_t, MaxLevels>& ask_qts,
        uint32_t depth_count, const MultiVenueCorrelator& correlator,
        double reversion, uint64_t now_ns, double probe_rate, double entropy,
        double chaos_lambda, const QueueDynamicsEngine& queue_eng) {

        if (HFT_UNLIKELY(last_ts_ == 0)) last_ts_ = now_ns;
        double dt = (double)(now_ns - last_ts_) / 1e9;
        last_ts_ = now_ns;
        
        kalman_.step(raw_mid, std::max(1e-6, dt));
        double smooth_mid = kalman_.get_price();
        double mid_vel = kalman_.get_velocity();

        float currents[256];
        currents[0] = static_cast<float>(std::abs(ofi) * 0.01f);
        currents[1] = static_cast<float>(std::abs(mid_vel) * 100.0f);
        uint64_t fire_mask = 0;
        snn_.input_current(currents, &fire_mask);
        bool snn_spike = (fire_mask != 0);

        darwin_.update(probe_rate, volatility);
        double thresh_val = std::max(0.92, darwin_.get_threshold() - (chaos_lambda * 0.02));
        
        // Use FixedPoint for high-resolution conviction math
        hft::math::FixedPoint thresh(thresh_val);
        hft::math::FixedPoint risk_mult(correlator.get_global_risk_factor(0));
        
        double effectively_toxic = toxicity * (1.0 + (1.0 - entropy) * 0.02);

        Quote out; out.toxic_cancel = false; out.spike_alert = snn_spike; out.smooth_mid = smooth_mid;
        if (HFT_UNLIKELY(snn_spike && std::abs(ofi) > 20000.0)) out.toxic_cancel = true;

        auto score_side = [&](bool buy) {
            const auto& pxs = buy ? bid_pxs : ask_pxs;
            const auto& qts = buy ? bid_qts : ask_qts;
            double best_px = pxs[0], best_score = -1e18, acc = 0;
            
            HFT_PREFETCH(&pxs[0], 0, 3);

            for (uint32_t i = 0; i < depth_count; ++i) {
                if (HFT_UNLIKELY(pxs[i] == 0)) break;
                double spread = std::abs(pxs[i] - smooth_mid);
                
                // Queue Position Engineering: Probability of Fill at this level
                double p_fill = queue_eng.get_fill_probability(buy, i, 10000.0) * std::exp(-0.0001 * acc / 2.0);
                double queue_score = queue_eng.get_join_timing_score(buy, i);

                double p_adv = effectively_toxic * (1.0 - p_fill) * risk_mult.to_double();
                if (HFT_UNLIKELY(p_adv > thresh.to_double())) out.toxic_cancel = true;
                
                double inv_p = static_cast<double>(inventory) * static_cast<double>(inventory) * params_.risk_aversion * volatility;
                
                // Final Score includes Join Dynamics
                double score = (p_fill * spread) - (p_adv * volatility) - (inv_p * 1e-6) + (queue_score * 0.001);
                
                if (buy) { if(mid_vel > 0.0001) score += (mid_vel * 75.0); }
                else { if(mid_vel < -0.0001) score += (-mid_vel * 75.0); }

                if (score > best_score) { best_score = score; best_px = pxs[i]; }
                acc += static_cast<double>(qts[i]);
            }
            return best_px;
        };
        out.bid_px = score_side(true); out.ask_px = score_side(false);
        
        double q_norm = static_cast<double>(inventory) / params_.inventory_cap;
        double base = (effectively_toxic < 0.95 ? 200.0 : 100.0);
        if (HFT_UNLIKELY(chaos_lambda > 0.15)) base *= 0.9;
        
        out.bid_qty = out.bid_px > 0 ? base * std::exp(-1.5 * q_norm) : 0;
        out.ask_qty = out.ask_px > 0 ? base * std::exp(1.5 * q_norm) : 0;
        return out;
    }
private:
    alignas(HFT_CACHE_LINE) Params params_; 
    DarwinianOptimizer darwin_; 
    ScalarKalman kalman_; 
    neuromorphic::SpikingCore snn_; 
    uint64_t last_ts_ = 0;
};
} }
