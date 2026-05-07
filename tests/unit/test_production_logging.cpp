#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "production_logging.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

class ProductionLoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test logs
        test_dir_ = "/tmp/test_production_logs";
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

TEST_F(ProductionLoggingTest, NICHardwareLogConstruction) {
    std::string log_file = get_test_file("nic.log");

    {
        hft::NICHardwareLog logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# nic_rx_tx_hw_ts.log");
}

TEST_F(ProductionLoggingTest, NICHardwareLogRxPacket) {
    std::string log_file = get_test_file("nic.log");
    hft::NICHardwareLog logger(log_file);

    logger.log_rx_packet(12345, "NSE_EQ", 1000000000ULL);

    // File should be written (no explicit flush needed for this test)
}

TEST_F(ProductionLoggingTest, NICHardwareLogTxPacket) {
    std::string log_file = get_test_file("nic.log");
    hft::NICHardwareLog logger(log_file);

    logger.log_tx_packet(12345, "NSE_EQ", 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, StrategyTraceLogConstruction) {
    std::string log_file = get_test_file("strategy.log");

    {
        hft::StrategyTraceLog logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# strategy_trace.log");
}

TEST_F(ProductionLoggingTest, StrategyTraceLogEventRx) {
    std::string log_file = get_test_file("strategy.log");
    hft::StrategyTraceLog logger(log_file);

    logger.log_event_rx(12345, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, StrategyTraceLogEventDecision) {
    std::string log_file = get_test_file("strategy.log");
    hft::StrategyTraceLog logger(log_file);

    logger.log_event_decision("BUY", 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, StrategyTraceLogEventSend) {
    std::string log_file = get_test_file("strategy.log");
    hft::StrategyTraceLog logger(log_file);

    logger.log_event_send(12345, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, ExchangeACKLogConstruction) {
    std::string log_file = get_test_file("exchange.log");

    {
        hft::ExchangeACKLog logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# exchange_ack.log");
}

TEST_F(ProductionLoggingTest, ExchangeACKLogAck) {
    std::string log_file = get_test_file("exchange.log");
    hft::ExchangeACKLog logger(log_file);

    logger.log_ack(12345, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, ExchangeACKLogFill) {
    std::string log_file = get_test_file("exchange.log");
    hft::ExchangeACKLog logger(log_file);

    logger.log_fill(12345, 100, 100.50, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, ExchangeACKLogReject) {
    std::string log_file = get_test_file("exchange.log");
    hft::ExchangeACKLog logger(log_file);

    logger.log_reject(12345, "INVALID_PRICE", 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, PTPSyncLogConstruction) {
    std::string log_file = get_test_file("ptp.log");

    {
        hft::PTPSyncLog logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# ptp_sync.log");
}

TEST_F(ProductionLoggingTest, PTPSyncLogSync) {
    std::string log_file = get_test_file("ptp.log");
    hft::PTPSyncLog logger(log_file);

    logger.log_sync(1000000000ULL, 17, 0.3);

    // File should be written
}

TEST_F(ProductionLoggingTest, PTPSyncLogGmChange) {
    std::string log_file = get_test_file("ptp.log");
    hft::PTPSyncLog logger(log_file);

    logger.log_gm_change("192.168.1.1", "192.168.1.2", 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, OrderGatewayLogConstruction) {
    std::string log_file = get_test_file("gateway.log");

    {
        hft::OrderGatewayLog logger(log_file);
        // Logger should be created successfully
    }

    // Check that file was created and has header
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# order_gateway.log");
}

TEST_F(ProductionLoggingTest, OrderGatewayLogSubmit) {
    std::string log_file = get_test_file("gateway.log");
    hft::OrderGatewayLog logger(log_file);

    logger.log_submit(12345, "BUY", 100.50, 100, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, OrderGatewayLogCancel) {
    std::string log_file = get_test_file("gateway.log");
    hft::OrderGatewayLog logger(log_file);

    logger.log_cancel(12345, 1000000000ULL);

    // File should be written
}

TEST_F(ProductionLoggingTest, ManifestGeneratorAddFile) {
    hft::ManifestGenerator manifest;

    manifest.add_file("test1.log", "abc123");
    manifest.add_file("test2.log", "def456");

    // Should not crash
    manifest.write_manifest(get_test_file("manifest.sha256"));
}

TEST_F(ProductionLoggingTest, ManifestGeneratorWriteManifest) {
    hft::ManifestGenerator manifest;

    manifest.add_file("nic.log", "a18f7c2e...");
    manifest.add_file("strategy.log", "b2e19f44...");

    std::string manifest_file = get_test_file("manifest.sha256");
    manifest.write_manifest(manifest_file);

    // Check that manifest file was created
    std::ifstream file(manifest_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line);
    EXPECT_EQ(line, "# MANIFEST.sha256");
}

TEST_F(ProductionLoggingTest, ProductionLogBundleConstruction) {
    std::string run_id = "test_run_001";

    // Create logs directory
    std::filesystem::create_directory(test_dir_ + "/logs");

    hft::ProductionLogBundle bundle(run_id);

    // Should not crash
    bundle.finalize();
}

TEST_F(ProductionLoggingTest, ProductionLogBundleAccessors) {
    std::string run_id = "test_run_002";

    // Create logs directory
    std::filesystem::create_directory(test_dir_ + "/logs");

    hft::ProductionLogBundle bundle(run_id);

    // Test that all loggers are accessible
    auto& nic = bundle.nic();
    auto& strategy = bundle.strategy();
    auto& exchange = bundle.exchange();
    auto& ptp = bundle.ptp();
    auto& gateway = bundle.gateway();

    // Should not crash when accessing
    nic.log_rx_packet(1, "TEST", 1000000000ULL);
    strategy.log_event_rx(1, 1000000000ULL);
    exchange.log_ack(1, 1000000000ULL);
    ptp.log_sync(1000000000ULL, 0, 0.0);
    gateway.log_submit(1, "BUY", 100.0, 100, 1000000000ULL);

    bundle.finalize();
}

TEST_F(ProductionLoggingTest, MultipleLogEntries) {
    std::string log_file = get_test_file("multi.log");
    hft::NICHardwareLog logger(log_file);

    // Log multiple entries
    for (uint64_t i = 1; i <= 10; ++i) {
        logger.log_rx_packet(i, "NSE_EQ", 1000000000ULL + i * 1000);
        logger.log_tx_packet(i, "NSE_EQ", 1000000000ULL + i * 1000 + 500);
    }

    // Should not crash
}

TEST_F(ProductionLoggingTest, LogFileIntegrity) {
    std::string log_file = get_test_file("integrity.log");
    {
        hft::ExchangeACKLog logger(log_file);
        logger.log_ack(12345, 1000000000ULL);
        logger.log_fill(12345, 100, 100.50, 1000000500ULL);
        logger.log_reject(12346, "INVALID_QTY", 1000001000ULL);
    } // Logger goes out of scope and closes file

    // Verify file contents
    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    bool found_ack = false;
    bool found_fill = false;
    bool found_reject = false;

    while (std::getline(file, line)) {
        if (line.find("ACK order_id=12345") != std::string::npos) {
            found_ack = true;
        }
        if (line.find("FILL order_id=12345") != std::string::npos) {
            found_fill = true;
        }
        if (line.find("REJECT order_id=12346") != std::string::npos) {
            found_reject = true;
        }
    }

    EXPECT_TRUE(found_ack);
    EXPECT_TRUE(found_fill);
    EXPECT_TRUE(found_reject);
}

TEST_F(ProductionLoggingTest, ManifestContainsExpectedFiles) {
    hft::ManifestGenerator manifest;

    manifest.add_file("nic_rx_tx_hw_ts_test.log", "a18f7c2e...");
    manifest.add_file("strategy_trace_test.log", "b2e19f44...");
    manifest.add_file("exchange_ack_test.log", "9f7c33aa...");

    std::string manifest_file = get_test_file("test_manifest.sha256");
    manifest.write_manifest(manifest_file);

    // Verify manifest contents
    std::ifstream file(manifest_file);
    ASSERT_TRUE(file.is_open());

    std::string line;
    int file_count = 0;
    while (std::getline(file, line)) {
        if (line.find("nic_rx_tx_hw_ts_test.log") != std::string::npos ||
            line.find("strategy_trace_test.log") != std::string::npos ||
            line.find("exchange_ack_test.log") != std::string::npos) {
            file_count++;
        }
    }

    EXPECT_EQ(file_count, 3);
}

} // namespace