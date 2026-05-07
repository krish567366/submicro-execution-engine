#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "institutional_logging.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

class InstitutionalLoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test logs
        test_dir_ = "/tmp/test_logs";
        std::filesystem::create_directory(test_dir_);
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    std::string get_test_file(const std::string& filename) {
        return test_dir_ + "/" + filename;
    }
};

TEST_F(InstitutionalLoggingTest, SHA256HasherFileChecksum) {
    // Create a test file
    std::string test_file = get_test_file("test.txt");
    std::ofstream file(test_file);
    file << "Hello, World!";
    file.close();

    std::string checksum = InstitutionalLogging::SHA256Hasher::file_checksum(test_file);

    // SHA256 checksum should be 40 characters long (hex)
    EXPECT_EQ(checksum.length(), 64);

    // Should not be an error message
    EXPECT_NE(checksum.find("ERROR"), 0);
}

TEST_F(InstitutionalLoggingTest, SHA256HasherStringChecksum) {
    std::string test_data = "Hello, World!";
    std::string checksum = InstitutionalLogging::SHA256Hasher::string_checksum(test_data);

    // SHA256 checksum should be 64 characters long (hex)
    EXPECT_EQ(checksum.length(), 64);

    // Should be consistent
    std::string checksum2 = InstitutionalLogging::SHA256Hasher::string_checksum(test_data);
    EXPECT_EQ(checksum, checksum2);
}

TEST_F(InstitutionalLoggingTest, SHA256HasherNonexistentFile) {
    std::string checksum = InstitutionalLogging::SHA256Hasher::file_checksum("/nonexistent/file.txt");
    EXPECT_EQ(checksum, "ERROR: Cannot open file");
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerConstruction) {
    std::string log_file = get_test_file("replay.log");

    {
        InstitutionalLogging::EventReplayLogger logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# DETERMINISTIC BACKTEST REPLAY LOG");
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogConfig) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    std::string config = "{\"latency\":500}";
    uint32_t seed = 42;
    std::string checksum = "abc123";

    logger.log_config(config, seed, checksum);

    // Flush and close
    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogMarketTick) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_market_tick(1000000000LL, 99.9, 100.1, 200, 150);

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogSignalDecision) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_signal_decision(1000000000LL, true, "BUY", 0.75, 5, 0.3);

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogOrderSubmit) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_order_submit(1000000000LL, 12345, "BUY", 100.0, 100);

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogOrderFill) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_order_fill(1000000000LL, 12345, 100.05, 100, 550);

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogOrderCancel) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_order_cancel(1000000000LL, 12345, "timeout");

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerLogPnlUpdate) {
    std::string log_file = get_test_file("replay.log");
    InstitutionalLogging::EventReplayLogger logger(log_file);

    logger.log_pnl_update(1000000000LL, 1000.0, 50.0, 10);

    logger.flush();
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionEmpty) {
    InstitutionalLogging::LatencyDistribution dist;

    // Should not crash when empty
    dist.calculate();

    EXPECT_EQ(dist.get_sample_count(), 0);
    EXPECT_EQ(dist.get_p50(), 0);
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionSingleSample) {
    InstitutionalLogging::LatencyDistribution dist;

    dist.add_sample(500);
    dist.calculate();

    EXPECT_EQ(dist.get_sample_count(), 1);
    EXPECT_EQ(dist.get_min(), 500);
    EXPECT_EQ(dist.get_max(), 500);
    EXPECT_EQ(dist.get_p50(), 500);
    EXPECT_DOUBLE_EQ(dist.get_mean(), 500.0);
    EXPECT_DOUBLE_EQ(dist.get_jitter(), 0.0);
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionMultipleSamples) {
    InstitutionalLogging::LatencyDistribution dist;

    // Add samples: 100, 200, 300, 400, 500
    for (int i = 1; i <= 5; ++i) {
        dist.add_sample(i * 100);
    }
    dist.calculate();

    EXPECT_EQ(dist.get_sample_count(), 5);
    EXPECT_EQ(dist.get_min(), 100);
    EXPECT_EQ(dist.get_max(), 500);
    EXPECT_EQ(dist.get_p50(), 300); // Median of sorted list
    EXPECT_DOUBLE_EQ(dist.get_mean(), 300.0);
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionPercentiles) {
    InstitutionalLogging::LatencyDistribution dist;

    // Add 100 samples from 1 to 100
    for (int i = 1; i <= 100; ++i) {
        dist.add_sample(i);
    }
    dist.calculate();

    EXPECT_EQ(dist.get_p50(), 50);   // 50th percentile
    EXPECT_EQ(dist.get_p90(), 90);   // 90th percentile
    EXPECT_EQ(dist.get_p99(), 99);   // 99th percentile
    EXPECT_EQ(dist.get_p999(), 100); // 99.9th percentile (rounded up)
}

TEST_F(InstitutionalLoggingTest, SlippageAnalyzerEmpty) {
    InstitutionalLogging::SlippageAnalyzer analyzer;

    // Should not crash
    analyzer.print_report();
}

TEST_F(InstitutionalLoggingTest, SlippageAnalyzerAddFillBuy) {
    InstitutionalLogging::SlippageAnalyzer analyzer;

    // Add a buy fill
    analyzer.add_fill(1000000000LL, 100.05, 100.0, 100.02, 100, "BUY");

    analyzer.print_report();
}

TEST_F(InstitutionalLoggingTest, SlippageAnalyzerAddFillSell) {
    InstitutionalLogging::SlippageAnalyzer analyzer;

    // Add a sell fill
    analyzer.add_fill(1000000000LL, 99.95, 100.0, 99.98, 100, "SELL");

    analyzer.print_report();
}

TEST_F(InstitutionalLoggingTest, RiskBreachLoggerConstruction) {
    std::string log_file = get_test_file("risk.log");

    {
        InstitutionalLogging::RiskBreachLogger logger(log_file);
        EXPECT_EQ(logger.get_breach_count(), 0);
    }

    // Check that file was created
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# RISK KILL-SWITCH LOG");
}

TEST_F(InstitutionalLoggingTest, RiskBreachLoggerLogPositionBreach) {
    std::string log_file = get_test_file("risk.log");
    InstitutionalLogging::RiskBreachLogger logger(log_file);

    logger.log_position_breach(1000000000LL, 1500, 1000, "reduce_position");

    EXPECT_EQ(logger.get_breach_count(), 1);
}

TEST_F(InstitutionalLoggingTest, RiskBreachLoggerLogDrawdownBreach) {
    std::string log_file = get_test_file("risk.log");
    InstitutionalLogging::RiskBreachLogger logger(log_file);

    logger.log_drawdown_breach(1000000000LL, 0.15, 0.10, "halt_trading");

    EXPECT_EQ(logger.get_breach_count(), 1);
}

TEST_F(InstitutionalLoggingTest, RiskBreachLoggerLogOrderRateBreach) {
    std::string log_file = get_test_file("risk.log");
    InstitutionalLogging::RiskBreachLogger logger(log_file);

    logger.log_order_rate_breach(1000000000LL, 150, 100, "throttle_orders");

    EXPECT_EQ(logger.get_breach_count(), 1);
}

TEST_F(InstitutionalLoggingTest, SystemVerificationLoggerGenerateReport) {
    std::string report_file = get_test_file("verification.log");

    InstitutionalLogging::SystemVerificationLogger::generate_report(report_file);

    // Check that file was created and has content
    std::ifstream file(report_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# SYSTEM VERIFICATION REPORT");
}

TEST_F(InstitutionalLoggingTest, EventReplayLoggerExceptionOnBadFile) {
    // Try to create logger with invalid path
    EXPECT_THROW(
        InstitutionalLogging::EventReplayLogger logger("/invalid/path/replay.log"),
        std::runtime_error
    );
}

TEST_F(InstitutionalLoggingTest, RiskBreachLoggerExceptionOnBadFile) {
    // Try to create logger with invalid path
    EXPECT_THROW(
        InstitutionalLogging::RiskBreachLogger logger("/invalid/path/risk.log"),
        std::runtime_error
    );
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionPrintReport) {
    InstitutionalLogging::LatencyDistribution dist;

    // Add some samples
    for (int i = 1; i <= 10; ++i) {
        dist.add_sample(i * 100);
    }
    dist.calculate();

    // Should not crash
    testing::internal::CaptureStdout();
    dist.print_report("TEST_LATENCY");
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("TEST_LATENCY LATENCY DISTRIBUTION"), std::string::npos);
    EXPECT_NE(output.find("Samples:"), std::string::npos);
}

TEST_F(InstitutionalLoggingTest, LatencyDistributionPrintHistogram) {
    InstitutionalLogging::LatencyDistribution dist;

    // Add samples with some variation
    for (int i = 1; i <= 100; ++i) {
        dist.add_sample(500 + (i % 10) * 10); // 500, 510, 520, ..., 590
    }
    dist.calculate();

    // Should not crash
    testing::internal::CaptureStdout();
    dist.print_histogram(10);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("HISTOGRAM:"), std::string::npos);
}

TEST_F(InstitutionalLoggingTest, SlippageAnalyzerMultipleFills) {
    InstitutionalLogging::SlippageAnalyzer analyzer;

    // Add multiple fills
    analyzer.add_fill(1000000000LL, 100.05, 100.0, 100.02, 100, "BUY");
    analyzer.add_fill(1000000001LL, 99.95, 100.0, 99.98, 100, "SELL");
    analyzer.add_fill(1000000002LL, 100.10, 100.0, 100.03, 100, "BUY");

    // Should not crash
    testing::internal::CaptureStdout();
    analyzer.print_report();
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("SLIPPAGE & MARKET IMPACT ANALYSIS"), std::string::npos);
    EXPECT_NE(output.find("Total Fills:"), std::string::npos);
}

} // namespace