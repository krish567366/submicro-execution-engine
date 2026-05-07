#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "compile_time_dispatch.hpp"
#include <limits>

namespace hft {
namespace compile_time {

class CompileTimeDispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }

    void TearDown() override {
        // Cleanup
    }
};

// Test compile-time math functions
TEST_F(CompileTimeDispatchTest, MathSqrt) {
    constexpr double sqrt4 = math::sqrt(4.0);
    constexpr double sqrt9 = math::sqrt(9.0);
    constexpr double sqrt2 = math::sqrt(2.0);

    EXPECT_DOUBLE_EQ(sqrt4, 2.0);
    EXPECT_DOUBLE_EQ(sqrt9, 3.0);
    EXPECT_NEAR(sqrt2, 1.414213562, 1e-6);

    // Test edge cases
    constexpr double sqrt0 = math::sqrt(0.0);
    constexpr double sqrt1 = math::sqrt(1.0);

    EXPECT_DOUBLE_EQ(sqrt0, 0.0);
    EXPECT_DOUBLE_EQ(sqrt1, 1.0);
}

TEST_F(CompileTimeDispatchTest, MathPow) {
    constexpr double pow2_3 = math::pow(2.0, 3);
    constexpr double pow3_2 = math::pow(3.0, 2);
    constexpr double pow5_0 = math::pow(5.0, 0);
    constexpr double pow2_neg1 = math::pow(2.0, -1);

    EXPECT_DOUBLE_EQ(pow2_3, 8.0);
    EXPECT_DOUBLE_EQ(pow3_2, 9.0);
    EXPECT_DOUBLE_EQ(pow5_0, 1.0);
    EXPECT_DOUBLE_EQ(pow2_neg1, 0.5);
}

TEST_F(CompileTimeDispatchTest, MathAbs) {
    constexpr double abs_pos = math::abs(5.5);
    constexpr double abs_neg = math::abs(-3.2);
    constexpr double abs_zero = math::abs(0.0);

    EXPECT_DOUBLE_EQ(abs_pos, 5.5);
    EXPECT_DOUBLE_EQ(abs_neg, 3.2);
    EXPECT_DOUBLE_EQ(abs_zero, 0.0);
}

TEST_F(CompileTimeDispatchTest, MathMinMax) {
    constexpr double min_result = math::minimum(5.0, 3.0);
    constexpr double max_result = math::maximum(5.0, 3.0);

    EXPECT_DOUBLE_EQ(min_result, 3.0);
    EXPECT_DOUBLE_EQ(max_result, 5.0);
}

TEST_F(CompileTimeDispatchTest, MathClamp) {
    constexpr double clamp_low = math::clamp(1.0, 5.0, 10.0);
    constexpr double clamp_mid = math::clamp(7.0, 5.0, 10.0);
    constexpr double clamp_high = math::clamp(15.0, 5.0, 10.0);

    EXPECT_DOUBLE_EQ(clamp_low, 5.0);
    EXPECT_DOUBLE_EQ(clamp_mid, 7.0);
    EXPECT_DOUBLE_EQ(clamp_high, 10.0);
}

// Test risk parameters specializations
TEST_F(CompileTimeDispatchTest, RiskParametersStrict) {
    EXPECT_DOUBLE_EQ(RiskParameters<StrictRiskPolicy>::MAX_POSITION_SIZE, 100.0);
    EXPECT_DOUBLE_EQ(RiskParameters<StrictRiskPolicy>::MAX_ORDER_SIZE, 10.0);
    EXPECT_DOUBLE_EQ(RiskParameters<StrictRiskPolicy>::MAX_DAILY_LOSS, 10000.0);
    EXPECT_DOUBLE_EQ(RiskParameters<StrictRiskPolicy>::MIN_SPREAD_BPS, 5.0);
    EXPECT_FALSE(RiskParameters<StrictRiskPolicy>::ALLOW_NAKED_SHORTS);
}

TEST_F(CompileTimeDispatchTest, RiskParametersModerate) {
    EXPECT_DOUBLE_EQ(RiskParameters<ModerateRiskPolicy>::MAX_POSITION_SIZE, 500.0);
    EXPECT_DOUBLE_EQ(RiskParameters<ModerateRiskPolicy>::MAX_ORDER_SIZE, 50.0);
    EXPECT_DOUBLE_EQ(RiskParameters<ModerateRiskPolicy>::MAX_DAILY_LOSS, 50000.0);
    EXPECT_DOUBLE_EQ(RiskParameters<ModerateRiskPolicy>::MIN_SPREAD_BPS, 2.0);
    EXPECT_FALSE(RiskParameters<ModerateRiskPolicy>::ALLOW_NAKED_SHORTS);
}

TEST_F(CompileTimeDispatchTest, RiskParametersAggressive) {
    EXPECT_DOUBLE_EQ(RiskParameters<AggressiveRiskPolicy>::MAX_POSITION_SIZE, 1000.0);
    EXPECT_DOUBLE_EQ(RiskParameters<AggressiveRiskPolicy>::MAX_ORDER_SIZE, 100.0);
    EXPECT_DOUBLE_EQ(RiskParameters<AggressiveRiskPolicy>::MAX_DAILY_LOSS, 100000.0);
    EXPECT_DOUBLE_EQ(RiskParameters<AggressiveRiskPolicy>::MIN_SPREAD_BPS, 1.0);
    EXPECT_TRUE(RiskParameters<AggressiveRiskPolicy>::ALLOW_NAKED_SHORTS);
}

// Test compile-time risk checker
TEST_F(CompileTimeDispatchTest, CompileTimeRiskCheckerStrict) {
    using RiskChecker = CompileTimeRiskChecker<StrictRiskPolicy>;

    // Test passing case
    bool result = RiskChecker::check_order(50.0, 5.0, Side::BUY, -5000.0, 6.0);
    EXPECT_TRUE(result);

    // Test position limit violation
    result = RiskChecker::check_order(90.0, 15.0, Side::BUY, -5000.0, 6.0);
    EXPECT_FALSE(result);

    // Test order size violation
    result = RiskChecker::check_order(50.0, 15.0, Side::BUY, -5000.0, 6.0);
    EXPECT_FALSE(result);

    // Test daily loss violation
    result = RiskChecker::check_order(50.0, 5.0, Side::BUY, -15000.0, 6.0);
    EXPECT_FALSE(result);

    // Test spread violation
    result = RiskChecker::check_order(50.0, 5.0, Side::BUY, -5000.0, 3.0);
    EXPECT_FALSE(result);

    // Test naked short violation (strict policy)
    result = RiskChecker::check_order(0.0, 5.0, Side::SELL, -5000.0, 6.0);
    EXPECT_FALSE(result);
}

TEST_F(CompileTimeDispatchTest, CompileTimeRiskCheckerAggressive) {
    using RiskChecker = CompileTimeRiskChecker<AggressiveRiskPolicy>;

    // Test passing case
    bool result = RiskChecker::check_order(500.0, 50.0, Side::BUY, -50000.0, 2.0);
    EXPECT_TRUE(result);

    // Test naked short allowed (aggressive policy)
    result = RiskChecker::check_order(0.0, 50.0, Side::SELL, -50000.0, 2.0);
    EXPECT_TRUE(result);

    // Test position limit violation
    result = RiskChecker::check_order(950.0, 60.0, Side::BUY, -50000.0, 2.0);
    EXPECT_FALSE(result);
}

TEST_F(CompileTimeDispatchTest, CompileTimeRiskCheckerIndividualChecks) {
    using RiskChecker = CompileTimeRiskChecker<ModerateRiskPolicy>;

    // Test individual check functions
    EXPECT_TRUE(RiskChecker::check_position_limit(300.0));
    EXPECT_FALSE(RiskChecker::check_position_limit(600.0));

    EXPECT_TRUE(RiskChecker::check_order_size(25.0));
    EXPECT_FALSE(RiskChecker::check_order_size(75.0));

    EXPECT_TRUE(RiskChecker::check_daily_loss(-25000.0));
    EXPECT_FALSE(RiskChecker::check_daily_loss(-75000.0));

    EXPECT_TRUE(RiskChecker::check_min_spread(3.0));
    EXPECT_FALSE(RiskChecker::check_min_spread(1.0));
}

// Test strategy parameters specializations
TEST_F(CompileTimeDispatchTest, StrategyParametersAvellanedaStoikov) {
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::RISK_AVERSION, 0.1);
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::VOLATILITY, 0.02);
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::TIME_HORIZON, 1.0);
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::INVENTORY_PENALTY, 0.01);
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::MIN_SPREAD, 0.0001);
    EXPECT_DOUBLE_EQ(StrategyParameters<AvellanedaStoikovStrategy>::MAX_SPREAD, 0.01);
}

TEST_F(CompileTimeDispatchTest, StrategyParametersSimpleMarketMaking) {
    EXPECT_DOUBLE_EQ(StrategyParameters<SimpleMarketMakingStrategy>::BASE_SPREAD_BPS, 5.0);
    EXPECT_DOUBLE_EQ(StrategyParameters<SimpleMarketMakingStrategy>::INVENTORY_SKEW_FACTOR, 0.1);
    EXPECT_DOUBLE_EQ(StrategyParameters<SimpleMarketMakingStrategy>::MIN_SPREAD_BPS, 2.0);
    EXPECT_DOUBLE_EQ(StrategyParameters<SimpleMarketMakingStrategy>::MAX_SPREAD_BPS, 20.0);
}

// Test compile-time strategy engine
TEST_F(CompileTimeDispatchTest, CompileTimeStrategyEngineAvellanedaStoikov) {
    using StrategyEngine = CompileTimeStrategyEngine<AvellanedaStoikovStrategy>;

    auto quote = StrategyEngine::compute_quotes(100.0, 10.0, 0.02, 1.0, 1.0);

    // Verify quote structure
    EXPECT_LT(quote.bid_price, quote.ask_price);  // Bid should be lower than ask
    EXPECT_GT(quote.bid_price, 0.0);
    EXPECT_GT(quote.ask_price, 0.0);
    EXPECT_DOUBLE_EQ(quote.bid_size, 10.0);
    EXPECT_DOUBLE_EQ(quote.ask_size, 10.0);

    // With positive inventory, both prices should be below mid due to reservation price adjustment
    EXPECT_LT(quote.bid_price, 100.0);
    EXPECT_LT(quote.ask_price, 100.0);
}

TEST_F(CompileTimeDispatchTest, CompileTimeStrategyEngineSimpleMarketMaking) {
    using StrategyEngine = CompileTimeStrategyEngine<SimpleMarketMakingStrategy>;

    auto quote = StrategyEngine::compute_quotes(100.0, 5.0, 0.02, 1.0, 1.0);

    // Verify quote structure
    EXPECT_LT(quote.bid_price, quote.ask_price);
    EXPECT_GT(quote.bid_price, 0.0);
    EXPECT_GT(quote.ask_price, 0.0);
    EXPECT_DOUBLE_EQ(quote.bid_size, 10.0);
    EXPECT_DOUBLE_EQ(quote.ask_size, 10.0);

    // Check spread is reasonable (around 5bps = 0.05)
    double spread = quote.ask_price - quote.bid_price;
    EXPECT_GT(spread, 0.0);
    EXPECT_LT(spread, 2.0);  // Less than 2% spread
}

TEST_F(CompileTimeDispatchTest, CompileTimeStrategyEngineRiskMultiplier) {
    using StrategyEngine = CompileTimeStrategyEngine<AvellanedaStoikovStrategy>;

    // Use higher volatility to avoid clamping
    auto quote_normal = StrategyEngine::compute_quotes(100.0, 0.0, 0.1, 1.0, 1.0);
    auto quote_aggressive = StrategyEngine::compute_quotes(100.0, 0.0, 0.1, 1.0, 2.0);

    // Aggressive multiplier should increase spread
    double spread_normal = quote_normal.ask_price - quote_normal.bid_price;
    double spread_aggressive = quote_aggressive.ask_price - quote_aggressive.bid_price;

    EXPECT_GT(spread_aggressive, spread_normal);
}

TEST_F(CompileTimeDispatchTest, CompileTimeStrategyEngineInventoryEffect) {
    using StrategyEngine = CompileTimeStrategyEngine<AvellanedaStoikovStrategy>;

    auto quote_long = StrategyEngine::compute_quotes(100.0, 20.0, 0.02, 1.0, 1.0);
    auto quote_short = StrategyEngine::compute_quotes(100.0, -20.0, 0.02, 1.0, 1.0);

    // Long inventory should push prices down (lower bid, lower ask)
    // Short inventory should push prices up (higher bid, higher ask)
    EXPECT_LT(quote_long.bid_price, quote_short.bid_price);
    EXPECT_LT(quote_long.ask_price, quote_short.ask_price);
}

// Test type aliases
TEST_F(CompileTimeDispatchTest, TypeAliasesDefault) {
    // Test that type aliases work
    DefaultStrategyEngine::Quote quote = DefaultStrategyEngine::compute_quotes(100.0, 0.0, 0.02, 1.0, 1.0);
    bool risk_ok = DefaultRiskChecker::check_order(100.0, 10.0, Side::BUY, -10000.0, 3.0);

    EXPECT_LT(quote.bid_price, quote.ask_price);
    EXPECT_TRUE(risk_ok);
}

TEST_F(CompileTimeDispatchTest, TypeAliasesAggressive) {
    AggressiveStrategyEngine::Quote quote = AggressiveStrategyEngine::compute_quotes(100.0, 0.0, 0.02, 1.0, 1.0);
    bool risk_ok = AggressiveRiskChecker::check_order(500.0, 50.0, Side::BUY, -50000.0, 1.5);

    EXPECT_LT(quote.bid_price, quote.ask_price);
    EXPECT_TRUE(risk_ok);
}

TEST_F(CompileTimeDispatchTest, TypeAliasesConservative) {
    ConservativeStrategyEngine::Quote quote = ConservativeStrategyEngine::compute_quotes(100.0, 0.0, 0.02, 1.0, 1.0);
    bool risk_ok = ConservativeRiskChecker::check_order(50.0, 5.0, Side::BUY, -5000.0, 6.0);

    EXPECT_LT(quote.bid_price, quote.ask_price);
    EXPECT_TRUE(risk_ok);
}

// Test performance characteristics
TEST_F(CompileTimeDispatchTest, PerformanceStrategyComputation) {
    using StrategyEngine = CompileTimeStrategyEngine<AvellanedaStoikovStrategy>;

    auto start = std::chrono::high_resolution_clock::now();

    // Perform many strategy computations
    for (int i = 0; i < 100000; ++i) {
        auto quote = StrategyEngine::compute_quotes(100.0, 10.0, 0.02, 1.0, 1.0);
        (void)quote;  // Avoid optimization away
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Should be very fast (< 200ns per computation on average)
    double avg_ns = duration.count() / 100000.0;
    EXPECT_LT(avg_ns, 200.0);
}

TEST_F(CompileTimeDispatchTest, PerformanceRiskChecking) {
    using RiskChecker = CompileTimeRiskChecker<ModerateRiskPolicy>;

    auto start = std::chrono::high_resolution_clock::now();

    // Perform many risk checks
    for (int i = 0; i < 100000; ++i) {
        bool result = RiskChecker::check_order(100.0, 10.0, Side::BUY, -10000.0, 3.0);
        (void)result;  // Avoid optimization away
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Should be very fast (< 100ns per check on average)
    double avg_ns = duration.count() / 100000.0;
    EXPECT_LT(avg_ns, 100.0);
}

} // namespace compile_time
} // namespace hft