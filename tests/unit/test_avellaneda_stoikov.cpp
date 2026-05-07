#include "avellaneda_stoikov.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

// Unit tests for Avellaneda-Stoikov market making strategy

void test_basic_quote_calculation() {
    std::cout << "Testing basic quote calculation..." << std::endl;

    hft::DynamicMMStrategy strategy(
        0.1,    // risk_aversion
        0.2,    // volatility
        300.0,  // time_horizon
        0.5,    // order_arrival_rate
        0.01,   // tick_size
        1000    // system_latency_ns
    );

    auto quotes = strategy.calculate_quotes(100.0, 0, 300.0, 0.001);

    // Basic sanity checks
    assert(quotes.bid_price > 0.0);
    assert(quotes.ask_price > quotes.bid_price);
    assert(quotes.spread > 0.0);
    assert(quotes.mid_price == 100.0);

    std::cout << "Basic quote calculation passed" << std::endl;
}

void test_inventory_skew() {
    std::cout << "Testing inventory skew adjustment..." << std::endl;

    hft::DynamicMMStrategy strategy(
        0.1, 0.2, 300.0, 0.5, 0.01, 1000
    );

    // Test with positive inventory (should skew ask higher)
    auto quotes_positive = strategy.calculate_quotes(100.0, 500, 300.0, 0.001);

    // Test with negative inventory (should skew bid lower)
    auto quotes_negative = strategy.calculate_quotes(100.0, -500, 300.0, 0.001);

    // Test with zero inventory (should be symmetric)
    auto quotes_zero = strategy.calculate_quotes(100.0, 0, 300.0, 0.001);

    // Positive inventory should have higher spread due to inventory penalty
    assert(quotes_positive.spread >= quotes_zero.spread);

    // Negative inventory should also have higher spread
    assert(quotes_negative.spread >= quotes_zero.spread);

    std::cout << "Inventory skew adjustment passed" << std::endl;
}

void test_latency_cost() {
    std::cout << "Testing latency cost incorporation..." << std::endl;

    hft::DynamicMMStrategy strategy(
        0.1, 0.2, 300.0, 0.5, 0.01, 1000
    );

    double latency_cost = strategy.calculate_latency_cost(0.2, 100.0);

    // Latency cost should be positive
    assert(latency_cost > 0.0);

    // Test quoting decision
    bool should_quote = strategy.should_quote(0.05, 0.02);  // spread > cost
    bool should_not_quote = strategy.should_quote(0.01, 0.02);  // spread < cost

    assert(should_quote == true);
    assert(should_not_quote == false);

    std::cout << "Latency cost incorporation passed" << std::endl;
}

void test_parameter_updates() {
    std::cout << "Testing parameter updates..." << std::endl;

    hft::DynamicMMStrategy strategy(
        0.1, 0.2, 300.0, 0.5, 0.01, 1000
    );

    // Test risk aversion update
    strategy.set_risk_aversion(0.2);
    assert(std::abs(strategy.get_risk_aversion() - 0.2) < 1e-6);

    // Test volatility update
    strategy.set_volatility(0.3);
    assert(std::abs(strategy.get_volatility() - 0.3) < 1e-6);

    std::cout << "Parameter updates passed" << std::endl;
}

void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;

    hft::DynamicMMStrategy strategy(
        0.1, 0.2, 300.0, 0.5, 0.01, 1000
    );

    // Test with zero/negative time remaining
    auto quotes_expired = strategy.calculate_quotes(100.0, 0, 0.0, 0.001);
    assert(quotes_expired.bid_price == 0.0);
    assert(quotes_expired.ask_price == 0.0);

    // Test with zero/negative price
    auto quotes_invalid = strategy.calculate_quotes(0.0, 0, 300.0, 0.001);
    assert(quotes_invalid.bid_price == 0.0);
    assert(quotes_invalid.ask_price == 0.0);

    std::cout << "Edge cases passed" << std::endl;
}

int main() {
    std::cout << "Running Avellaneda-Stoikov Unit Tests" << std::endl;
    std::cout << "====================================" << std::endl;

    try {
        test_basic_quote_calculation();
        test_inventory_skew();
        test_latency_cost();
        test_parameter_updates();
        test_edge_cases();

        std::cout << std::endl;
        std::cout << " All tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << " Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << " Test failed with unknown exception" << std::endl;
        return 1;
    }
}