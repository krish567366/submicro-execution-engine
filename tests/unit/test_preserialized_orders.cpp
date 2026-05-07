#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "preserialized_orders.hpp"
#include <cstring>
#include <array>

namespace {

class PreserializedOrdersTest : public ::testing::Test {
protected:
    void SetUp() override {
        submitter_ = std::make_unique<hft::preserialized::FastOrderSubmitter>(12345, 67890);
        submitter_->initialize_symbol(1, "AAPL");
    }

    void TearDown() override {
        submitter_.reset();
    }

    std::unique_ptr<hft::preserialized::FastOrderSubmitter> submitter_;
};

TEST_F(PreserializedOrdersTest, OrderMessageHeaderSize) {
    hft::preserialized::OrderMessageHeader header;
    EXPECT_EQ(sizeof(header), 24); // 4 + 2 + 2 + 8 + 4 + 4 = 24 bytes
}

TEST_F(PreserializedOrdersTest, BinaryNewOrderMessageSize) {
    hft::preserialized::BinaryNewOrderMessage msg;
    // Header (24) + client_order_id (8) + symbol_id (4) + side (1) + order_type (1) +
    // time_in_force (1) + padding (1) + price (8) + quantity (8) + checksum (4) = 60 bytes
    EXPECT_EQ(sizeof(msg), 60);
}

TEST_F(PreserializedOrdersTest, BinaryCancelOrderMessageSize) {
    hft::preserialized::BinaryCancelOrderMessage msg;
    // Header (24) + client_order_id (8) + original_order_id (8) + symbol_id (4) + padding (4) = 48 bytes
    EXPECT_EQ(sizeof(msg), 48);
}

TEST_F(PreserializedOrdersTest, OrderTemplateInitialization) {
    hft::preserialized::OrderTemplate tmpl;
    tmpl.initialize_limit_order_template(12345, 67890, 1, 0); // GTC

    EXPECT_GT(tmpl.get_buffer_size(), 0);
    EXPECT_EQ(tmpl.get_buffer_size(), sizeof(hft::preserialized::BinaryNewOrderMessage));
}

TEST_F(PreserializedOrdersTest, OrderTemplatePatchAndSend) {
    hft::preserialized::OrderTemplate tmpl;
    tmpl.initialize_limit_order_template(12345, 67890, 1, 0);

    std::array<uint8_t, 256> output_buffer;
    uint64_t order_id = 999;
    double price = 150.50;
    double quantity = 100.0;
    uint64_t timestamp = 1000000000ULL;

    tmpl.patch_and_send(order_id, hft::Side::BUY, price, quantity, timestamp, output_buffer.data());

    // Verify the patched message
    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());

    EXPECT_EQ(msg->header.client_id, 12345);
    EXPECT_EQ(msg->header.session_id, 67890);
    EXPECT_EQ(msg->header.message_type, 100); // NEW_ORDER
    EXPECT_EQ(msg->header.client_timestamp, timestamp);
    EXPECT_EQ(msg->client_order_id, order_id);
    EXPECT_EQ(msg->symbol_id, 1);
    EXPECT_EQ(msg->side, 0); // BUY
    EXPECT_DOUBLE_EQ(msg->price, price);
    EXPECT_DOUBLE_EQ(msg->quantity, quantity);
    EXPECT_EQ(msg->order_type, 1); // LIMIT
    EXPECT_EQ(msg->time_in_force, 0); // GTC
}

TEST_F(PreserializedOrdersTest, OrderTemplatePatchAndSendSell) {
    hft::preserialized::OrderTemplate tmpl;
    tmpl.initialize_limit_order_template(12345, 67890, 1, 1); // IOC

    std::array<uint8_t, 256> output_buffer;

    tmpl.patch_and_send(888, hft::Side::SELL, 149.75, 200.0, 2000000000ULL, output_buffer.data());

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());

    EXPECT_EQ(msg->side, 1); // SELL
    EXPECT_EQ(msg->time_in_force, 1); // IOC
    EXPECT_DOUBLE_EQ(msg->price, 149.75);
    EXPECT_DOUBLE_EQ(msg->quantity, 200.0);
}

TEST_F(PreserializedOrdersTest, OrderTemplatePoolInitialization) {
    hft::preserialized::OrderTemplatePool pool(11111, 22222);
    pool.initialize_symbol_templates(5, "GOOGL");

    // Should not crash
}

TEST_F(PreserializedOrdersTest, OrderTemplatePoolSubmitLimitOrderGTC) {
    hft::preserialized::OrderTemplatePool pool(11111, 22222);
    pool.initialize_symbol_templates(5, "GOOGL");

    std::array<uint8_t, 256> output_buffer;
    size_t size = pool.submit_limit_order_gtc(5, hft::Side::BUY, 2500.0, 10.0, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryNewOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->symbol_id, 5);
    EXPECT_EQ(msg->side, 0); // BUY
    EXPECT_DOUBLE_EQ(msg->price, 2500.0);
    EXPECT_DOUBLE_EQ(msg->quantity, 10.0);
    EXPECT_EQ(msg->time_in_force, 0); // GTC
}

TEST_F(PreserializedOrdersTest, OrderTemplatePoolSubmitLimitOrderIOC) {
    hft::preserialized::OrderTemplatePool pool(11111, 22222);
    pool.initialize_symbol_templates(5, "GOOGL");

    std::array<uint8_t, 256> output_buffer;
    size_t size = pool.submit_limit_order_ioc(5, hft::Side::SELL, 2499.0, 5.0, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryNewOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->symbol_id, 5);
    EXPECT_EQ(msg->side, 1); // SELL
    EXPECT_DOUBLE_EQ(msg->price, 2499.0);
    EXPECT_DOUBLE_EQ(msg->quantity, 5.0);
    EXPECT_EQ(msg->time_in_force, 1); // IOC
}

TEST_F(PreserializedOrdersTest, OrderTemplatePoolSubmitCancelOrder) {
    hft::preserialized::OrderTemplatePool pool(11111, 22222);

    std::array<uint8_t, 256> output_buffer;
    size_t size = pool.submit_cancel_order(5, 777, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryCancelOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryCancelOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->header.message_type, 101); // CANCEL_ORDER
    EXPECT_EQ(msg->symbol_id, 5);
    EXPECT_EQ(msg->original_order_id, 777);
}

TEST_F(PreserializedOrdersTest, FastOrderSubmitterInitialization) {
    hft::preserialized::FastOrderSubmitter submitter(99999, 88888);
    submitter.initialize_symbol(10, "MSFT");

    // Should not crash
}

TEST_F(PreserializedOrdersTest, FastOrderSubmitterSubmitLimitOrderGTC) {
    std::array<uint8_t, 256> output_buffer;
    size_t size = submitter_->submit_limit_order(1, hft::Side::BUY, 180.0, 25.0, false, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryNewOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->symbol_id, 1);
    EXPECT_EQ(msg->side, 0); // BUY
    EXPECT_DOUBLE_EQ(msg->price, 180.0);
    EXPECT_DOUBLE_EQ(msg->quantity, 25.0);
    EXPECT_EQ(msg->time_in_force, 0); // GTC
}

TEST_F(PreserializedOrdersTest, FastOrderSubmitterSubmitLimitOrderIOC) {
    std::array<uint8_t, 256> output_buffer;
    size_t size = submitter_->submit_limit_order(1, hft::Side::SELL, 179.5, 15.0, true, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryNewOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->symbol_id, 1);
    EXPECT_EQ(msg->side, 1); // SELL
    EXPECT_DOUBLE_EQ(msg->price, 179.5);
    EXPECT_DOUBLE_EQ(msg->quantity, 15.0);
    EXPECT_EQ(msg->time_in_force, 1); // IOC
}

TEST_F(PreserializedOrdersTest, FastOrderSubmitterSubmitCancel) {
    std::array<uint8_t, 256> output_buffer;
    size_t size = submitter_->submit_cancel(1, 555, output_buffer.data());

    EXPECT_EQ(size, sizeof(hft::preserialized::BinaryCancelOrderMessage));

    auto* msg = reinterpret_cast<hft::preserialized::BinaryCancelOrderMessage*>(output_buffer.data());
    EXPECT_EQ(msg->header.message_type, 101); // CANCEL_ORDER
    EXPECT_EQ(msg->symbol_id, 1);
    EXPECT_EQ(msg->original_order_id, 555);
}

TEST_F(PreserializedOrdersTest, MessageHeaderFields) {
    hft::preserialized::OrderMessageHeader header;
    header.sequence_number = 42;
    header.message_type = 100;
    header.message_length = 60;
    header.client_timestamp = 1234567890ULL;
    header.client_id = 11111;
    header.session_id = 22222;

    EXPECT_EQ(header.sequence_number, 42);
    EXPECT_EQ(header.message_type, 100);
    EXPECT_EQ(header.message_length, 60);
    EXPECT_EQ(header.client_timestamp, 1234567890ULL);
    EXPECT_EQ(header.client_id, 11111);
    EXPECT_EQ(header.session_id, 22222);
}

TEST_F(PreserializedOrdersTest, BinaryNewOrderMessageFields) {
    hft::preserialized::BinaryNewOrderMessage msg;
    msg.client_order_id = 999;
    msg.symbol_id = 7;
    msg.side = 1; // SELL
    msg.order_type = 1; // LIMIT
    msg.time_in_force = 2; // FOK
    msg.price = 123.45;
    msg.quantity = 67.89;
    msg.checksum = 0xDEADBEEF;

    EXPECT_EQ(msg.client_order_id, 999);
    EXPECT_EQ(msg.symbol_id, 7);
    EXPECT_EQ(msg.side, 1);
    EXPECT_EQ(msg.order_type, 1);
    EXPECT_EQ(msg.time_in_force, 2);
    EXPECT_DOUBLE_EQ(msg.price, 123.45);
    EXPECT_DOUBLE_EQ(msg.quantity, 67.89);
    EXPECT_EQ(msg.checksum, 0xDEADBEEF);
}

TEST_F(PreserializedOrdersTest, BinaryCancelOrderMessageFields) {
    hft::preserialized::BinaryCancelOrderMessage msg;
    msg.client_order_id = 888;
    msg.original_order_id = 777;
    msg.symbol_id = 3;

    EXPECT_EQ(msg.client_order_id, 888);
    EXPECT_EQ(msg.original_order_id, 777);
    EXPECT_EQ(msg.symbol_id, 3);
}

TEST_F(PreserializedOrdersTest, OrderTemplateBufferSize) {
    hft::preserialized::OrderTemplate tmpl;
    EXPECT_EQ(tmpl.get_buffer_size(), 0);

    tmpl.initialize_limit_order_template(1, 2, 3, 4);
    EXPECT_EQ(tmpl.get_buffer_size(), sizeof(hft::preserialized::BinaryNewOrderMessage));
}

TEST_F(PreserializedOrdersTest, MultipleSymbolsSupport) {
    hft::preserialized::FastOrderSubmitter submitter(33333, 44444);
    submitter.initialize_symbol(1, "AAPL");
    submitter.initialize_symbol(2, "GOOGL");
    submitter.initialize_symbol(3, "MSFT");

    std::array<uint8_t, 256> buffer1, buffer2, buffer3;

    submitter.submit_limit_order(1, hft::Side::BUY, 150.0, 10.0, false, buffer1.data());
    submitter.submit_limit_order(2, hft::Side::SELL, 2500.0, 5.0, false, buffer2.data());
    submitter.submit_limit_order(3, hft::Side::BUY, 300.0, 8.0, true, buffer3.data());

    auto* msg1 = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(buffer1.data());
    auto* msg2 = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(buffer2.data());
    auto* msg3 = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(buffer3.data());

    EXPECT_EQ(msg1->symbol_id, 1);
    EXPECT_EQ(msg2->symbol_id, 2);
    EXPECT_EQ(msg3->symbol_id, 3);

    EXPECT_EQ(msg1->time_in_force, 0); // GTC
    EXPECT_EQ(msg3->time_in_force, 1); // IOC
}

TEST_F(PreserializedOrdersTest, TimestampGeneration) {
    std::array<uint8_t, 256> output_buffer;
    uint64_t before = std::chrono::steady_clock::now().time_since_epoch().count();

    submitter_->submit_limit_order(1, hft::Side::BUY, 100.0, 1.0, false, output_buffer.data());

    uint64_t after = std::chrono::steady_clock::now().time_since_epoch().count();

    auto* msg = reinterpret_cast<hft::preserialized::BinaryNewOrderMessage*>(output_buffer.data());

    // Timestamp should be between before and after (allowing some tolerance)
    EXPECT_GE(msg->header.client_timestamp, before - 1000000); // 1ms tolerance
    EXPECT_LE(msg->header.client_timestamp, after + 1000000);
}

} // namespace