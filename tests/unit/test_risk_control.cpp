#include "risk_control.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

// Unit tests for Risk Control System

void test_basic_pre_trade_checks() {
    std::cout << "Testing basic pre-trade risk checks..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Create a valid order
    hft::Order order(12345, 1, hft::Side::BUY, 100.0, 50);

    // Test valid order
    assert(risk.check_pre_trade_limits(order, 0));

    // Test position limit violation
    assert(!risk.check_pre_trade_limits(order, 980));  // Would make position 1030 > 1000

    // Test order value limit violation
    hft::Order large_order(12346, 1, hft::Side::BUY, 200.0, 300);  // Value = 60000 > 50000
    assert(!risk.check_pre_trade_limits(large_order, 0));

    std::cout << "Basic pre-trade checks passed" << std::endl;
}

void test_regime_based_limits() {
    std::cout << "Testing regime-based position limits..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Normal regime (default)
    assert(risk.get_current_regime() == hft::MarketRegime::NORMAL);
    assert(risk.get_max_position() == 1000);

    // Elevated volatility
    risk.set_regime_multiplier(0.6);
    assert(risk.get_current_regime() == hft::MarketRegime::ELEVATED_VOLATILITY);
    assert(risk.get_max_position() == 700);  // 1000 * 0.7

    // High stress
    risk.set_regime_multiplier(1.5);
    assert(risk.get_current_regime() == hft::MarketRegime::HIGH_STRESS);
    assert(risk.get_max_position() == 400);  // 1000 * 0.4

    // Extreme stress (halted)
    risk.set_regime_multiplier(2.5);
    assert(risk.get_current_regime() == hft::MarketRegime::HALTED);
    assert(risk.get_max_position() == 0);

    std::cout << "Regime-based limits passed" << std::endl;
}

void test_kill_switch() {
    std::cout << "Testing kill switch functionality..." << std::endl;

    hft::RiskControl risk(1000, 1000.0, 50000.0);

    // Initially not triggered
    assert(!risk.is_kill_switch_triggered());

    // Create order
    hft::Order order(12345, 1, hft::Side::BUY, 100.0, 50);

    // Should pass initially
    assert(risk.check_pre_trade_limits(order, 0));

    // Trigger kill switch manually
    risk.trigger_kill_switch();
    assert(risk.is_kill_switch_triggered());

    // Should fail now
    assert(!risk.check_pre_trade_limits(order, 0));

    // Reset kill switch
    risk.reset_kill_switch("EMERGENCY_RESET");
    assert(!risk.is_kill_switch_triggered());

    // Should pass again
    assert(risk.check_pre_trade_limits(order, 0));

    std::cout << "Kill switch functionality passed" << std::endl;
}

void test_pnl_and_loss_limits() {
    std::cout << "Testing P&L and loss limit enforcement..." << std::endl;

    hft::RiskControl risk(1000, 500.0, 50000.0);

    // Initially zero P&L
    assert(risk.get_total_pnl() == 0.0);

    // Update P&L positively
    risk.update_pnl(100.0);
    assert(risk.get_total_pnl() == 100.0);

    // Update P&L negatively
    risk.update_pnl(-50.0);
    assert(risk.get_total_pnl() == 50.0);

    // Large loss that triggers kill switch
    risk.update_pnl(-600.0);  // Total P&L = -550.0 < -500.0 threshold
    assert(risk.get_total_pnl() == -550.0);
    assert(risk.is_kill_switch_triggered());

    std::cout << "P&L and loss limits passed" << std::endl;
}

void test_position_tracking() {
    std::cout << "Testing position tracking..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Initially zero position
    assert(risk.get_current_position() == 0);

    // Buy 100 units
    risk.update_position(hft::Side::BUY, 100);
    assert(risk.get_current_position() == 100);

    // Sell 50 units
    risk.update_position(hft::Side::SELL, 50);
    assert(risk.get_current_position() == 50);

    // Sell 200 units (net short position)
    risk.update_position(hft::Side::SELL, 200);
    assert(risk.get_current_position() == -150);

    std::cout << "Position tracking passed" << std::endl;
}

void test_daily_trade_limits() {
    std::cout << "Testing daily trade count limits..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Initially zero trades
    assert(risk.get_daily_trade_count() == 0);

    // Increment trade count
    for (int i = 1; i <= 5; ++i) {
        risk.increment_trade_count();
        assert(risk.get_daily_trade_count() == i);
    }

    // Reset daily counters
    risk.reset_daily_counters();
    assert(risk.get_daily_trade_count() == 0);
    assert(risk.get_total_pnl() == 0.0);

    std::cout << "Daily trade limits passed" << std::endl;
}

void test_safe_quote_size() {
    std::cout << "Testing safe quote size calculation..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Normal conditions
    double safe_size = risk.get_safe_quote_size(0, 100.0);
    assert(safe_size == 100.0);  // Full capacity available

    // Partial capacity used
    safe_size = risk.get_safe_quote_size(500, 100.0);
    assert(safe_size == 50.0);  // Half capacity available

    // At limit
    safe_size = risk.get_safe_quote_size(1000, 100.0);
    assert(safe_size == 0.0);  // No capacity available

    // Over limit (shouldn't happen in practice)
    safe_size = risk.get_safe_quote_size(1200, 100.0);
    assert(safe_size == 0.0);

    std::cout << "Safe quote size calculation passed" << std::endl;
}

void test_unwind_recommendations() {
    std::cout << "Testing position unwind recommendations..." << std::endl;

    hft::RiskControl risk(1000, 10000.0, 50000.0);

    // Normal position - no unwind needed
    int64_t unwind = risk.get_unwind_recommendation(500);
    assert(unwind == 0);

    // High position - unwind recommended
    unwind = risk.get_unwind_recommendation(900);  // 90% of limit
    assert(unwind == 400);  // Unwind 400 units

    // Negative position - unwind recommended
    unwind = risk.get_unwind_recommendation(-850);  // -85% of limit
    assert(unwind == -350);  // Unwind 350 units (buy back)

    std::cout << "Position unwind recommendations passed" << std::endl;
}

void test_concurrent_access() {
    std::cout << "Testing concurrent access to risk control..." << std::endl;

    hft::RiskControl risk(10000, 100000.0, 500000.0);

    const int num_threads = 4;
    const int updates_per_thread = 1000;

    std::vector<std::thread> threads;

    // Multiple threads updating P&L concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&risk, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; ++i) {
                risk.update_pnl(1.0);
                risk.update_position(hft::Side::BUY, 1);
                risk.increment_trade_count();
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify final state
    assert(risk.get_total_pnl() == num_threads * updates_per_thread);
    assert(risk.get_current_position() == num_threads * updates_per_thread);
    assert(risk.get_daily_trade_count() == num_threads * updates_per_thread);

    std::cout << "Concurrent access passed" << std::endl;
}

int main() {
    std::cout << "Running Risk Control Unit Tests" << std::endl;
    std::cout << "===============================" << std::endl;

    try {
        test_basic_pre_trade_checks();
        test_regime_based_limits();
        test_kill_switch();
        test_pnl_and_loss_limits();
        test_position_tracking();
        test_daily_trade_limits();
        test_safe_quote_size();
        test_unwind_recommendations();
        test_concurrent_access();

        std::cout << std::endl;
        std::cout << "All risk control tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}