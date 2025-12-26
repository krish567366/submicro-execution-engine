#pragma once

#include "common_types.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

namespace hft {

class HawkesEngine {
private:
    double mu_b, mu_s;
    double alpha_self;
    double alpha_cross;
    double beta, gamma;

    std::deque<TradingEvent> buy_hist, sell_hist;
    size_t max_hist;

    double lambda_b, lambda_s;
    uint64_t last_update;

    void recalc() {
        auto current_time_ns = to_nanos(now());

        lambda_b = mu_b;
        lambda_s = mu_s;

        for (auto& e : buy_hist) {
            int64_t event_time_ns = to_nanos(e.arrival_time);
            double dt = (current_time_ns - event_time_ns) * 1e-9;
            if (dt > 0) {
                double row = std::pow(beta + dt, -gamma);
                lambda_b += alpha_self * row;
                lambda_s += alpha_cross * row;
            }
        }

        for (auto& e : sell_hist) {
            int64_t event_time_ns = to_nanos(e.arrival_time);
            double dt = (current_time_ns - event_time_ns) * 1e-9;
            if (dt > 0) {
                double row = std::pow(beta + dt, -gamma);
                lambda_s += alpha_self * row;
                lambda_b += alpha_cross * row;
            }
        }

        last_update = current_time_ns;
    }

public:
    HawkesEngine(double mu_buy = 10.0, double mu_sell = 10.0,
                 double a_self = 0.5, double a_cross = 0.2,
                 double b = 1e-3, double g = 1.5, size_t hist = 1000)
        : mu_b(mu_buy), mu_s(mu_sell), alpha_self(a_self), alpha_cross(a_cross),
          beta(b), gamma(g), max_hist(hist), lambda_b(mu_buy), lambda_s(mu_sell),
          last_update(0) {

        if (gamma <= 1.0) gamma = 1.5;
        if (beta <= 0) beta = 1e-6;
    }

    void update(const TradingEvent& event) {
        if (event.event_type  == Side::BUY) {
            buy_hist.push_back(event);
            if (buy_hist.size() > max_hist) buy_hist.pop_front();
        } else {
            sell_hist.push_back(event);
            if (sell_hist.size() > max_hist) sell_hist.pop_front();
        }

        recalc();
    }

    double buy_intensity() const { return lambda_b; }
    double sell_intensity() const { return lambda_s; }

    double get_sell_intensity() const {
        return intensity_sell_;
    }

    double get_intensity_imbalance() const {
        const double total = intensity_buy_ + intensity_sell_;
        if (total < 1e-10) return 0.0;
        return (intensity_buy_ - intensity_sell_) / total;
    }

    double predict_buy_intensity(Duration forecast_horizon) const {
        Timestamp future_time = current_time_ + forecast_horizon;
        return compute_intensity(Side::BUY, future_time);
    }

    double predict_sell_intensity(Duration forecast_horizon) const {
        Timestamp future_time = current_time_ + forecast_horizon;
        return compute_intensity(Side::SELL, future_time);
    }

    void reset() {
        buy_events_.clear();
        sell_events_.clear();
        intensity_buy_ = mu_buy_;
        intensity_sell_ = mu_sell_;
        current_time_ = now();
    }

    size_t get_buy_event_count() const { return buy_events_.size(); }
    size_t get_sell_event_count() const { return sell_events_.size(); }

private:

    double power_law_kernel(double tau_seconds) const {
        if (tau_seconds < 0.0) return 0.0;
        return std::pow(beta_ + tau_seconds, -gamma_);
    }

    void recalculate_intensity() {
        intensity_buy_ = compute_intensity(Side::BUY, current_time_);
        intensity_sell_ = compute_intensity(Side::SELL, current_time_);
    }

    double compute_intensity(Side side, Timestamp eval_time) const {
        double intensity = (side  == Side::BUY) ? mu_buy_ : mu_sell_;

        const int64_t eval_nanos = to_nanos(eval_time);

        const auto& same_events = (side  == Side::BUY) ? buy_events_ : sell_events_;
        for (const auto& evt : same_events) {
            const int64_t event_nanos = to_nanos(evt.arrival_time);
            if (event_nanos < eval_nanos) {
                const double tau_seconds = (eval_nanos - event_nanos) * 1e-9;
                intensity += alpha_self_ * power_law_kernel(tau_seconds);
            }
        }

        const auto& cross_events = (side  == Side::BUY) ? sell_events_ : buy_events_;
        for (const auto& evt : cross_events) {
            const int64_t event_nanos = to_nanos(evt.arrival_time);
            if (event_nanos < eval_nanos) {
                const double tau_seconds = (eval_nanos - event_nanos) * 1e-9;
                intensity += alpha_cross_ * power_law_kernel(tau_seconds);
            }
        }

        return std::max(intensity, 1e-10);
    }

    double mu_buy_;
    double mu_sell_;
    double alpha_self_;
    double alpha_cross_;
    double beta_;
    double gamma_;
    size_t max_history_;

    Timestamp current_time_;
    double intensity_buy_;
    double intensity_sell_;

    std::deque<TradingEvent> buy_events_;
    std::deque<TradingEvent> sell_events_;
};

}