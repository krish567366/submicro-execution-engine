#include "fast_lob.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>

// Unit tests for Fast Limit Order Book (LOB)

void test_array_based_order_book_basic() {
    std::cout << "Testing ArrayBasedOrderBook basic operations..." << std::endl;

    hft::ArrayBasedOrderBook<10> book;

    // Test initial state
    auto best_bid = book.get_best_bid();
    auto best_ask = book.get_best_ask();
    assert(best_bid == nullptr);
    assert(best_ask == nullptr);

    // Add some bid levels
    book.update_bid(100.0, 10.0, 1);
    book.update_bid(99.5, 20.0, 2);
    book.update_bid(99.0, 15.0, 1);

    // Add some ask levels
    book.update_ask(100.5, 12.0, 1);
    book.update_ask(101.0, 25.0, 3);
    book.update_ask(101.5, 8.0, 1);

    // Test best bid/ask
    best_bid = book.get_best_bid();
    best_ask = book.get_best_ask();

    assert(best_bid != nullptr);
    assert(best_bid->price == 100.0);
    assert(best_bid->quantity == 10.0);

    assert(best_ask != nullptr);
    assert(best_ask->price == 100.5);
    assert(best_ask->quantity == 12.0);

    std::cout << "Basic operations passed" << std::endl;
}

void test_top_levels_retrieval() {
    std::cout << "Testing top levels retrieval..." << std::endl;

    hft::ArrayBasedOrderBook<20> book;

    // Add multiple bid levels
    book.update_bid(100.0, 10.0, 1);
    book.update_bid(99.5, 20.0, 2);
    book.update_bid(99.0, 15.0, 1);
    book.update_bid(98.5, 30.0, 3);
    book.update_bid(98.0, 25.0, 2);

    // Add multiple ask levels
    book.update_ask(100.5, 12.0, 1);
    book.update_ask(101.0, 25.0, 3);
    book.update_ask(101.5, 8.0, 1);
    book.update_ask(102.0, 18.0, 2);
    book.update_ask(102.5, 22.0, 4);

    std::vector<hft::FastPriceLevel> bids, asks;

    // Get top 3 bids
    book.get_top_bids(3, bids);
    assert(bids.size() == 3);
    assert(bids[0].price == 100.0);
    assert(bids[1].price == 99.5);
    assert(bids[2].price == 99.0);

    // Get top 3 asks
    book.get_top_asks(3, asks);
    assert(asks.size() == 3);
    assert(asks[0].price == 100.5);
    assert(asks[1].price == 101.0);
    assert(asks[2].price == 101.5);

    std::cout << "Top levels retrieval passed" << std::endl;
}

void test_level_updates() {
    std::cout << "Testing level updates..." << std::endl;

    hft::ArrayBasedOrderBook<10> book;

    // Add initial level
    book.update_bid(100.0, 10.0, 1);
    auto best_bid = book.get_best_bid();
    assert(best_bid->price == 100.0);
    assert(best_bid->quantity == 10.0);

    // Update quantity
    book.update_bid(100.0, 25.0, 2);
    best_bid = book.get_best_bid();
    assert(best_bid->price == 100.0);
    assert(best_bid->quantity == 25.0);

    // Remove level (quantity = 0)
    book.update_bid(100.0, 0.0, 0);
    best_bid = book.get_best_bid();
    assert(best_bid == nullptr);

    std::cout << "Level updates passed" << std::endl;
}

void test_fast_lob_reconstructor() {
    std::cout << "Testing FastLOBReconstructor..." << std::endl;

    hft::FastLOBReconstructor reconstructor("TEST");

    // Test ADD operations
    assert(reconstructor.process_update(1, 0, 1001, 100.0, 10.0, true));   // ADD BID
    assert(reconstructor.process_update(2, 0, 1002, 99.5, 20.0, true));    // ADD BID
    assert(reconstructor.process_update(3, 0, 2001, 100.5, 15.0, false));  // ADD ASK
    assert(reconstructor.process_update(4, 0, 2002, 101.0, 25.0, false));  // ADD ASK

    // Check BBO
    auto bbo = reconstructor.get_bbo();
    assert(bbo.first != nullptr && bbo.first->price == 100.0);
    assert(bbo.second != nullptr && bbo.second->price == 100.5);

    // Test MODIFY operation
    assert(reconstructor.process_update(5, 1, 1001, 99.8, 15.0, true));   // MODIFY BID

    bbo = reconstructor.get_bbo();
    assert(bbo.first != nullptr && bbo.first->price == 99.8);

    // Test EXECUTE operation
    assert(reconstructor.process_update(6, 3, 1002, 0.0, 10.0, true));    // EXECUTE BID

    // Test DELETE operation
    assert(reconstructor.process_update(7, 2, 2001, 0.0, 0.0, false));  // DELETE ASK

    std::cout << "FastLOBReconstructor passed" << std::endl;
}

void test_sequence_validation() {
    std::cout << "Testing sequence number validation..." << std::endl;

    hft::FastLOBReconstructor reconstructor("TEST");

    // Valid sequence
    assert(reconstructor.process_update(1, 0, 1001, 100.0, 10.0, true));

    // Gap in sequence - should fail
    assert(!reconstructor.process_update(3, 0, 1002, 99.5, 20.0, true));

    // Correct sequence - should work
    assert(reconstructor.process_update(2, 0, 1002, 99.5, 20.0, true));

    std::cout << "Sequence validation passed" << std::endl;
}

void test_order_aggregation() {
    std::cout << "Testing order aggregation at same price levels..." << std::endl;

    hft::FastLOBReconstructor reconstructor("TEST");

    // Add multiple orders at same bid price
    assert(reconstructor.process_update(1, 0, 1001, 100.0, 10.0, true));
    assert(reconstructor.process_update(2, 0, 1002, 100.0, 15.0, true));
    assert(reconstructor.process_update(3, 0, 1003, 100.0, 5.0, true));

    // Add multiple orders at same ask price
    assert(reconstructor.process_update(4, 0, 2001, 101.0, 12.0, false));
    assert(reconstructor.process_update(5, 0, 2002, 101.0, 8.0, false));

    // Check aggregated quantities
    auto bbo = reconstructor.get_bbo();
    assert(bbo.first != nullptr && bbo.first->quantity == 30.0);  // 10 + 15 + 5
    assert(bbo.second != nullptr && bbo.second->quantity == 20.0); // 12 + 8

    std::cout << "Order aggregation passed" << std::endl;
}

void test_edge_cases() {
    std::cout << "Testing edge cases..." << std::endl;

    hft::ArrayBasedOrderBook<5> book;

    // Test with zero quantity
    book.update_bid(100.0, 0.0, 0);
    assert(book.get_best_bid() == nullptr);

    // Test with negative quantity (should be treated as zero)
    book.update_ask(101.0, -5.0, 1);
    assert(book.get_best_ask() == nullptr);

    // Test clearing book
    book.update_bid(100.0, 10.0, 1);
    book.update_ask(101.0, 15.0, 1);
    assert(book.get_best_bid() != nullptr);
    assert(book.get_best_ask() != nullptr);

    // Note: clear() method not exposed in public interface
    // This is expected for the test

    std::cout << "Edge cases passed" << std::endl;
}

int main() {
    std::cout << "Running Fast LOB Unit Tests" << std::endl;
    std::cout << "============================" << std::endl;

    try {
        test_array_based_order_book_basic();
        test_top_levels_retrieval();
        test_level_updates();
        test_fast_lob_reconstructor();
        test_sequence_validation();
        test_order_aggregation();
        test_edge_cases();

        std::cout << std::endl;
        std::cout << "All fast LOB tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}