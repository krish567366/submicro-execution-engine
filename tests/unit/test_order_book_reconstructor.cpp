#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "order_book_reconstructor.hpp"
#include <thread>
#include <chrono>

namespace {

class OrderBookReconstructorTest : public ::testing::Test {
protected:
    void SetUp() override {
        reconstructor_ = std::make_unique<hft::OrderBookReconstructor>("TEST");
    }

    void TearDown() override {
        reconstructor_.reset();
    }

    std::unique_ptr<hft::OrderBookReconstructor> reconstructor_;

    // Helper to create a basic snapshot
    hft::OrderBookSnapshot create_basic_snapshot() {
        hft::OrderBookSnapshot snapshot;
        snapshot.symbol = "TEST";
        snapshot.sequence_number = 1000;
        snapshot.timestamp_ns = 1000000000LL;

        // Add some bids (descending prices)
        snapshot.bids.push_back(hft::PriceLevel(99.9, 100.0, 5));
        snapshot.bids.push_back(hft::PriceLevel(99.8, 200.0, 3));
        snapshot.bids.push_back(hft::PriceLevel(99.7, 150.0, 2));

        // Add some asks (ascending prices)
        snapshot.asks.push_back(hft::PriceLevel(100.1, 120.0, 4));
        snapshot.asks.push_back(hft::PriceLevel(100.2, 180.0, 6));
        snapshot.asks.push_back(hft::PriceLevel(100.3, 90.0, 1));

        return snapshot;
    }

    // Helper to create an update
    hft::OrderBookUpdate create_update(hft::UpdateType type, uint64_t order_id,
                                      double price, double quantity, bool is_bid,
                                      uint64_t seq_num) {
        hft::OrderBookUpdate update;
        update.type = type;
        update.order_id = order_id;
        update.price = price;
        update.quantity = quantity;
        update.is_bid = is_bid;
        update.sequence_number = seq_num;
        update.timestamp_ns = 1000000000LL + seq_num * 1000;
        update.exchange_timestamp_ns = update.timestamp_ns;
        return update;
    }
};

TEST_F(OrderBookReconstructorTest, ConstructorInitialization) {
    // Just test that we can create the object without crashing
    EXPECT_NE(reconstructor_.get(), nullptr);
}

TEST_F(OrderBookReconstructorTest, InitializeFromSnapshot) {
    auto snapshot = create_basic_snapshot();
    bool success = reconstructor_->initialize_from_snapshot(snapshot);

    EXPECT_TRUE(success);

    auto [best_bid, best_ask] = reconstructor_->get_top_of_book();
    ASSERT_TRUE(best_bid.has_value());
    ASSERT_TRUE(best_ask.has_value());

    EXPECT_DOUBLE_EQ(best_bid->price, 99.9);
    EXPECT_DOUBLE_EQ(best_bid->quantity, 100.0);
    EXPECT_DOUBLE_EQ(best_ask->price, 100.1);
    EXPECT_DOUBLE_EQ(best_ask->quantity, 120.0);
}

TEST_F(OrderBookReconstructorTest, ProcessAddUpdate) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Add a new bid order
    auto update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1001);
    bool success = reconstructor_->process_update(update);

    EXPECT_TRUE(success);

    auto [best_bid, best_ask] = reconstructor_->get_top_of_book();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_DOUBLE_EQ(best_bid->price, 99.95); // New price should be best bid
    EXPECT_DOUBLE_EQ(best_bid->quantity, 50.0);
}

TEST_F(OrderBookReconstructorTest, ProcessModifyUpdate) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // First add an order
    auto add_update = create_update(hft::UpdateType::ADD, 1001, 99.9, 50.0, true, 1001);
    reconstructor_->process_update(add_update);

    // Then modify it
    auto modify_update = create_update(hft::UpdateType::MODIFY, 1001, 99.9, 75.0, true, 1002);
    bool success = reconstructor_->process_update(modify_update);

    EXPECT_TRUE(success);

    auto [best_bid, best_ask] = reconstructor_->get_top_of_book();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_DOUBLE_EQ(best_bid->price, 99.9);
    EXPECT_DOUBLE_EQ(best_bid->quantity, 125.0); // 100.0 + 75.0 - 50.0
}

TEST_F(OrderBookReconstructorTest, ProcessDeleteUpdate) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Add an order
    auto add_update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1001);
    reconstructor_->process_update(add_update);

    // Delete it
    auto delete_update = create_update(hft::UpdateType::DELETE, 1001, 99.95, 50.0, true, 1002);
    bool success = reconstructor_->process_update(delete_update);

    EXPECT_TRUE(success);

    auto [best_bid, best_ask] = reconstructor_->get_top_of_book();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_DOUBLE_EQ(best_bid->price, 99.9); // Should revert to original best bid
}

TEST_F(OrderBookReconstructorTest, ProcessExecuteUpdate) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Execute part of an existing order
    auto execute_update = create_update(hft::UpdateType::EXECUTE, 1001, 99.9, 50.0, true, 1001);
    bool success = reconstructor_->process_update(execute_update);

    EXPECT_TRUE(success);

    auto [best_bid, best_ask] = reconstructor_->get_top_of_book();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_DOUBLE_EQ(best_bid->price, 99.9);
    EXPECT_DOUBLE_EQ(best_bid->quantity, 50.0); // 100.0 - 50.0
}

TEST_F(OrderBookReconstructorTest, SequenceNumberGapDetection) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Skip sequence number (gap)
    auto update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1005); // seq 1005 vs expected 1001
    bool success = reconstructor_->process_update(update);

    EXPECT_FALSE(success);
    EXPECT_TRUE(reconstructor_->needs_snapshot_recovery());
}

TEST_F(OrderBookReconstructorTest, ResetGapDetection) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Create gap
    auto update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1005);
    reconstructor_->process_update(update);

    EXPECT_TRUE(reconstructor_->needs_snapshot_recovery());

    // Reset
    reconstructor_->reset_gap_detection();
    EXPECT_FALSE(reconstructor_->needs_snapshot_recovery());
}

TEST_F(OrderBookReconstructorTest, GetDepth) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    auto [bids, asks] = reconstructor_->get_depth(2);

    EXPECT_EQ(bids.size(), 2);
    EXPECT_EQ(asks.size(), 2);

    // Bids should be sorted descending (best first)
    EXPECT_DOUBLE_EQ(bids[0].price, 99.9);
    EXPECT_DOUBLE_EQ(bids[1].price, 99.8);

    // Asks should be sorted ascending (best first)
    EXPECT_DOUBLE_EQ(asks[0].price, 100.1);
    EXPECT_DOUBLE_EQ(asks[1].price, 100.2);
}

TEST_F(OrderBookReconstructorTest, GetStatistics) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Add some updates
    auto update1 = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1001);
    auto update2 = create_update(hft::UpdateType::ADD, 1002, 100.15, 30.0, false, 1002);

    reconstructor_->process_update(update1);
    reconstructor_->process_update(update2);

    auto stats = reconstructor_->get_statistics();

    EXPECT_EQ(stats.total_updates, 2);
    EXPECT_EQ(stats.current_bid_levels, 4); // 3 original + 1 new
    EXPECT_EQ(stats.current_ask_levels, 4); // 3 original + 1 new
    EXPECT_DOUBLE_EQ(stats.last_mid_price, (99.95 + 100.15) / 2.0);
    EXPECT_DOUBLE_EQ(stats.last_spread, 100.15 - 99.95);
}

TEST_F(OrderBookReconstructorTest, DeepOFICalculation) {
    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Add an update to trigger OFI calculation
    auto update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1001);
    reconstructor_->process_update(update);

    auto ofi = reconstructor_->get_current_ofi();

    // Check basic OFI properties
    EXPECT_NE(ofi.timestamp_ns, 0);
    EXPECT_DOUBLE_EQ(ofi.mid_price, (99.95 + 100.1) / 2.0);
    EXPECT_DOUBLE_EQ(ofi.bid_ask_spread, 100.1 - 99.95);

    // Volume imbalance should be reasonable
    EXPECT_GE(ofi.volume_imbalance, -1.0);
    EXPECT_LE(ofi.volume_imbalance, 1.0);
}

TEST_F(OrderBookReconstructorTest, CallbackRegistration) {
    bool callback_called = false;
    hft::DeepOFIFeatures captured_features;

    reconstructor_->register_deep_state_callback(
        [&](const hft::DeepOFIFeatures& features) {
            callback_called = true;
            captured_features = features;
        }
    );

    auto snapshot = create_basic_snapshot();
    reconstructor_->initialize_from_snapshot(snapshot);

    // Trigger callback with an update
    auto update = create_update(hft::UpdateType::ADD, 1001, 99.95, 50.0, true, 1001);
    reconstructor_->process_update(update);

    EXPECT_TRUE(callback_called);
    EXPECT_NE(captured_features.timestamp_ns, 0);
}

TEST_F(OrderBookReconstructorTest, PriceLevelOperations) {
    hft::PriceLevel level(100.0, 50.0, 2);

    EXPECT_DOUBLE_EQ(level.price, 100.0);
    EXPECT_DOUBLE_EQ(level.quantity, 50.0);
    EXPECT_EQ(level.order_count, 2);
}

TEST_F(OrderBookReconstructorTest, TrackedOrderOperations) {
    hft::TrackedOrder order;
    order.order_id = 12345;
    order.price = 100.0;
    order.quantity = 50.0;
    order.is_bid = true;
    order.timestamp_ns = 1000000000LL;

    EXPECT_EQ(order.order_id, 12345);
    EXPECT_DOUBLE_EQ(order.price, 100.0);
    EXPECT_DOUBLE_EQ(order.quantity, 50.0);
    EXPECT_TRUE(order.is_bid);
    EXPECT_EQ(order.timestamp_ns, 1000000000LL);
}

TEST_F(OrderBookReconstructorTest, OrderBookUpdateDefaults) {
    hft::OrderBookUpdate update;

    EXPECT_EQ(update.type, hft::UpdateType::ADD);
    EXPECT_EQ(update.order_id, 0);
    EXPECT_DOUBLE_EQ(update.price, 0.0);
    EXPECT_DOUBLE_EQ(update.quantity, 0.0);
    EXPECT_TRUE(update.is_bid);
    EXPECT_EQ(update.sequence_number, 0);
    EXPECT_EQ(update.timestamp_ns, 0);
    EXPECT_EQ(update.exchange_timestamp_ns, 0);
}

TEST_F(OrderBookReconstructorTest, DeepOFIFeaturesDefaults) {
    hft::DeepOFIFeatures features;

    EXPECT_DOUBLE_EQ(features.total_ofi, 0.0);
    EXPECT_DOUBLE_EQ(features.weighted_ofi, 0.0);
    EXPECT_DOUBLE_EQ(features.volume_imbalance, 0.0);
    EXPECT_DOUBLE_EQ(features.mid_price, 0.0);
    EXPECT_EQ(features.timestamp_ns, 0);

    // Check arrays are zeroed
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_DOUBLE_EQ(features.bid_ofi[i], 0.0);
        EXPECT_DOUBLE_EQ(features.ask_ofi[i], 0.0);
    }
}

TEST_F(OrderBookReconstructorTest, OrderBookSnapshotDefaults) {
    hft::OrderBookSnapshot snapshot;

    EXPECT_EQ(snapshot.sequence_number, 0);
    EXPECT_EQ(snapshot.timestamp_ns, 0);
    EXPECT_TRUE(snapshot.symbol.empty());
    EXPECT_TRUE(snapshot.bids.empty());
    EXPECT_TRUE(snapshot.asks.empty());
}

} // namespace