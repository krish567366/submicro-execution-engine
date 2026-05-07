#pragma once

#include "common_types.hpp"
#include <array>
#include <cstdint>
#include <type_traits>
#include <tuple>
#include <variant>

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
    #define HFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define HFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define HFT_FORCE_INLINE [[gnu::always_inline]] inline
#else
    #define HFT_LIKELY(x)   (x)
    #define HFT_UNLIKELY(x) (x)
    #define HFT_FORCE_INLINE inline
#endif

 namespace hft {
 namespace optimized_logic {

/**
 * Compile-Time Decision Tree
 * 
 * Concept:
 * Instead of: if (A) { if (B) { return X; } else { return Y; } }
 * We define a type: Tree<ConditionA, Tree<ConditionB, ActionX, ActionY>, ...>
 * 
 * The compiler flattens this into a single block of assembly without vtables.
 * This acts like an "FPGA in C++" - fixed logic pipelines.
 */

// ==== Data Context ====
// The "Input Bus" for our logic gate
struct MarketContext {
    double mid_price;
    double obi; // Order Book Imbalance
    double volatility;
    int32_t position;
    double pnl;
};

// ==== Actions ====
enum class ActionType : uint8_t {
    NONE = 0,
    BUY = 1,
    SELL = 2,
    CLOSE = 3,
    PANIC = 4
};

// Leaf Node: Action
template<ActionType TAction>
struct ActionNode {
    HFT_FORCE_INLINE static ActionType evaluate(const MarketContext&) {
        return TAction;
    }
};

// ==== Conditions ====

// A condition is a struct providing static bool check(Context)

// Example: "Is Volatility High?"
template<long ThresX1000> // Template params must be int/long, so we scale by 1000
struct IsHighVol {
    HFT_FORCE_INLINE static bool check(const MarketContext& ctx) {
        return ctx.volatility > (double)ThresX1000 / 1000.0;
    }
};

// Example: "Is OBI Positive?"
template<long ThresX1000>
struct IsOBIBullish {
    HFT_FORCE_INLINE static bool check(const MarketContext& ctx) {
        return ctx.obi > (double)ThresX1000 / 1000.0;
    }
};

// Example: "Is Position Maxed?"
template<int MaxPos>
struct IsPositionSafe {
    HFT_FORCE_INLINE static bool check(const MarketContext& ctx) {
        return std::abs(ctx.position) < MaxPos;
    }
};

// ==== Decision Node ====
// If Condition::check is true, goto Left, else goto Right
template<typename Condition, typename LeftBranch, typename RightBranch>
struct DecisionNode {
    HFT_FORCE_INLINE static ActionType evaluate(const MarketContext& ctx) {
        if (HFT_LIKELY(Condition::check(ctx))) {
            return LeftBranch::evaluate(ctx);
        } else {
            return RightBranch::evaluate(ctx);
        }
    }
};

/**
 * Strategy Compiler
 * 
 * Usage:
 * using MyStrategy = DecisionNode<
 *      IsPositionSafe<100>,
 *      DecisionNode<
 *          IsHighVol<500>, // Vol > 0.5
 *          ActionNode<ActionType::NONE>, // Too volatile
 *          DecisionNode< // Normal Vol
 *              IsOBIBullish<200>, // OBI > 0.2
 *              ActionNode<ActionType::BUY>,
 *              ActionNode<ActionType::SELL>
 *          >
 *      >,
 *      ActionNode<ActionType::CLOSE> // Unsafe position
 * >;
 */
template<typename RootNode>
class CompiledStrategy {
public:
    // This function compiles checks for invalid conditions?
    // C++ templates ensure types are valid at compile time.
    
    HFT_FORCE_INLINE static ActionType execute(const MarketContext& ctx) {
        // Pure inline evaluation
        return RootNode::evaluate(ctx);
    }
};

// ==== Pre-Built Logic Blocks ====

// Momentum Logic: If OBI > X buy, if OBI < -X sell
template<long Threshold>
using MomentumBlock = DecisionNode<
    IsOBIBullish<Threshold>,
    ActionNode<ActionType::BUY>,
    DecisionNode<
        IsOBIBullish<-Threshold>,
        ActionNode<ActionType::NONE>, // Neutral
        ActionNode<ActionType::SELL>
    >
>;

} // namespace optimized_logic

            template<size_t MaxLevels = 1000>
        class FlatArrayOrderBook {
    public:
    struct PriceLevel {
    double price;
     double quantity;
     uint32_t order_count;
     bool active;
     };

    FlatArrayOrderBook() : num_bids_(0), num_asks_(0) {

        static_assert(sizeof(PriceLevel) == 24, "Ensure tight packing");
    }

    inline void update_bid(size_t level_idx, double price, double quantity) {
        if (HFT_LIKELY(level_idx < num_bids_)) {
            bids_[level_idx].price = price;
            bids_[level_idx].quantity = quantity;
            bids_[level_idx].active = (quantity > 0.0);
        } else if (HFT_UNLIKELY(num_bids_ < MaxLevels)) {
            bids_[num_bids_].price = price;
            bids_[num_bids_].quantity = quantity;
            bids_[num_bids_].order_count = 1;
            bids_[num_bids_].active = true;
            num_bids_++;
        }
    }

    inline double get_best_bid() const {
        if (HFT_LIKELY(num_bids_ > 0)) {
            return bids_[0].price;
        }
        return 0.0;
    }

 inline void get_top_bids(size_t length, double* prices, double* quantities) const {
 size_t count = (length < num_bids_) ? length : num_bids_;

 for (size_t idx = 0; idx < count; idx++) {
 prices[idx] = bids_[idx].price;
 quantities[idx] = bids_[idx].quantity;
 }
 }

 private:

 alignas(64) std::array<PriceLevel, MaxLevels> bids_;
 alignas(64) std::array<PriceLevel, MaxLevels> asks_;
 size_t num_bids_;
size_t num_asks_;
};

    namespace compile_time_math {

        constexpr double BASE_RISK_THRESHOLD = 100.0;
        constexpr double VOLATILITY_MULTIPLIER = 1.5;
    constexpr double POSITION_ADJUSTMENT = 0.02;

     constexpr double COMPUTED_THRESHOLD =
     BASE_RISK_THRESHOLD * VOLATILITY_MULTIPLIER * POSITION_ADJUSTMENT;

    constexpr double pow(double base, int exp) {
        return (exp == 0) ? 1.0 :
        (exp == 1) ? base :
        (exp < 0) ? 1.0 / pow(base, -exp) :
            base * pow(base, exp - 1);
            }

    constexpr uint64_t factorial(int length) {
        return (length <= 1) ? 1 : length * factorial(length - 1);
    }

    inline bool check_risk_optimized(double price, double position) {
        if (HFT_LIKELY(price > COMPUTED_THRESHOLD)) {
            return true;
        }
        return false;
    }
}

        class PGOInstrumentation {
        public:

            static inline void mark_hot_path() {

    #ifdef PROFILE_GENERATE
    __builtin_assume(true);
    #endif
     }

     static inline void mark_cold_path() {
    #ifdef PROFILE_GENERATE
        __builtin_assume(false);
        #endif
        }
        };

class OptimizedTradingLoop {
public:
    OptimizedTradingLoop() : position_(0.0), daily_pnl_(0.0) {}

    inline void process_market_data(double bid, double ask, double last_price) {
        double signal_strength = calculate_signal(bid, ask, last_price);

        if (HFT_LIKELY(signal_strength > compile_time_math::COMPUTED_THRESHOLD)) {
            PGOInstrumentation::mark_hot_path();

            if (HFT_LIKELY(check_risk_inline(10.0, position_))) {
                submit_order_inline(signal_strength > 0 ? 1 : -1, last_price);
            } else {
                handle_risk_rejection();
            }
        } else {
            PGOInstrumentation::mark_cold_path();
            update_passive_stats();
        }
    }

private:
    double position_;
    double daily_pnl_;

    inline double calculate_signal(double bid, double ask, double last_price) {
        return (bid - ask) / last_price;
    }

    inline bool check_risk_inline(double size, double pos) {
        return (size <= 100.0 && pos <= 1000.0);
    }

    inline void submit_order_inline(int side, double price) {
        position_ += side * 10.0;
    }

    inline void handle_risk_rejection() {
        // Risk rejection handling
    }

    inline void update_passive_stats() {
        // Passive stats update
    }
};

}
}