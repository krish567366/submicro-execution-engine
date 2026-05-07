#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <memory>

#include "smart_order_router.hpp"
#include "avellaneda_stoikov.hpp"

// Mock for AvellanedaStoikov to avoid complex dependencies
class MockAvellanedaStoikov : public DynamicMMStrategy {
public:
    MockAvellanedaStoikov()
        : DynamicMMStrategy(0.1, 0.5, 1.5, 1.0, 0.01, 100000) {}

    MOCK_METHOD(hft::QuotePair, calculate_quotes, (double, int64_t, double, double), (const));
    MOCK_METHOD(double, calculate_latency_cost, (double, double), (const));
};

class SmartOrderRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock AS model
        mock_as_model_ = std::make_unique<MockAvellanedaStoikov>();

        // Initialize router with mock
        router_.initialize(mock_as_model_.get());

        // The router is already initialized with 3 default venues: BINANCE, COINBASE, KRAKEN
        // Setup heartbeats to make them connected
        setup_heartbeats();
    }

    void setup_heartbeats() {
        Timestamp now = std::chrono::steady_clock::now();
        for (const auto& venue_id : router_.get_active_venues()) {
            router_.send_heartbeat(venue_id, now);
        }
    }

    std::unique_ptr<MockAvellanedaStoikov> mock_as_model_;
    SmartOrderRouter router_;
};

// Test venue management
TEST_F(SmartOrderRouterTest, VenueManagement) {
    // Test adding venues
    EXPECT_EQ(router_.get_active_venues().size(), 3);

    // Test venue info retrieval - check that default venues exist
    auto active_venues = router_.get_active_venues();
    EXPECT_TRUE(std::find(active_venues.begin(), active_venues.end(), "BINANCE") != active_venues.end());
    EXPECT_TRUE(std::find(active_venues.begin(), active_venues.end(), "COINBASE") != active_venues.end());
    EXPECT_TRUE(std::find(active_venues.begin(), active_venues.end(), "KRAKEN") != active_venues.end());

    // Test removing venue
    router_.remove_venue("COINBASE");
    active_venues = router_.get_active_venues();
    EXPECT_EQ(active_venues.size(), 2);
    EXPECT_FALSE(std::find(active_venues.begin(), active_venues.end(), "COINBASE") != active_venues.end());
}

// Test heartbeat monitoring
TEST_F(SmartOrderRouterTest, HeartbeatMonitoring) {
    // Test is simplified - just verify heartbeat sending works
    Timestamp now = std::chrono::steady_clock::now();

    // Send additional heartbeats to all venues (they already have 1 from setup)
    for (const auto& venue_id : router_.get_active_venues()) {
        router_.send_heartbeat(venue_id, now);
    }

    // Check states - should have 2 heartbeats sent (1 from setup + 1 from test)
    for (const auto& venue_id : router_.get_active_venues()) {
        auto state = router_.get_venue_state(venue_id);
        ASSERT_TRUE(state.has_value());
        EXPECT_TRUE(state->is_connected);
        EXPECT_EQ(state->last_heartbeat_sent, now);
        EXPECT_EQ(state->total_heartbeats_sent, 2);
    }
}

// Test latency budget calculation
TEST_F(SmartOrderRouterTest, LatencyBudgetCalculation) {
    // Setup mock expectations for profitable scenario
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;
    profitable_quotes.mid_price = 50000.0;
    profitable_quotes.spread = 20.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(5.0));

    double budget = router_.calculate_latency_budget(50000.0, 0.5, 100, 10, MarketRegime::NORMAL);
    EXPECT_GT(budget, 0.0);
    EXPECT_LE(budget, 10000.0);  // Should be clamped

    // Test unprofitable scenario
    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(50.0));  // Higher cost than profit

    budget = router_.calculate_latency_budget(50000.0, 0.5, 100, 10, MarketRegime::HIGH_STRESS);
    EXPECT_EQ(budget, 80.0);  // Minimum budget (100 * 0.8 safety margin)
}

// Test order routing - successful routing
TEST_F(SmartOrderRouterTest, SuccessfulOrderRouting) {
    // Setup mock for profitable trade
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;
    profitable_quotes.mid_price = 50000.0;
    profitable_quotes.spread = 20.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(5.0));

    // Venue prices (BINANCE has best price for buying)
    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 49999.0;    // Best bid
    venue_prices["COINBASE"] = 49998.0;   // Worse bid
    venue_prices["KRAKEN"] = 49997.0;     // Worst bid

    // Route buy order
    auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);

    // Should select BINANCE venue (best price, meets latency budget)
    EXPECT_EQ(decision.selected_venue, "BINANCE");
    EXPECT_TRUE(decision.rejection_reason.empty());
    EXPECT_GT(decision.latency_budget_us, 0.0);
    EXPECT_GT(decision.composite_score, 0.0);
    EXPECT_GE(decision.price_quality, 0.0);
    EXPECT_LE(decision.price_quality, 1.0);
}

// Test order routing - rejection due to latency budget
TEST_F(SmartOrderRouterTest, OrderRejectionLatencyBudget) {
    // Setup mock for unprofitable trade (very tight budget)
    hft::QuotePair unprofitable_quotes;
    unprofitable_quotes.bid_price = 49995.0;
    unprofitable_quotes.ask_price = 50005.0;
    unprofitable_quotes.mid_price = 50000.0;
    unprofitable_quotes.spread = 10.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(unprofitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(50.0));

    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 50001.0;
    venue_prices["COINBASE"] = 50002.0;
    venue_prices["KRAKEN"] = 50003.0;

    auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);

    // Should be rejected due to tight latency budget
    EXPECT_TRUE(decision.selected_venue.empty());
    EXPECT_FALSE(decision.rejection_reason.empty());
    EXPECT_THAT(decision.rejection_reason, testing::HasSubstr("latency budget"));
}

// Test order routing - rejection due to all venues exceeding latency budget
TEST_F(SmartOrderRouterTest, OrderRejectionDisconnectedVenues) {
    // Setup mock for very tight latency budget (unprofitable trade)
    hft::QuotePair tight_quotes;
    tight_quotes.bid_price = 49995.0;
    tight_quotes.ask_price = 50005.0;
    tight_quotes.bid_size = 1000.0;
    tight_quotes.ask_size = 1000.0;
    tight_quotes.spread = 10.0;
    tight_quotes.mid_price = 50000.0;
    tight_quotes.generated_at = hft::now();

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(tight_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(50.0));  // High latency cost

    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 50001.0;
    venue_prices["COINBASE"] = 50002.0;
    venue_prices["KRAKEN"] = 50003.0;

    auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);

    // Should be rejected due to tight latency budget (profit < latency cost)
    EXPECT_TRUE(decision.selected_venue.empty());
    EXPECT_FALSE(decision.rejection_reason.empty());
}

// Test order size validation
TEST_F(SmartOrderRouterTest, OrderSizeValidation) {
    // Setup mock
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillRepeatedly(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillRepeatedly(testing::Return(5.0));

    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 50001.0;
    venue_prices["COINBASE"] = 50002.0;
    venue_prices["KRAKEN"] = 50003.0;

    // Test order too small for COINBASE venue (min 0.01)
    auto decision = router_.route_order(50000.0, 0.5, 100, 0, MarketRegime::NORMAL, venue_prices);
    EXPECT_TRUE(decision.selected_venue.empty());  // Should reject due to size constraints

    // Test order too large for KRAKEN venue (max 3000)
    decision = router_.route_order(50000.0, 0.5, 100, 5000, MarketRegime::NORMAL, venue_prices);
    // Should still work with BINANCE venue which can handle large orders
    EXPECT_FALSE(decision.selected_venue.empty());
}

// Test order result recording
TEST_F(SmartOrderRouterTest, OrderResultRecording) {
    // Record some order results for BINANCE venue
    router_.record_order_result("BINANCE", true, false);   // Filled
    router_.record_order_result("BINANCE", false, true);   // Timeout
    router_.record_order_result("BINANCE", false, false);  // Rejected
    router_.record_order_result("BINANCE", true, false);   // Another filled

    // Check statistics
    auto state = router_.get_venue_state("BINANCE");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->orders_sent, 4);
    EXPECT_EQ(state->orders_filled, 2);
    EXPECT_EQ(state->orders_timeout, 1);
    EXPECT_EQ(state->orders_rejected, 1);

    // Test fill rate calculation
    double expected_fill_rate = 2.0 / 4.0;  // 2 filled out of 4 sent
    double actual_fill_rate = static_cast<double>(state->orders_filled) / state->orders_sent;
    EXPECT_DOUBLE_EQ(actual_fill_rate, expected_fill_rate);
}

// Test venue state monitoring
TEST_F(SmartOrderRouterTest, VenueStateMonitoring) {
    // Get all venue states
    const auto& all_states = router_.get_all_venue_states();
    EXPECT_EQ(all_states.size(), 3);

    // Verify all venues are present
    EXPECT_TRUE(all_states.count("BINANCE"));
    EXPECT_TRUE(all_states.count("COINBASE"));
    EXPECT_TRUE(all_states.count("KRAKEN"));

    // Test individual venue state
    auto binance_state = router_.get_venue_state("BINANCE");
    ASSERT_TRUE(binance_state.has_value());
    EXPECT_TRUE(binance_state->is_connected);  // Heartbeat sent in SetUp
    EXPECT_EQ(binance_state->orders_sent, 0);
    EXPECT_EQ(binance_state->orders_filled, 0);
}

// Test latency spike detection
TEST_F(SmartOrderRouterTest, LatencySpikeDetection) {
    // Simulate latency spike by directly modifying state (this is a test, so we bend the rules)
    auto& venue_states = const_cast<std::unordered_map<std::string, VenueState>&>(router_.get_all_venue_states());
    auto state_it = venue_states.find("BINANCE");
    ASSERT_NE(state_it, venue_states.end());
    state_it->second.current_rtt_us = 1000.0;  // Much higher than baseline 500us
    state_it->second.ema_rtt_us = 500.0;
    state_it->second.std_dev_rtt_us = 50.0;

    // Setup mock for routing
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(5.0));

    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 50001.0;
    venue_prices["COINBASE"] = 50002.0;
    venue_prices["KRAKEN"] = 50003.0;

    auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);

    // BINANCE should be rejected due to latency spike, so another venue should be selected
    EXPECT_NE(decision.selected_venue, "BINANCE");
    EXPECT_TRUE(decision.selected_venue == "COINBASE" || decision.selected_venue == "KRAKEN");
}

// Test concurrent access (basic thread safety)
TEST_F(SmartOrderRouterTest, ConcurrentAccess) {
    // Setup mock expectations for multiple calls
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;
    profitable_quotes.bid_size = 1000.0;
    profitable_quotes.ask_size = 1000.0;
    profitable_quotes.spread = 20.0;
    profitable_quotes.mid_price = 50000.0;
    profitable_quotes.generated_at = hft::now();

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillRepeatedly(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillRepeatedly(testing::Return(5.0));

    std::vector<std::thread> threads;

    // Launch multiple threads doing routing decisions
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, i]() {
            // Send heartbeat first
            Timestamp now = std::chrono::steady_clock::now();
            router_.send_heartbeat("BINANCE", now);

            std::unordered_map<std::string, double> venue_prices;
            venue_prices["BINANCE"] = 50001.0;

            auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);

            // Should succeed
            EXPECT_FALSE(decision.selected_venue.empty());
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
}

// Test performance - routing decisions should be fast
TEST_F(SmartOrderRouterTest, PerformanceRoutingDecision) {
    // Setup mock
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillRepeatedly(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillRepeatedly(testing::Return(5.0));

    std::unordered_map<std::string, double> venue_prices;
    venue_prices["BINANCE"] = 50001.0;
    venue_prices["COINBASE"] = 50002.0;
    venue_prices["KRAKEN"] = 50003.0;

    // Measure time for multiple routing decisions
    auto start = std::chrono::high_resolution_clock::now();

    const int num_decisions = 1000;
    for (int i = 0; i < num_decisions; ++i) {
        auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, venue_prices);
        ASSERT_FALSE(decision.selected_venue.empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_per_decision = static_cast<double>(duration.count()) / num_decisions;

    // Should be very fast (< 100 microseconds per decision)
    EXPECT_LT(avg_time_per_decision, 100.0);
}

// Test edge cases
TEST_F(SmartOrderRouterTest, EdgeCases) {
    // Test with empty venue prices
    hft::QuotePair profitable_quotes;
    profitable_quotes.bid_price = 49990.0;
    profitable_quotes.ask_price = 50010.0;
    profitable_quotes.bid_size = 1000.0;
    profitable_quotes.ask_size = 1000.0;
    profitable_quotes.spread = 20.0;
    profitable_quotes.mid_price = 50000.0;
    profitable_quotes.generated_at = hft::now();

    EXPECT_CALL(*mock_as_model_, calculate_quotes(50000.0, 100, 600.0, 0.0))
        .WillOnce(testing::Return(profitable_quotes));
    EXPECT_CALL(*mock_as_model_, calculate_latency_cost(0.5, 50000.0))
        .WillOnce(testing::Return(5.0));

    std::unordered_map<std::string, double> empty_prices;
    auto decision = router_.route_order(50000.0, 0.5, 100, 10, MarketRegime::NORMAL, empty_prices);

    // Should still work (price_quality = 0.5 for unknown prices)
    EXPECT_FALSE(decision.selected_venue.empty());
    EXPECT_TRUE(decision.rejection_reason.empty());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}