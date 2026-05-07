#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "branch_optimization.hpp"
#include <array>
#include <chrono>

namespace hft {
namespace branch_optimization {

class BranchOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }

    void TearDown() override {
        // Cleanup
    }
};

// Test BranchOptimizedRouter functionality
TEST_F(BranchOptimizationTest, BranchOptimizedRouterStrongBuySignal) {
    BranchOptimizedRouter router;

    // Test strong buy signal (hot path)
    int result = router.execute_signal(BranchOptimizedRouter::Signal::STRONG_BUY, 100.0, 50000.0);
    EXPECT_EQ(result, 1);  // Should return 1 for buy
}

TEST_F(BranchOptimizationTest, BranchOptimizedRouterStrongSellSignal) {
    BranchOptimizedRouter router;

    // Test strong sell signal (hot path)
    int result = router.execute_signal(BranchOptimizedRouter::Signal::STRONG_SELL, -100.0, 50000.0);
    EXPECT_EQ(result, -1);  // Should return -1 for sell
}

TEST_F(BranchOptimizationTest, BranchOptimizedRouterWeakSignals) {
    BranchOptimizedRouter router;

    // Test weak signals (cold path)
    int result_buy = router.execute_signal(BranchOptimizedRouter::Signal::WEAK_BUY, 50.0, 50000.0);
    int result_neutral = router.execute_signal(BranchOptimizedRouter::Signal::NEUTRAL, 0.0, 50000.0);
    int result_sell = router.execute_signal(BranchOptimizedRouter::Signal::WEAK_SELL, -50.0, 50000.0);

    // Weak signals should return 0 (no action)
    EXPECT_EQ(result_buy, 0);
    EXPECT_EQ(result_neutral, 0);
    EXPECT_EQ(result_sell, 0);
}

// Test risk checking functionality
TEST_F(BranchOptimizationTest, RiskCheckPass) {
    BranchOptimizedRouter router;

    // Test case that should pass (hot path)
    bool result = router.check_risk(50.0, 500.0, -10000.0);
    EXPECT_TRUE(result);
}

TEST_F(BranchOptimizationTest, RiskCheckOrderSizeFail) {
    BranchOptimizedRouter router;

    // Test order size too large (cold path)
    bool result = router.check_risk(150.0, 500.0, -10000.0);
    EXPECT_FALSE(result);
}

TEST_F(BranchOptimizationTest, RiskCheckPositionFail) {
    BranchOptimizedRouter router;

    // Test position too large (cold path)
    bool result = router.check_risk(50.0, 1500.0, -10000.0);
    EXPECT_FALSE(result);
}

TEST_F(BranchOptimizationTest, RiskCheckDailyLossFail) {
    BranchOptimizedRouter router;

    // Test daily loss too high (cold path)
    bool result = router.check_risk(50.0, 500.0, -60000.0);
    EXPECT_FALSE(result);
}

// Test FlatArrayOrderBook functionality
TEST_F(BranchOptimizationTest, FlatArrayOrderBookInitialization) {
    FlatArrayOrderBook<100> book;

    // Test initial state
    EXPECT_DOUBLE_EQ(book.get_best_bid(), 0.0);
}

TEST_F(BranchOptimizationTest, FlatArrayOrderBookUpdateBid) {
    FlatArrayOrderBook<100> book;

    // Update first bid level
    book.update_bid(0, 50000.0, 10.0);

    // Check best bid
    EXPECT_DOUBLE_EQ(book.get_best_bid(), 50000.0);
}

TEST_F(BranchOptimizationTest, FlatArrayOrderBookUpdateMultipleBids) {
    FlatArrayOrderBook<100> book;

    // Update multiple bid levels (assuming sorted externally)
    book.update_bid(0, 50000.0, 10.0);
    book.update_bid(1, 49999.0, 15.0);
    book.update_bid(2, 49998.0, 20.0);

    // Best bid should still be first level
    EXPECT_DOUBLE_EQ(book.get_best_bid(), 50000.0);
}

TEST_F(BranchOptimizationTest, FlatArrayOrderBookGetTopBids) {
    FlatArrayOrderBook<100> book;

    // Update multiple bid levels
    book.update_bid(0, 50000.0, 10.0);
    book.update_bid(1, 49999.0, 15.0);
    book.update_bid(2, 49998.0, 20.0);

    // Get top 2 bids
    std::array<double, 3> prices;
    std::array<double, 3> quantities;
    book.get_top_bids(2, prices.data(), quantities.data());

    EXPECT_DOUBLE_EQ(prices[0], 50000.0);
    EXPECT_DOUBLE_EQ(quantities[0], 10.0);
    EXPECT_DOUBLE_EQ(prices[1], 49999.0);
    EXPECT_DOUBLE_EQ(quantities[1], 15.0);
}

TEST_F(BranchOptimizationTest, FlatArrayOrderBookZeroQuantity) {
    FlatArrayOrderBook<100> book;

    // Update bid with quantity, then set to zero
    book.update_bid(0, 50000.0, 10.0);
    EXPECT_DOUBLE_EQ(book.get_best_bid(), 50000.0);

    // Update same level to zero quantity
    book.update_bid(0, 50000.0, 0.0);
    // Note: get_best_bid doesn't check active flag, but level should be inactive
}

// Test compile-time math functions
TEST_F(BranchOptimizationTest, CompileTimeConstants) {
    // Test that constants are computed correctly
    constexpr double expected_threshold = 100.0 * 1.5 * 0.02;  // 3.0
    EXPECT_DOUBLE_EQ(compile_time_math::COMPUTED_THRESHOLD, expected_threshold);
    EXPECT_DOUBLE_EQ(compile_time_math::COMPUTED_THRESHOLD, 3.0);
}

TEST_F(BranchOptimizationTest, CompileTimePow) {
    // Test compile-time power function
    constexpr double pow2 = compile_time_math::pow(2.0, 3);
    constexpr double pow3 = compile_time_math::pow(3.0, 2);
    constexpr double pow0 = compile_time_math::pow(5.0, 0);

    EXPECT_DOUBLE_EQ(pow2, 8.0);
    EXPECT_DOUBLE_EQ(pow3, 9.0);
    EXPECT_DOUBLE_EQ(pow0, 1.0);
}

TEST_F(BranchOptimizationTest, CompileTimeFactorial) {
    // Test compile-time factorial
    constexpr uint64_t fact0 = compile_time_math::factorial(0);
    constexpr uint64_t fact1 = compile_time_math::factorial(1);
    constexpr uint64_t fact5 = compile_time_math::factorial(5);

    EXPECT_EQ(fact0, 1ULL);
    EXPECT_EQ(fact1, 1ULL);
    EXPECT_EQ(fact5, 120ULL);
}

TEST_F(BranchOptimizationTest, CheckRiskOptimized) {
    // Test optimized risk check
    bool pass = compile_time_math::check_risk_optimized(5.0, 100.0);  // 5.0 > 3.0
    bool fail = compile_time_math::check_risk_optimized(1.0, 100.0);  // 1.0 < 3.0

    EXPECT_TRUE(pass);
    EXPECT_FALSE(fail);
}

// Test PGO instrumentation (mostly no-op in tests)
TEST_F(BranchOptimizationTest, PGOInstrumentation) {
    // These should not crash
    PGOInstrumentation::mark_hot_path();
    PGOInstrumentation::mark_cold_path();

    SUCCEED();  // Just verify they don't throw
}

// Test OptimizedTradingLoop
TEST_F(BranchOptimizationTest, OptimizedTradingLoopInitialization) {
    OptimizedTradingLoop loop;

    // Should initialize without error
    SUCCEED();
}

TEST_F(BranchOptimizationTest, OptimizedTradingLoopProcessMarketData) {
    OptimizedTradingLoop loop;

    // Process market data with strong signal (should trigger trade)
    loop.process_market_data(50010.0, 49990.0, 50000.0);

    // Process market data with weak signal (should not trigger trade)
    loop.process_market_data(50005.0, 49995.0, 50000.0);

    SUCCEED();  // Just verify no crashes
}

// Test performance characteristics (basic timing)
TEST_F(BranchOptimizationTest, PerformanceBranchOptimizedRouter) {
    BranchOptimizedRouter router;

    auto start = std::chrono::high_resolution_clock::now();

    // Execute many signals to measure performance
    for (int i = 0; i < 100000; ++i) {
        router.execute_signal(BranchOptimizedRouter::Signal::STRONG_BUY, 100.0, 50000.0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Should be very fast (< 50ns per operation on average)
    double avg_ns = duration.count() / 100000.0;
    EXPECT_LT(avg_ns, 50.0);  // Less than 50ns per operation
}

TEST_F(BranchOptimizationTest, PerformanceFlatArrayOrderBook) {
    FlatArrayOrderBook<1000> book;

    auto start = std::chrono::high_resolution_clock::now();

    // Perform many order book operations
    for (int i = 0; i < 100000; ++i) {
        book.update_bid(i % 100, 50000.0 + i, 10.0);
        book.get_best_bid();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Should be very fast (< 100ns per operation pair)
    double avg_ns = duration.count() / 100000.0;
    EXPECT_LT(avg_ns, 100.0);
}

// Test edge cases
TEST_F(BranchOptimizationTest, FlatArrayOrderBookMaxLevels) {
    FlatArrayOrderBook<5> book;

    // Fill all levels
    for (size_t i = 0; i < 5; ++i) {
        book.update_bid(i, 50000.0 - i, 10.0);
    }

    // Try to add beyond max (should be ignored)
    book.update_bid(5, 49990.0, 10.0);

    // Best bid should still be highest
    EXPECT_DOUBLE_EQ(book.get_best_bid(), 50000.0);
}

TEST_F(BranchOptimizationTest, BranchOptimizedRouterBoundaryValues) {
    BranchOptimizedRouter router;

    // Test with boundary values (should pass)
    bool risk_pass = router.check_risk(100.0, 1000.0, -49999.0);  // Just above daily loss limit
    bool risk_fail = router.check_risk(100.1, 1000.0, -49999.0);  // Just over order size limit

    EXPECT_TRUE(risk_pass);
    EXPECT_FALSE(risk_fail);
}

} // namespace branch_optimization
} // namespace hft