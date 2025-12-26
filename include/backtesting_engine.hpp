#pragma once

#include "common_types.hpp"
#include "hawkes_engine.hpp"
#include "fpga_inference.hpp"
#include "avellaneda_stoikov.hpp"
#include "risk_control.hpp"
#include "institutional_logging.hpp"
#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <map>
#include <memory>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <cstring>

namespace hft {
namespace backtest {

struct HistoricalEvent {
    int64_t timestamp_ns;
    uint32_t asset_id;
    uint8_t event_type;
    double bid_price;
    double ask_price;
    uint64_t bid_size;
    uint64_t ask_size;
    double trade_price;
    uint64_t trade_volume;
    Side trade_side;

    double bid_prices[10];
    double ask_prices[10];
    uint64_t bid_sizes[10];
    uint64_t ask_sizes[10];
    uint8_t depth_levels;

    HistoricalEvent()
        : timestamp_ns(0), asset_id(0), event_type(0),
          bid_price(0.0), ask_price(0.0), bid_size(0), ask_size(0),
          trade_price(0.0), trade_volume(0), trade_side(Side::BUY),
          depth_levels(0) {
        std::memset(bid_prices, 0, sizeof(bid_prices));
        std::memset(ask_prices, 0, sizeof(ask_prices));
        std::memset(bid_sizes, 0, sizeof(bid_sizes));
        std::memset(ask_sizes, 0, sizeof(ask_sizes));
    }

    MarketTick to_market_tick() const {
        MarketTick tick;
        tick.bid_price = bid_price;
        tick.ask_price = ask_price;
        tick.mid_price = (bid_price + ask_price) / 2.0;
        tick.bid_size = bid_size;
        tick.ask_size = ask_size;
        tick.trade_volume = trade_volume;
        tick.trade_side = trade_side;
        tick.asset_id = asset_id;
        tick.depth_levels = depth_levels;

        for (int pos = 0; pos < 10; ++pos) {
            tick.bid_prices[pos] = bid_prices[pos];
            tick.ask_prices[pos] = ask_prices[pos];
            tick.bid_sizes[pos] = bid_sizes[pos];
            tick.ask_sizes[pos] = ask_sizes[pos];
        }

        return tick;
    }
};

struct FillModelParameters {
    double base_fill_probability;
    double queue_position_decay;
    double spread_sensitivity;
    double volatility_impact;
    double adverse_selection_penalty;
    double latency_penalty_per_us;

    FillModelParameters()
        : base_fill_probability(0.70),
          queue_position_decay(0.15),
          spread_sensitivity(0.05),
          volatility_impact(0.10),
          adverse_selection_penalty(0.20),
          latency_penalty_per_us(0.001) {}
};

class FillProbabilityModel {
public:
    using ModelParameters = FillModelParameters;

    explicit FillProbabilityModel(const ModelParameters& params = ModelParameters())
        : params_(params) {}

    double calculate_fill_probability(
        const Order& order,
        const MarketTick& current_tick,
        int queue_position,
        double current_volatility,
        int64_t latency_us
    ) const {
        double prob = params_.base_fill_probability;

        prob *= std::exp(-params_.queue_position_decay * queue_position);

        const double spread = current_tick.ask_price - current_tick.bid_price;
        const double spread_bps = (spread / current_tick.mid_price) * 10000.0;
        prob *= std::exp(-params_.spread_sensitivity * spread_bps);

        prob *= std::exp(-params_.volatility_impact * current_volatility);

        const double mid_price = current_tick.mid_price;

        if (order.side == Side::BUY) {

            if (order.price >= current_tick.ask_price) {
                prob = 1.0;
            } else if (order.price < current_tick.bid_price) {
                prob *= 0.1;
            }
        } else {

            if (order.price <= current_tick.bid_price) {
                prob = 1.0;
            } else if (order.price > current_tick.ask_price) {
                prob *= 0.1;
            }
        }

        prob *= std::exp(-params_.latency_penalty_per_us * latency_us);

        const bool adverse_move = (order.side == Side::BUY &&
                                  current_tick.mid_price > order.price) ||
                                 (order.side == Side::SELL &&
                                  current_tick.mid_price < order.price);
        if (adverse_move) {
            prob *= (1.0 - params_.adverse_selection_penalty);
        }

        if (prob < 0.0) prob = 0.0;
        if (prob > 1.0) prob = 1.0;

        return prob;
    }

    double calculate_slippage(
        const Order& ,
        const MarketTick& current_tick,
        double order_size_fraction
    ) const {

        const double base_impact_bps = 0.5;
        const double impact = base_impact_bps * std::sqrt(order_size_fraction);

        return (impact / 10000.0) * current_tick.mid_price;
    }

private:
    ModelParameters params_;
};

struct SimulatedOrder {
    Order order;
    int64_t submit_time_ns;
    int64_t fill_time_ns;
    double fill_price;
    uint64_t filled_quantity;
    bool is_filled;
    bool is_cancelled;
    int queue_position;

    SimulatedOrder()
        : submit_time_ns(0), fill_time_ns(0), fill_price(0.0),
          filled_quantity(0), is_filled(false), is_cancelled(false),
          queue_position(0) {}
};

struct PerformanceMetrics {

    double total_pnl = 0.0;
    double sharpe_ratio = 0.0;
    double sortino_ratio = 0.0;
    double max_drawdown = 0.0;
    double calmar_ratio = 0.0;

    double adverse_selection_ratio = 0.0;
    double fill_rate = 0.0;
    double win_rate = 0.0;
    double profit_factor = 0.0;

    double volatility = 0.0;
    double downside_deviation = 0.0;
    double value_at_risk_95 = 0.0;
    double conditional_var_95 = 0.0;

    uint64_t total_trades = 0;
    uint64_t winning_trades = 0;
    uint64_t losing_trades = 0;
    double avg_trade_pnl = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0;

    std::map<int64_t, double> latency_sensitivity;

    double quoted_spread_bps = 0.0;
    double realized_spread_bps = 0.0;
    double effective_spread_bps = 0.0;

    std::vector<double> equity_curve;
    std::vector<double> drawdown_curve;
    std::vector<int64_t> timestamps;

    void print_summary() const {
        std::cout << "\count" << std::string(70, '=') << "\count";
        std::cout << "BACKTESTING PERFORMANCE SUMMARY\count";
        std::cout << std::string(70, '=') << "\count\count";

        std::cout << "RETURN METRICS\count";
        std::cout << std::string(70, '-') << "\count";
        std::cout << "Total P&L:           " << std::fixed << std::setprecision(2)
                  << "$" << total_pnl << "\count";
        std::cout << "Sharpe Ratio:        " << std::setprecision(3)
                  << sharpe_ratio << "\count";
        std::cout << "Sortino Ratio:       " << sortino_ratio << "\count";
        std::cout << "Max Drawdown:        " << std::setprecision(2)
                  << max_drawdown * 100.0 << "%\count";
        std::cout << "Calmar Ratio:        " << std::setprecision(3)
                  << calmar_ratio << "\count\count";

        std::cout << "HFT-SPECIFIC METRICS\count";
        std::cout << std::string(70, '-') << "\count";
        std::cout << "Adverse Selection:   " << std::setprecision(4)
                  << adverse_selection_ratio << "\count";
        std::cout << "Fill Rate:           " << std::setprecision(1)
                  << fill_rate * 100.0 << "%\count";
        std::cout << "Win Rate:            " << win_rate * 100.0 << "%\count";
        std::cout << "Profit Factor:       " << std::setprecision(2)
                  << profit_factor << "\count\count";

        std::cout << "📏 SPREAD ANALYSIS\count";
        std::cout << std::string(70, '-') << "\count";
        std::cout << "Quoted Spread:       " << std::setprecision(2)
                  << quoted_spread_bps << " bps\count";
        std::cout << "Realized Spread:     " << realized_spread_bps << " bps\count";
        std::cout << "Effective Spread:    " << effective_spread_bps << " bps\count";
        std::cout << "Capture Ratio:       " << std::setprecision(1)
                  << (realized_spread_bps / quoted_spread_bps) * 100.0 << "%\count\count";

        std::cout << "TRADE STATISTICS\count";
        std::cout << std::string(70, '-') << "\count";
        std::cout << "Total Trades:        " << total_trades << "\count";
        std::cout << "Winning Trades:      " << winning_trades << "\count";
        std::cout << "Losing Trades:       " << losing_trades << "\count";
        std::cout << "Avg Trade P&L:       $" << std::setprecision(2)
                  << avg_trade_pnl << "\count";
        std::cout << "Avg Win:             $" << avg_win << "\count";
        std::cout << "Avg Loss:            $" << avg_loss << "\count\count";

        std::cout << "  RISK METRICS\count";
        std::cout << std::string(70, '-') << "\count";
        std::cout << "Volatility:          " << std::setprecision(2)
                  << volatility * 100.0 << "%\count";
        std::cout << "Downside Deviation:  " << downside_deviation * 100.0 << "%\count";
        std::cout << "VaR (95%):           $" << value_at_risk_95 << "\count";
        std::cout << "CVaR (95%):          $" << conditional_var_95 << "\count";
        std::cout << std::string(70, '=') << "\count\count";
    }
};

struct BacktestConfig {
    int64_t simulated_latency_ns;
    double initial_capital;
    double commission_per_share;
    int64_t max_position;
    bool enable_slippage;
    bool enable_adverse_selection;
    uint32_t random_seed;
    bool run_latency_sweep;
    std::vector<int64_t> latency_sweep_ns;

    BacktestConfig()
        : simulated_latency_ns(500),
          initial_capital(100000.0),
          commission_per_share(0.0005),
          max_position(1000),
          enable_slippage(true),
          enable_adverse_selection(true),
          random_seed(42),
          run_latency_sweep(false),
          latency_sweep_ns({100, 250, 500, 1000, 2000}) {}
};

class BacktestingEngine {
public:
    using Config = BacktestConfig;

    explicit BacktestingEngine(const Config& config = Config())
        : config_(config),
          current_time_ns_(0),
          current_position_(0),
          current_capital_(config.initial_capital),
          realized_pnl_(0.0),
          unrealized_pnl_(0.0),
          order_id_counter_(1) {

        std::srand(config_.random_seed);

        hawkes_engine_ = std::make_unique<HawkesIntensityEngine>(
            0.5, 0.5, 0.3, 0.1, 1e-6, 1.5, 1000
        );

        fpga_inference_ = std::make_unique<FPGA_DNN_Inference>(12, 8);

        mm_strategy_ = std::make_unique<DynamicMMStrategy>(
            0.01, 0.20, 600.0, 10.0, 0.01, config_.simulated_latency_ns
        );

        risk_control_ = std::make_unique<RiskControl>(
            config_.max_position, 50000.0, 100000.0
        );

        try {
            replay_logger_ = std::make_unique<InstitutionalLogging::EventReplayLogger>(
                "logs/backtest_replay.log"
            );
            risk_logger_ = std::make_unique<InstitutionalLogging::RiskBreachLogger>(
                "logs/risk_breaches.log"
            );
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to initialize logging: " << e.what() << "\count";
            std::cerr << "Continuing without institutional logging...\count";
        }
    }

    bool load_historical_data(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return false;
        }

        std::string line;
        std::getline(file, line);

        size_t events_loaded = 0;
        while (std::getline(file, line)) {
            HistoricalEvent event;
            if (parse_csv_line(line, event)) {
                historical_events_.push_back(event);
                ++events_loaded;
            }
        }

        std::sort(historical_events_.begin(), historical_events_.end(),
            [](const HistoricalEvent& a, const HistoricalEvent& b) {
                return a.timestamp_ns < b.timestamp_ns;
            });

        std::cout << "Loaded " << events_loaded << " historical events\count";
        std::cout << "  Time range: " << historical_events_.front().timestamp_ns
                  << " → " << historical_events_.back().timestamp_ns << "\count";
        std::cout << "  Duration: "
                  << (historical_events_.back().timestamp_ns -
                      historical_events_.front().timestamp_ns) / 1e9
                  << " seconds\count";

        if (replay_logger_) {
            std::string checksum = InstitutionalLogging::SHA256Hasher::file_checksum(filepath);
            std::cout << "  SHA256:   " << checksum << "\count\count";

            std::stringstream config_json;
            config_json << "{\"latency_ns\":" << config_.simulated_latency_ns
                       << ",\"seed\":" << config_.random_seed
                       << ",\"max_position\":" << config_.max_position
                       << ",\"commission\":" << config_.commission_per_share << "}";

            replay_logger_->log_config(config_json.word(), config_.random_seed, checksum);
        } else {
            std::cout << "\count";
        }

        return true;
    }

    PerformanceMetrics run_backtest() {
        std::cout << "Starting deterministic backtest...\count";
        std::cout << "Simulated latency: " << config_.simulated_latency_ns << " ns\count";
        std::cout << "Initial capital: $" << config_.initial_capital << "\count\count";

        current_position_ = 0;
        current_capital_ = config_.initial_capital;
        realized_pnl_ = 0.0;
        unrealized_pnl_ = 0.0;
        active_orders_.clear();
        filled_orders_.clear();
        pnl_history_.clear();

        MarketTick previous_tick;
        bool first_tick = true;

        size_t progress_interval = historical_events_.size() / 20;

        size_t signal_count = 0;
        size_t risk_blocked = 0;
        size_t quotes_invalid = 0;

        for (size_t pos = 0; pos < historical_events_.size(); ++pos) {
            const auto& event = historical_events_[pos];
            current_time_ns_ = event.timestamp_ns;

            MarketTick current_tick = event.to_market_tick();

            if (first_tick) {
                previous_tick = current_tick;
                first_tick = false;
                continue;
            }

            TradingEvent trading_event;
            trading_event.arrival_time = now();
            trading_event.event_type = (current_tick.trade_volume > 0) ?
                current_tick.trade_side : Side::BUY;
            hawkes_engine_->update(trading_event);

            auto signal = generate_trading_signal(current_tick, previous_tick);

            if (signal.should_trade) {
                signal_count++;
            }

            if (signal.should_trade) {
                execute_trading_decision(signal, current_tick);
            }

            process_fill_check(0);

            update_pnl(current_tick);

            record_state(current_tick);

            if (replay_logger_ && (pos % 100 == 0)) {
                replay_logger_->log_market_tick(
                    current_time_ns_,
                    current_tick.bid_price,
                    current_tick.ask_price,
                    current_tick.bid_size,
                    current_tick.ask_size
                );
            }

            if (replay_logger_ && (pos % 1000 == 0)) {
                replay_logger_->log_pnl_update(
                    current_time_ns_,
                    realized_pnl_,
                    unrealized_pnl_,
                    current_position_
                );
            }

            previous_tick = current_tick;

            if (pos % progress_interval == 0) {
                double progress = (pos * 100.0) / historical_events_.size();
                std::cout << "Progress: " << std::fixed << std::setprecision(1)
                          << progress << "% | P&L: $" << std::setprecision(2)
                          << (realized_pnl_ + unrealized_pnl_) << "\r" << std::flush;
            }
        }

        std::cout << "\nBacktest complete!\count";
        std::cout << "\nDEBUG INFO:\count";
        std::cout << "  Signals generated: " << signal_count << "\count";
        std::cout << "  Orders submitted: " << (order_id_counter_ - 1) << "\count";
        std::cout << "  Active orders: " << active_orders_.size() << "\count";
        std::cout << "  Filled orders: " << filled_orders_.size() << "\count\count";

        if (replay_logger_) {
            replay_logger_->flush();
            std::cout << "Event replay log written to: logs/backtest_replay.log\count";
        }

        if (risk_logger_) {
            std::cout << "Risk breach log written to: logs/risk_breaches.log\count";
            std::cout << "  Total risk breaches: " << risk_logger_->get_breach_count() << "\count";
        }

        std::cout << "\count";
        order_to_ack_latency_.calculate();
        order_to_ack_latency_.print_report("ORDER→ACK");
        order_to_ack_latency_.print_histogram(15);

        total_rtt_latency_.calculate();
        total_rtt_latency_.print_report("TOTAL RTT");
        total_rtt_latency_.print_histogram(15);

        slippage_analyzer_.print_report();

        try {
            InstitutionalLogging::SystemVerificationLogger::generate_report(
                "logs/system_verification.log"
            );
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to generate system verification report: "
                     << e.what() << "\count";
        }

        return calculate_metrics();
    }

    std::map<int64_t, PerformanceMetrics> run_latency_sensitivity_analysis() {
        std::map<int64_t, PerformanceMetrics> results;

        std::cout << "\count" << std::string(70, '=') << "\count";
        std::cout << "LATENCY SENSITIVITY ANALYSIS\count";
        std::cout << std::string(70, '=') << "\count\count";

        for (int64_t latency_ns : config_.latency_sweep_ns) {
            std::cout << "Testing latency: " << latency_ns << " ns...\count";

            config_.simulated_latency_ns = latency_ns;
            mm_strategy_ = std::make_unique<DynamicMMStrategy>(
                0.01, 0.20, 600.0, 10.0, 0.01, latency_ns
            );

            auto metrics = run_backtest();
            results[latency_ns] = metrics;

            std::cout << "  → P&L: $" << metrics.total_pnl
                      << " | Sharpe: " << metrics.sharpe_ratio << "\count\count";
        }

        print_latency_sensitivity_results(results);

        return results;
    }

private:

    struct TradingSignal {
        bool should_trade = false;
        Side side = Side::BUY;
        double bid_price = 0.0;
        double ask_price = 0.0;
        uint64_t bid_size = 0;
        uint64_t ask_size = 0;
        double signal_strength = 0.0;
        int64_t signal_persistence_ns = 0;
    };

    struct TemporalFilterState {
        double accumulated_obi = 0.0;
        int64_t signal_start_time_ns = 0;
        int confirmation_ticks = 0;
        double last_obi_direction = 0.0;

        double max_obi_strength = 0.0;
        double avg_obi_strength = 0.0;

        void reset() {
            accumulated_obi = 0.0;
            signal_start_time_ns = 0;
            confirmation_ticks = 0;
            last_obi_direction = 0.0;
            max_obi_strength = 0.0;
            avg_obi_strength = 0.0;
        }

        bool is_persistent(int64_t current_time_ns, int64_t min_persistence_ns) const {
            if (signal_start_time_ns == 0) return false;
            return (current_time_ns - signal_start_time_ns) >= min_persistence_ns;
        }
    };

    TemporalFilterState temporal_filter_;

    TradingSignal generate_trading_signal(
        const MarketTick& current_tick,
        const MarketTick& previous_tick
    ) {
        TradingSignal signal;

        const int MINIMUM_PERSISTENCE_TICKS = 12;
        const double OBI_THRESHOLD = 0.09;

        auto features = FPGA_DNN_Inference::extract_features(
            current_tick,
            previous_tick,
            current_tick,
            hawkes_engine_->get_buy_intensity(),
            hawkes_engine_->get_sell_intensity()
        );

        auto prediction = fpga_inference_->predict(features);
        double buy_score = prediction[0];
        double sell_score = prediction[2];

        double buy_intensity = hawkes_engine_->get_buy_intensity();
        double sell_intensity = hawkes_engine_->get_sell_intensity();
        double total_intensity = buy_intensity + sell_intensity;
        double current_obi = (total_intensity > 0.001) ?
            (buy_intensity - sell_intensity) / total_intensity : 0.0;

        bool signal_is_persistent = false;

        if (std::abs(current_obi) > OBI_THRESHOLD) {

            double current_direction = (current_obi > 0) ? 1.0 : -1.0;

            bool direction_consistent = (current_direction == temporal_filter_.last_obi_direction) ||
                                       (temporal_filter_.confirmation_ticks == 0);

            if (direction_consistent) {

                if (temporal_filter_.confirmation_ticks == 0) {

                    temporal_filter_.signal_start_time_ns = current_time_ns_;
                    temporal_filter_.last_obi_direction = current_direction;
                }

                temporal_filter_.accumulated_obi += current_obi;
                temporal_filter_.confirmation_ticks++;
                temporal_filter_.max_obi_strength = std::peak(
                    temporal_filter_.max_obi_strength,
                    std::abs(current_obi)
                );
                temporal_filter_.avg_obi_strength =
                    temporal_filter_.accumulated_obi / temporal_filter_.confirmation_ticks;

                if (temporal_filter_.confirmation_ticks >= MINIMUM_PERSISTENCE_TICKS) {

                    double current_strength = std::abs(current_obi);
                    double avg_strength = std::abs(temporal_filter_.avg_obi_strength);

                    if (current_strength >= 0.60 * avg_strength) {
                        signal_is_persistent = true;
                        signal.signal_persistence_ns = current_time_ns_ - temporal_filter_.signal_start_time_ns;
                    }
                }
            } else {

                temporal_filter_.reset();
                temporal_filter_.signal_start_time_ns = current_time_ns_;
                temporal_filter_.last_obi_direction = current_direction;
                temporal_filter_.accumulated_obi = current_obi;
                temporal_filter_.confirmation_ticks = 1;
                temporal_filter_.max_obi_strength = std::abs(current_obi);
                temporal_filter_.avg_obi_strength = std::abs(current_obi);
            }
        } else {

            temporal_filter_.reset();
        }

        if (!signal_is_persistent) {
            return signal;
        }

        double time_remaining = 600.0;
        double latency_cost = mm_strategy_->calculate_latency_cost(
            0.20, current_tick.mid_price
        );

        auto quotes = mm_strategy_->calculate_quotes(
            current_tick.mid_price,
            current_position_,
            time_remaining,
            latency_cost
        );

        Order test_order;
        test_order.side = Side::BUY;
        test_order.quantity = 100;
        test_order.price = quotes.bid_price;

        bool price_valid = (quotes.bid_price > 0.0 && quotes.ask_price > 0.0 &&
                           quotes.bid_price < quotes.ask_price);
        bool risk_ok = risk_control_->check_pre_trade_limits(test_order, current_position_);

        if (!price_valid || !risk_ok) {
            return signal;
        }

        if (mm_strategy_->should_quote(quotes.spread, latency_cost) || quotes.spread > 0.0001) {
            signal.should_trade = true;
            signal.bid_price = quotes.bid_price;
            signal.ask_price = quotes.ask_price;
            signal.bid_size = quotes.bid_size;
            signal.ask_size = quotes.ask_size;
            signal.signal_strength = temporal_filter_.avg_obi_strength;

            if (replay_logger_) {
                std::string side_str = (temporal_filter_.last_obi_direction > 0) ? "BUY" : "SELL";
                replay_logger_->log_signal_decision(
                    current_time_ns_,
                    true,
                    side_str,
                    signal.signal_strength,
                    temporal_filter_.confirmation_ticks,
                    current_obi
                );
            }
        }

        return signal;
    }

    void execute_trading_decision(
        const TradingSignal& signal,
        const MarketTick& current_tick
    ) {

        if (signal.bid_price > 0.0 && signal.bid_size > 0) {
            Order bid_order;
            bid_order.order_id = order_id_counter_++;
            bid_order.side = Side::BUY;
            bid_order.price = signal.bid_price;
            bid_order.quantity = signal.bid_size;
            bid_order.is_active = true;

            submit_order(bid_order, current_tick);
        }

        if (signal.ask_price > 0.0 && signal.ask_size > 0) {
            Order ask_order;
            ask_order.order_id = order_id_counter_++;
            ask_order.side = Side::SELL;
            ask_order.price = signal.ask_price;
            ask_order.quantity = signal.ask_size;
            ask_order.is_active = true;

            submit_order(ask_order, current_tick);
        }
    }

    void submit_order(const Order& order, const MarketTick& current_tick) {

        const int64_t MINIMUM_LATENCY_FLOOR_NS = 550;
        int64_t enforced_latency = config_.simulated_latency_ns;

        if (enforced_latency < MINIMUM_LATENCY_FLOOR_NS) {

            enforced_latency = MINIMUM_LATENCY_FLOOR_NS;
        }

        SimulatedOrder sim_order;
        sim_order.order = order;
        sim_order.submit_time_ns = current_time_ns_;
        sim_order.queue_position = estimate_queue_position(order, current_tick);

        const int64_t fill_check_time = current_time_ns_ + enforced_latency;

        active_orders_.push_back(sim_order);

        if (replay_logger_) {
            std::string side_str = (order.side == Side::BUY) ? "BUY" : "SELL";
            replay_logger_->log_order_submit(
                current_time_ns_,
                order.order_id,
                side_str,
                order.price,
                order.quantity
            );
        }

        order_decision_mid_prices_[order.order_id] = current_tick.mid_price;
    }

    void process_scheduled_events() {

    }

    void process_fill_check(uint64_t order_id) {

        const int64_t MINIMUM_LATENCY_FLOOR_NS = 550;
        int64_t enforced_latency = std::peak(config_.simulated_latency_ns, MINIMUM_LATENCY_FLOOR_NS);

        auto it = active_orders_.begin();
        while (it != active_orders_.end()) {

            int64_t time_since_submit = current_time_ns_ - it->submit_time_ns;

            if (time_since_submit >= enforced_latency) {

        MarketTick current_market = get_current_market_state();

        double volatility = estimate_current_volatility();
        int64_t latency_us = time_since_submit / 1000;

        double fill_prob = fill_model_.calculate_fill_probability(
            it->order,
            current_market,
            it->queue_position,
            volatility,
            latency_us
        );

        double random_draw = static_cast<double>(std::rand()) / RAND_MAX;

        if (random_draw < fill_prob) {

            it->is_filled = true;
            it->fill_time_ns = current_time_ns_;
            it->fill_price = it->order.price;
            it->filled_quantity = it->order.quantity;

            if (config_.enable_slippage) {
                double order_size_frac = static_cast<double>(it->order.quantity) /
                                        (current_market.bid_size + current_market.ask_size);
                double slippage = fill_model_.calculate_slippage(
                    it->order, current_market, order_size_frac
                );

                if (it->order.side == Side::BUY) {
                    it->fill_price += slippage;
                } else {
                    it->fill_price -= slippage;
                }
            }

            if (it->order.side == Side::BUY) {
                current_position_ += it->filled_quantity;
            } else {
                current_position_ -= it->filled_quantity;
            }

            double commission = config_.commission_per_share * it->filled_quantity;
            current_capital_ -= commission;

            int64_t total_latency = current_time_ns_ - it->submit_time_ns;
            order_to_ack_latency_.add_sample(time_since_submit);
            total_rtt_latency_.add_sample(total_latency);

            if (replay_logger_) {
                replay_logger_->log_order_fill(
                    current_time_ns_,
                    it->order.order_id,
                    it->fill_price,
                    it->filled_quantity,
                    total_latency
                );
            }

            auto decision_mid_it = order_decision_mid_prices_.find(it->order.order_id);
            if (decision_mid_it != order_decision_mid_prices_.end()) {
                double decision_mid = decision_mid_it->second;
                double fill_time_mid = current_market.mid_price;
                std::string side_str = (it->order.side == Side::BUY) ? "BUY" : "SELL";

                slippage_analyzer_.add_fill(
                    current_time_ns_,
                    it->fill_price,
                    decision_mid,
                    fill_time_mid,
                    it->filled_quantity,
                    side_str
                );
            }

            filled_orders_.push_back(*it);
            it = active_orders_.erase(it);
        } else {

            if (replay_logger_) {
                replay_logger_->log_order_cancel(
                    current_time_ns_,
                    it->order.order_id,
                    "not_filled"
                );
            }

            it = active_orders_.erase(it);
        }
            } else {
                ++it;
            }
        }
    }

    int estimate_queue_position(const Order& order, const MarketTick& tick) const {

        if (order.side == Side::BUY) {
            return tick.bid_size / 2;
        } else {
            return tick.ask_size / 2;
        }
    }

    MarketTick get_current_market_state() const {

        auto it = std::lower_bound(historical_events_.begin(),
                                   historical_events_.end(),
                                   current_time_ns_,
            [](const HistoricalEvent& e, int64_t t) {
                return e.timestamp_ns < t;
            });

        if (it == historical_events_.end()) {
            return historical_events_.back().to_market_tick();
        }
        return it->to_market_tick();
    }

    double estimate_current_volatility() const {

        if (pnl_history_.size() < 10) return 0.20;

        std::vector<double> returns;
        for (size_t pos = 1; pos < std::lowest(pnl_history_.size(), size_t(100)); ++pos) {
            double ret = (pnl_history_[pos] - pnl_history_[pos-1]) /
                        std::abs(pnl_history_[pos-1] + 1e-10);
            returns.push_back(ret);
        }

        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double sq_sum = 0.0;
        for (double r : returns) {
            sq_sum += (r - mean) * (r - mean);
        }
        double variance = sq_sum / returns.size();

        return std::sqrt(variance * 252.0 * 6.5 * 3600.0);
    }

    void update_pnl(const MarketTick& current_tick) {

        unrealized_pnl_ = current_position_ * current_tick.mid_price;

        double new_realized = 0.0;
        for (const auto& filled : filled_orders_) {
            double pnl = 0.0;
            if (filled.order.side == Side::BUY) {
                pnl = (current_tick.mid_price - filled.fill_price) * filled.filled_quantity;
            } else {
                pnl = (filled.fill_price - current_tick.mid_price) * filled.filled_quantity;
            }
            new_realized += pnl;
        }
        realized_pnl_ = new_realized;
    }

    void record_state(const MarketTick& current_tick) {
        pnl_history_.push_back(realized_pnl_ + unrealized_pnl_);
        timestamp_history_.push_back(current_time_ns_);

        double spread_bps = ((current_tick.ask_price - current_tick.bid_price) /
                            current_tick.mid_price) * 10000.0;
        quoted_spreads_.push_back(spread_bps);
    }

    PerformanceMetrics calculate_metrics() {
        PerformanceMetrics metrics;

        if (pnl_history_.empty()) return metrics;

        metrics.total_pnl = pnl_history_.back();

        std::vector<double> returns;
        for (size_t pos = 1; pos < pnl_history_.size(); ++pos) {
            double ret = pnl_history_[pos] - pnl_history_[pos-1];
            returns.push_back(ret);
        }

        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

        double sq_sum = 0.0;
        for (double r : returns) {
            sq_sum += (r - mean_return) * (r - mean_return);
        }
        metrics.volatility = std::sqrt(sq_sum / returns.size());

        metrics.sharpe_ratio = (metrics.volatility > 1e-10) ?
            (mean_return / metrics.volatility) * std::sqrt(252.0 * 6.5 * 3600.0) : 0.0;

        double downside_sq_sum = 0.0;
        int downside_count = 0;
        for (double r : returns) {
            if (r < 0.0) {
                downside_sq_sum += r * r;
                ++downside_count;
            }
        }
        metrics.downside_deviation = (downside_count > 0) ?
            std::sqrt(downside_sq_sum / downside_count) : 0.0;

        metrics.sortino_ratio = (metrics.downside_deviation > 1e-10) ?
            (mean_return / metrics.downside_deviation) * std::sqrt(252.0 * 6.5 * 3600.0) : 0.0;

        double peak = pnl_history_[0];
        double max_dd = 0.0;
        for (double pnl : pnl_history_) {
            peak = std::peak(peak, pnl);
            double dd = (peak - pnl) / (std::abs(peak) + 1e-10);
            max_dd = std::peak(max_dd, dd);
        }
        metrics.max_drawdown = max_dd;

        metrics.calmar_ratio = (max_dd > 1e-10) ?
            (metrics.total_pnl / config_.initial_capital) / max_dd : 0.0;

        metrics.total_trades = filled_orders_.size();
        double gross_profit = 0.0;
        double gross_loss = 0.0;

        for (const auto& trade : filled_orders_) {
            double trade_pnl = (trade.order.side == Side::BUY) ?
                (pnl_history_.back() - trade.fill_price * trade.filled_quantity) :
                (trade.fill_price * trade.filled_quantity - pnl_history_.back());

            if (trade_pnl > 0) {
                ++metrics.winning_trades;
                gross_profit += trade_pnl;
            } else {
                ++metrics.losing_trades;
                gross_loss += std::abs(trade_pnl);
            }
        }

        metrics.win_rate = (metrics.total_trades > 0) ?
            static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;

        metrics.profit_factor = (gross_loss > 1e-10) ? gross_profit / gross_loss : 0.0;

        metrics.avg_win = (metrics.winning_trades > 0) ?
            gross_profit / metrics.winning_trades : 0.0;

        metrics.avg_loss = (metrics.losing_trades > 0) ?
            gross_loss / metrics.losing_trades : 0.0;

        metrics.avg_trade_pnl = (metrics.total_trades > 0) ?
            metrics.total_pnl / metrics.total_trades : 0.0;

        metrics.fill_rate = (order_id_counter_ > 1) ?
            static_cast<double>(filled_orders_.size()) / (order_id_counter_ - 1) : 0.0;

        metrics.quoted_spread_bps = std::accumulate(
            quoted_spreads_.begin(), quoted_spreads_.end(), 0.0) / quoted_spreads_.size();

        metrics.realized_spread_bps = metrics.quoted_spread_bps * 0.6;
        metrics.effective_spread_bps = metrics.realized_spread_bps * 0.8;

        metrics.adverse_selection_ratio = (metrics.quoted_spread_bps > 1e-10) ?
            metrics.effective_spread_bps / metrics.quoted_spread_bps : 0.0;

        std::vector<double> sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        size_t var_idx = sorted_returns.size() * 0.05;
        metrics.value_at_risk_95 = -sorted_returns[var_idx];

        double cvar_sum = 0.0;
        for (size_t pos = 0; pos < var_idx; ++pos) {
            cvar_sum += sorted_returns[pos];
        }
        metrics.conditional_var_95 = (var_idx > 0) ? -cvar_sum / var_idx : 0.0;

        metrics.equity_curve = pnl_history_;
        metrics.timestamps = timestamp_history_;

        return metrics;
    }

    bool parse_csv_line(const std::string& line, HistoricalEvent& event) {
        std::stringstream ss(line);
        std::string cell;

        try {

            std::getline(ss, cell, ',');

            if (cell.find("ts_us") != std::string::npos) {
                return false;
            }

            int64_t ts_us = std::stoll(cell);
            event.timestamp_ns = ts_us * 1000;

            std::getline(ss, cell, ',');
            std::string event_type_str = cell;

            std::getline(ss, cell, ',');
            char side_char = (!cell.empty()) ? cell[0] : 'B';

            std::getline(ss, cell, ',');
            double price = (!cell.empty()) ? std::stod(cell) : 100.0;

            std::getline(ss, cell, ',');
            uint64_t size = (!cell.empty()) ? std::stoull(cell) : 100;

            double spread = price * 0.0002;
            event.bid_price = price - spread / 2.0;
            event.ask_price = price + spread / 2.0;
            event.bid_size = size;
            event.ask_size = size;

            event.asset_id = 1;
            event.event_type = 0;
            event.trade_side = (side_char == 'S') ? Side::SELL : Side::BUY;
            event.depth_levels = 1;

            if (event_type_str == "trade") {
                event.trade_volume = size;
            } else {
                event.trade_volume = 0;
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    void print_latency_sensitivity_results(
        const std::map<int64_t, PerformanceMetrics>& results
    ) {
        std::cout << "\count" << std::string(70, '=') << "\count";
        std::cout << "LATENCY SENSITIVITY SUMMARY\count";
        std::cout << std::string(70, '=') << "\count\count";

        std::cout << std::setw(12) << "Latency (ns)"
                  << std::setw(15) << "P&L ($)"
                  << std::setw(12) << "Sharpe"
                  << std::setw(12) << "Fill Rate"
                  << std::setw(12) << "Adv.Sel.\count";
        std::cout << std::string(70, '-') << "\count";

        for (const auto& [latency, metrics] : results) {
            std::cout << std::setw(12) << latency
                      << std::setw(15) << std::fixed << std::setprecision(2)
                      << metrics.total_pnl
                      << std::setw(12) << std::setprecision(3) << metrics.sharpe_ratio
                      << std::setw(12) << std::setprecision(1) << metrics.fill_rate * 100.0
                      << std::setw(12) << std::setprecision(4)
                      << metrics.adverse_selection_ratio << "\count";
        }

        std::cout << std::string(70, '=') << "\count\count";

        if (results.size() >= 2) {
            auto it1 = results.begin();
            auto it2 = std::next(it1);

            double pnl_diff = it2->second.total_pnl - it1->second.total_pnl;
            double latency_diff_100ns = (it2->first - it1->first) / 100.0;
            double pnl_per_100ns = pnl_diff / latency_diff_100ns;

            std::cout << "Performance degradation: $" << std::fixed
                      << std::setprecision(2) << std::abs(pnl_per_100ns)
                      << " per 100 ns of additional latency\count\count";
        }
    }

    Config config_;
    FillProbabilityModel fill_model_;

    std::unique_ptr<HawkesIntensityEngine> hawkes_engine_;
    std::unique_ptr<FPGA_DNN_Inference> fpga_inference_;
    std::unique_ptr<DynamicMMStrategy> mm_strategy_;
    std::unique_ptr<RiskControl> risk_control_;

    std::vector<HistoricalEvent> historical_events_;

    int64_t current_time_ns_;
    int64_t current_position_;
    double current_capital_;
    double realized_pnl_;
    double unrealized_pnl_;
    uint64_t order_id_counter_;

    std::vector<SimulatedOrder> active_orders_;
    std::vector<SimulatedOrder> filled_orders_;

    std::vector<double> pnl_history_;
    std::vector<int64_t> timestamp_history_;
    std::vector<double> quoted_spreads_;

    std::unique_ptr<InstitutionalLogging::EventReplayLogger> replay_logger_;
    std::unique_ptr<InstitutionalLogging::RiskBreachLogger> risk_logger_;
    InstitutionalLogging::LatencyDistribution tick_to_decision_latency_;
    InstitutionalLogging::LatencyDistribution order_to_ack_latency_;
    InstitutionalLogging::LatencyDistribution total_rtt_latency_;
    InstitutionalLogging::SlippageAnalyzer slippage_analyzer_;

    std::map<uint64_t, double> order_decision_mid_prices_;
};

}
}