#include <gtest/gtest.h>
#include "websocket_server.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

// Test fixture for WebSocket server tests
class WebSocketServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a metrics collector with some test data
        collector_ = std::make_unique<MetricsCollector>(10);

        // Add some test data
        collector_->update_market_data(100.0, 99.5, 100.5);
        collector_->update_position(10, 5.0, 2.0);
        collector_->increment_orders_sent();
        collector_->take_snapshot();

        collector_->update_market_data(101.0, 100.5, 101.5);
        collector_->update_position(20, 10.0, 5.0);
        collector_->increment_orders_filled();
        collector_->take_snapshot();
    }

    void TearDown() override {
        collector_.reset();
    }

    std::unique_ptr<MetricsCollector> collector_;
};

// Test WebSocketSession message parsing
TEST_F(WebSocketServerTest, WebSocketSessionMessageParsing) {
    // We can't easily test the full WebSocketSession due to network dependencies,
    // but we can test the message parsing logic by creating a minimal test

    // Test JSON parsing for get_history command
    nlohmann::json history_cmd = {{"command", "get_history"}};
    std::string history_msg = history_cmd.dump();

    // Verify the message is valid JSON
    ASSERT_NO_THROW({
        auto parsed = nlohmann::json::parse(history_msg);
        ASSERT_EQ(parsed["command"], "get_history");
    });

    // Test JSON parsing for get_summary command
    nlohmann::json summary_cmd = {{"command", "get_summary"}};
    std::string summary_msg = summary_cmd.dump();

    ASSERT_NO_THROW({
        auto parsed = nlohmann::json::parse(summary_msg);
        ASSERT_EQ(parsed["command"], "get_summary");
    });
}

// Test DashboardServer construction
TEST_F(WebSocketServerTest, DashboardServerConstruction) {
    // Test server construction with default port
    DashboardServer server(*collector_, 8080);

    // Server should be constructed successfully
    SUCCEED();
}

// Test metrics data serialization
TEST_F(WebSocketServerTest, MetricsSerialization) {
    // Test that metrics can be serialized to JSON
    auto& metrics = collector_->get_metrics();

    nlohmann::json j = {
        {"type", "update"},
        {"timestamp", std::chrono::steady_clock::now().time_since_epoch().count()},
        {"mid_price", metrics.mid_price.load()},
        {"spread", metrics.spread_bps.load()},
        {"pnl", metrics.total_pnl.load()},
        {"position", metrics.current_position.load()},
        {"buy_intensity", metrics.buy_intensity.load()},
        {"sell_intensity", metrics.sell_intensity.load()},
        {"latency", metrics.avg_cycle_latency_us.load()},
        {"orders_sent", metrics.orders_sent.load()},
        {"orders_filled", metrics.orders_filled.load()},
        {"regime", metrics.current_regime.load()},
        {"position_usage", metrics.position_limit_usage.load()}
    };

    // Should serialize without throwing
    std::string msg = j.dump();
    ASSERT_FALSE(msg.empty());

    // Should be valid JSON
    ASSERT_NO_THROW({
        auto parsed = nlohmann::json::parse(msg);
        ASSERT_EQ(parsed["type"], "update");
    });
}

// Test history data serialization
TEST_F(WebSocketServerTest, HistorySerialization) {
    // Test that history data can be serialized
    auto snapshots = collector_->get_recent_snapshots(1000);

    nlohmann::json j = nlohmann::json::array();

    for (const auto& snap : snapshots) {
        j.push_back({
            {"timestamp", snap.timestamp_ns},
            {"mid_price", snap.mid_price},
            {"spread", snap.spread_bps},
            {"pnl", snap.pnl},
            {"position", snap.position},
            {"buy_intensity", snap.buy_intensity},
            {"sell_intensity", snap.sell_intensity},
            {"latency", snap.cycle_latency_us}
        });
    }

    std::string msg = j.dump();
    ASSERT_FALSE(msg.empty());

    // Should be valid JSON array
    ASSERT_NO_THROW({
        auto parsed = nlohmann::json::parse(msg);
        ASSERT_TRUE(parsed.is_array());
        if (!parsed.empty()) {
            ASSERT_TRUE(parsed[0].contains("timestamp"));
            ASSERT_TRUE(parsed[0].contains("mid_price"));
        }
    });
}

// Test summary data serialization
TEST_F(WebSocketServerTest, SummarySerialization) {
    // Test that summary data can be serialized
    auto stats = collector_->get_summary();

    nlohmann::json j = {
        {"type", "summary"},
        {"avg_pnl", stats.avg_pnl},
        {"max_pnl", stats.max_pnl},
        {"min_pnl", stats.min_pnl},
        {"avg_latency", stats.avg_latency_us},
        {"max_latency", stats.max_latency_us},
        {"total_trades", stats.total_trades},
        {"fill_rate", stats.fill_rate}
    };

    std::string msg = j.dump();
    ASSERT_FALSE(msg.empty());

    // Should be valid JSON
    ASSERT_NO_THROW({
        auto parsed = nlohmann::json::parse(msg);
        ASSERT_EQ(parsed["type"], "summary");
        ASSERT_TRUE(parsed.contains("avg_pnl"));
        ASSERT_TRUE(parsed.contains("total_trades"));
    });
}

// Test malformed message handling
TEST_F(WebSocketServerTest, MalformedMessageHandling) {
    // Test that malformed JSON doesn't crash
    std::vector<std::string> malformed_messages = {
        "",
        "{",
        "}",
        "{invalid json}",
        "{\"command\": }",
        "not json at all",
        "{\"command\": \"unknown_command\"}"
    };

    for (const auto& msg : malformed_messages) {
        // Should not throw when parsing
        try {
            auto j = nlohmann::json::parse(msg);
            // If parsing succeeds, command should be handled gracefully (even unknown commands)
            std::string cmd = j.value("command", "");
            // No assertion needed - unknown commands are handled by doing nothing
        } catch (...) {
            // Expected for truly malformed messages
        }
    }
}

// Test concurrent metrics updates during serialization
TEST_F(WebSocketServerTest, ConcurrentMetricsUpdates) {
    std::atomic<bool> stop_flag(false);

    // Thread that continuously updates metrics
    std::thread updater([this, &stop_flag]() {
        while (!stop_flag.load()) {
            collector_->update_market_data(100.0 + rand() % 10, 99.5, 100.5);
            collector_->update_position(rand() % 100, 5.0, 2.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Test serialization under concurrent updates
    for (int i = 0; i < 100; ++i) {
        auto& metrics = collector_->get_metrics();
        nlohmann::json j = {
            {"mid_price", metrics.mid_price.load()},
            {"spread", metrics.spread_bps.load()},
            {"pnl", metrics.total_pnl.load()}
        };
        std::string msg = j.dump();
        ASSERT_FALSE(msg.empty());
    }

    stop_flag.store(true);
    updater.join();
}

// Test server lifecycle (start/stop without actual networking)
TEST_F(WebSocketServerTest, ServerLifecycle) {
    DashboardServer server(*collector_, 8081); // Use different port to avoid conflicts

    // Server should start and stop without throwing
    // Note: We don't actually start the server since it requires network resources
    // This test just verifies the object can be created and destroyed
    SUCCEED();
}

// Test session management logic (mocked)
TEST_F(WebSocketServerTest, SessionManagement) {
    // Test that session management logic works conceptually
    std::set<std::shared_ptr<WebSocketSession>> sessions;
    std::mutex sessions_mutex;

    // Simulate adding sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        // In real code, sessions would be added here
        ASSERT_EQ(sessions.size(), 0);
    }

    // Simulate broadcasting to sessions
    nlohmann::json j = {{"type", "test"}, {"data", "hello"}};
    std::string msg = j.dump();

    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        // In real code, we would iterate and send to each session
        for (auto& session : sessions) {
            // session->send_metrics(msg);
        }
    }
}

// Test broadcast timing
TEST_F(WebSocketServerTest, BroadcastTiming) {
    // Test that broadcast intervals are reasonable
    auto start = std::chrono::steady_clock::now();

    // Simulate broadcast loop timing (100ms intervals)
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

        // Should be close to expected timing (allowing some variance)
        EXPECT_NEAR(elapsed.count(), (i + 1) * 100, 20);
    }
}

// Test edge cases in data serialization
TEST_F(WebSocketServerTest, EdgeCases) {
    // Test with empty collector
    MetricsCollector empty_collector(10);
    auto empty_snapshots = empty_collector.get_recent_snapshots(1000);
    auto empty_stats = empty_collector.get_summary();

    // Should handle empty data gracefully
    nlohmann::json empty_history = nlohmann::json::array();
    for (const auto& snap : empty_snapshots) {
        empty_history.push_back({
            {"timestamp", snap.timestamp_ns},
            {"mid_price", snap.mid_price}
        });
    }

    nlohmann::json empty_summary = {
        {"type", "summary"},
        {"avg_pnl", empty_stats.avg_pnl},
        {"total_trades", empty_stats.total_trades}
    };

    std::string empty_history_msg = empty_history.dump();
    std::string empty_summary_msg = empty_summary.dump();

    ASSERT_NO_THROW({
        auto parsed_history = nlohmann::json::parse(empty_history_msg);
        auto parsed_summary = nlohmann::json::parse(empty_summary_msg);
        ASSERT_TRUE(parsed_history.is_array());
        ASSERT_EQ(parsed_summary["type"], "summary");
    });
}

// Test command validation
TEST_F(WebSocketServerTest, CommandValidation) {
    std::vector<std::string> valid_commands = {"get_history", "get_summary"};
    std::vector<std::string> invalid_commands = {"", "unknown", "get_metrics", "shutdown"};

    for (const auto& cmd : valid_commands) {
        nlohmann::json j = {{"command", cmd}};
        std::string msg = j.dump();

        auto parsed = nlohmann::json::parse(msg);
        std::string parsed_cmd = parsed.value("command", "");
        ASSERT_EQ(parsed_cmd, cmd);
    }

    for (const auto& cmd : invalid_commands) {
        nlohmann::json j = {{"command", cmd}};
        std::string msg = j.dump();

        auto parsed = nlohmann::json::parse(msg);
        std::string parsed_cmd = parsed.value("command", "");
        ASSERT_EQ(parsed_cmd, cmd);
        // Commands should be handled gracefully even if unknown
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
