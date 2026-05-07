#pragma once

#include "common_types.hpp"
#include "tsx_lock.hpp"
#include <cmath>
#include <algorithm>
#include <deque>

namespace hft {
namespace mm {

/**
 * Volatility Estimator (Real-Time Weighted)
 */
class AdaptiveVolEstimator {
public:
    AdaptiveVolEstimator(double alpha = 0.01) : alpha_(alpha), variance_(1e-6) {}
    inline void update(double price) {
        if (last_price_ > 0) {
            double ret = std::log(price / last_price_);
            variance_ = (1.0 - alpha_) * variance_ + alpha_ * (ret * ret);
        }
        last_price_ = price;
    }
    inline double get_vol() const { return std::sqrt(variance_ * 252 * 6.5 * 3600); }
    inline double get_raw_var() const { return variance_; }
private:
    double alpha_, variance_, last_price_ = 0;
};

/**
 * Inventory Warehouse Controller (TSX Optimized)
 */
class InventoryWarehouse {
public:
    struct Stats {
        int32_t position = 0; double cost_basis = 0; double realized_pnl = 0;
        double unrealized_pnl = 0; double fees_paid = 0; double rebates_earned = 0;
    };

    void on_fill(int32_t qty, double price, double fee_rebate) {
        if (qty == 0) return;
        lock_.acquire();
        if (fee_rebate > 0) stats_.rebates_earned += fee_rebate;
        else stats_.fees_paid += std::abs(fee_rebate);

        bool is_buy = qty > 0;
        int32_t new_pos = stats_.position + qty;

        if ((stats_.position > 0 && !is_buy) || (stats_.position < 0 && is_buy)) {
            int32_t closed_qty = std::min(std::abs(qty), std::abs(stats_.position));
            double pnl = closed_qty * (price - stats_.cost_basis);
            if (stats_.position < 0) pnl = -pnl;
            stats_.realized_pnl += pnl;
        }

        if (std::abs(new_pos) > std::abs(stats_.position)) {
            int32_t opened_qty = std::abs(new_pos) - std::abs(stats_.position);
            stats_.cost_basis = ((std::abs(stats_.position) * stats_.cost_basis) + (opened_qty * price)) / std::abs(new_pos);
        }
        stats_.position = new_pos;
        lock_.release();
    }

    void update_unrealized(double mid_price) {
        stats_.unrealized_pnl = stats_.position * (mid_price - stats_.cost_basis);
    }

    const Stats& get_stats() const { return stats_; }

private:
    Stats stats_;
    concurrency::RestrictedTransactionLock lock_;
};

} }
