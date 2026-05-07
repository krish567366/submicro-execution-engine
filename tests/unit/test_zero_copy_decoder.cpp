#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "zero_copy_decoder.hpp"
#include <cstring>
#include <vector>

namespace hft {
namespace zerocopy {

class ZeroCopyDecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }

    void TearDown() override {
        // Cleanup
    }
};

// Test binary message structure sizes (critical for zero-copy alignment)
TEST_F(ZeroCopyDecoderTest, BinaryMessageHeaderSize) {
    EXPECT_EQ(sizeof(BinaryMessageHeader), 16);  // 4+2+2+8 bytes
}

TEST_F(ZeroCopyDecoderTest, BinaryOrderBookUpdateSize) {
    EXPECT_EQ(sizeof(BinaryOrderBookUpdate), 48);  // header + 8+4+1+1+2+8+8 bytes
}

TEST_F(ZeroCopyDecoderTest, BinaryTradeMessageSize) {
    EXPECT_EQ(sizeof(BinaryTradeMessage), 48);  // header + 8+4+1+3+8+8 bytes
}

TEST_F(ZeroCopyDecoderTest, BinaryQuoteMessageSize) {
    EXPECT_EQ(sizeof(BinaryQuoteMessage), 56);  // header + 4+4+8+8+8+8 bytes
}

// Test zero-copy parsing functions
TEST_F(ZeroCopyDecoderTest, ParseOrderBookUpdate) {
    // Create test buffer with order book update
    std::vector<uint8_t> buffer(sizeof(BinaryOrderBookUpdate));
    auto* update = reinterpret_cast<BinaryOrderBookUpdate*>(buffer.data());

    // Fill with test data
    update->header.sequence_number = 12345;
    update->header.message_type = 1;
    update->header.message_length = sizeof(BinaryOrderBookUpdate);
    update->header.timestamp_ns = 1640995200000000000ULL;  // 2022-01-01 00:00:00
    update->order_id = 987654321ULL;
    update->symbol_id = 42;
    update->side = 0;  // bid
    update->update_type = 1;  // MODIFY
    update->price = 50000.0;
    update->quantity = 1.5;

    // Parse using zero-copy decoder
    const auto* parsed = ZeroCopyDecoder::parse_order_book_update(buffer.data());

    // Verify zero-copy (same memory address)
    EXPECT_EQ(parsed, reinterpret_cast<const BinaryOrderBookUpdate*>(buffer.data()));

    // Verify parsed data
    EXPECT_EQ(parsed->header.sequence_number, 12345ULL);
    EXPECT_EQ(parsed->header.message_type, 1);
    EXPECT_EQ(parsed->header.message_length, sizeof(BinaryOrderBookUpdate));
    EXPECT_EQ(parsed->header.timestamp_ns, 1640995200000000000ULL);
    EXPECT_EQ(parsed->order_id, 987654321ULL);
    EXPECT_EQ(parsed->symbol_id, 42U);
    EXPECT_EQ(parsed->side, 0);
    EXPECT_EQ(parsed->update_type, 1);
    EXPECT_DOUBLE_EQ(parsed->price, 50000.0);
    EXPECT_DOUBLE_EQ(parsed->quantity, 1.5);
}

TEST_F(ZeroCopyDecoderTest, ParseTradeMessage) {
    // Create test buffer with trade message
    std::vector<uint8_t> buffer(sizeof(BinaryTradeMessage));
    auto* trade = reinterpret_cast<BinaryTradeMessage*>(buffer.data());

    // Fill with test data
    trade->header.sequence_number = 67890;
    trade->header.message_type = 2;
    trade->header.message_length = sizeof(BinaryTradeMessage);
    trade->header.timestamp_ns = 1640995260000000000ULL;
    trade->trade_id = 555666777ULL;
    trade->symbol_id = 7;
    trade->aggressor_side = 1;  // sell
    trade->price = 49999.5;
    trade->quantity = 2.0;

    // Parse using zero-copy decoder
    const auto* parsed = ZeroCopyDecoder::parse_trade(buffer.data());

    // Verify zero-copy
    EXPECT_EQ(parsed, reinterpret_cast<const BinaryTradeMessage*>(buffer.data()));

    // Verify parsed data
    EXPECT_EQ(parsed->header.sequence_number, 67890ULL);
    EXPECT_EQ(parsed->header.message_type, 2);
    EXPECT_EQ(parsed->trade_id, 555666777ULL);
    EXPECT_EQ(parsed->symbol_id, 7U);
    EXPECT_EQ(parsed->aggressor_side, 1);
    EXPECT_DOUBLE_EQ(parsed->price, 49999.5);
    EXPECT_DOUBLE_EQ(parsed->quantity, 2.0);
}

TEST_F(ZeroCopyDecoderTest, ParseQuoteMessage) {
    // Create test buffer with quote message
    std::vector<uint8_t> buffer(sizeof(BinaryQuoteMessage));
    auto* quote = reinterpret_cast<BinaryQuoteMessage*>(buffer.data());

    // Fill with test data
    quote->header.sequence_number = 11111;
    quote->header.message_type = 3;
    quote->header.message_length = sizeof(BinaryQuoteMessage);
    quote->header.timestamp_ns = 1640995320000000000ULL;
    quote->symbol_id = 100;
    quote->bid_price = 49995.0;
    quote->bid_quantity = 10.0;
    quote->ask_price = 50005.0;
    quote->ask_quantity = 15.0;

    // Parse using zero-copy decoder
    const auto* parsed = ZeroCopyDecoder::parse_quote(buffer.data());

    // Verify zero-copy
    EXPECT_EQ(parsed, reinterpret_cast<const BinaryQuoteMessage*>(buffer.data()));

    // Verify parsed data
    EXPECT_EQ(parsed->header.sequence_number, 11111ULL);
    EXPECT_EQ(parsed->header.message_type, 3);
    EXPECT_EQ(parsed->symbol_id, 100U);
    EXPECT_DOUBLE_EQ(parsed->bid_price, 49995.0);
    EXPECT_DOUBLE_EQ(parsed->bid_quantity, 10.0);
    EXPECT_DOUBLE_EQ(parsed->ask_price, 50005.0);
    EXPECT_DOUBLE_EQ(parsed->ask_quantity, 15.0);
}

// Test header validation
TEST_F(ZeroCopyDecoderTest, ValidateHeaderValid) {
    std::vector<uint8_t> buffer(sizeof(BinaryMessageHeader) + 10);
    auto* header = reinterpret_cast<BinaryMessageHeader*>(buffer.data());

    header->sequence_number = 12345;
    header->message_type = 100;
    header->message_length = static_cast<uint16_t>(buffer.size());
    header->timestamp_ns = 1640995200000000000ULL;

    EXPECT_TRUE(ZeroCopyDecoder::validate_header(buffer.data(), buffer.size()));
}

TEST_F(ZeroCopyDecoderTest, ValidateHeaderTooSmall) {
    std::vector<uint8_t> buffer(sizeof(BinaryMessageHeader) - 1);
    EXPECT_FALSE(ZeroCopyDecoder::validate_header(buffer.data(), buffer.size()));
}

TEST_F(ZeroCopyDecoderTest, ValidateHeaderLengthTooLarge) {
    std::vector<uint8_t> buffer(sizeof(BinaryMessageHeader));
    auto* header = reinterpret_cast<BinaryMessageHeader*>(buffer.data());

    header->sequence_number = 12345;
    header->message_type = 100;
    header->message_length = 1000;  // Larger than buffer
    header->timestamp_ns = 1640995200000000000ULL;

    EXPECT_FALSE(ZeroCopyDecoder::validate_header(buffer.data(), buffer.size()));
}

TEST_F(ZeroCopyDecoderTest, ValidateHeaderInvalidMessageType) {
    std::vector<uint8_t> buffer(sizeof(BinaryMessageHeader));
    auto* header = reinterpret_cast<BinaryMessageHeader*>(buffer.data());

    header->sequence_number = 12345;
    header->message_type = 300;  // > 255
    header->message_length = static_cast<uint16_t>(buffer.size());
    header->timestamp_ns = 1640995200000000000ULL;

    EXPECT_FALSE(ZeroCopyDecoder::validate_header(buffer.data(), buffer.size()));
}

// Test field extraction methods
TEST_F(ZeroCopyDecoderTest, FieldExtraction) {
    BinaryOrderBookUpdate update;
    update.order_id = 123456789ULL;
    update.price = 45000.5;
    update.quantity = 5.25;
    update.side = 0;  // bid
    update.header.sequence_number = 99999;

    EXPECT_EQ(ZeroCopyDecoder::get_order_id(&update), 123456789ULL);
    EXPECT_DOUBLE_EQ(ZeroCopyDecoder::get_price(&update), 45000.5);
    EXPECT_DOUBLE_EQ(ZeroCopyDecoder::get_quantity(&update), 5.25);
    EXPECT_TRUE(ZeroCopyDecoder::is_bid_side(&update));
    EXPECT_EQ(ZeroCopyDecoder::get_sequence_number(&update), 99999ULL);

    // Test ask side
    update.side = 1;
    EXPECT_FALSE(ZeroCopyDecoder::is_bid_side(&update));
}

// Test SymbolMapper functionality
TEST_F(ZeroCopyDecoderTest, SymbolMapperInitialization) {
    SymbolMapper mapper;

    // Test pre-populated symbols
    EXPECT_EQ(mapper.get_id("BTCUSD"), 1U);
    EXPECT_EQ(mapper.get_id("ETHUSD"), 2U);
    EXPECT_EQ(mapper.get_id("SOLUSD"), 3U);
    EXPECT_EQ(mapper.get_id("BNBUSD"), 4U);
    EXPECT_EQ(mapper.get_id("XRPUSD"), 5U);

    // Test reverse lookup
    EXPECT_EQ(mapper.get_symbol(1), "BTCUSD");
    EXPECT_EQ(mapper.get_symbol(2), "ETHUSD");
    EXPECT_EQ(mapper.get_symbol(3), "SOLUSD");
}

TEST_F(ZeroCopyDecoderTest, SymbolMapperAddSymbol) {
    SymbolMapper mapper;

    mapper.add_symbol("ADAUSD", 99);
    EXPECT_EQ(mapper.get_id("ADAUSD"), 99U);
    EXPECT_EQ(mapper.get_symbol(99), "ADAUSD");
}

TEST_F(ZeroCopyDecoderTest, SymbolMapperUnknownSymbol) {
    SymbolMapper mapper;

    EXPECT_EQ(mapper.get_id("UNKNOWN"), 0U);
    EXPECT_EQ(mapper.get_symbol(999), "");
}

// Test packed structures don't have padding issues
TEST_F(ZeroCopyDecoderTest, PackedStructureAlignment) {
    // Verify structures are truly packed (no unexpected padding)
    BinaryOrderBookUpdate update;

    // Manually calculate expected offsets
    size_t expected_header_offset = 0;
    size_t expected_order_id_offset = expected_header_offset + sizeof(BinaryMessageHeader);
    size_t expected_symbol_id_offset = expected_order_id_offset + sizeof(uint64_t);
    size_t expected_side_offset = expected_symbol_id_offset + sizeof(uint32_t);
    size_t expected_update_type_offset = expected_side_offset + sizeof(uint8_t);
    size_t expected_padding_offset = expected_update_type_offset + sizeof(uint8_t);
    size_t expected_price_offset = expected_padding_offset + sizeof(uint16_t);
    size_t expected_quantity_offset = expected_price_offset + sizeof(double);

    // Verify offsets match expectations
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, header), expected_header_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, order_id), expected_order_id_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, symbol_id), expected_symbol_id_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, side), expected_side_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, update_type), expected_update_type_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, padding), expected_padding_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, price), expected_price_offset);
    EXPECT_EQ(offsetof(BinaryOrderBookUpdate, quantity), expected_quantity_offset);
}

} // namespace zerocopy
} // namespace hft