#include <gtest/gtest.h>
#include "shared_memory.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

namespace {

// Test fixture for shared memory tests
class SharedMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test segments
        shm_unlink("test_ring_buffer");
        shm_unlink("test_market_data");
    }

    void TearDown() override {
        // Clean up test segments
        shm_unlink("test_ring_buffer");
        shm_unlink("test_market_data");
    }
};

// Test basic SharedMemoryRingBuffer creation and destruction
TEST_F(SharedMemoryTest, SharedMemoryRingBufferBasic) {
    // Test creating a ring buffer
    hft::shm::SharedMemoryRingBuffer<int, 64> buffer("test_ring_buffer", true);

    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
}

// Test SharedMemoryRingBuffer producer-consumer basic functionality
TEST_F(SharedMemoryTest, SharedMemoryRingBufferBasicReadWrite) {
    // Create producer
    hft::shm::SharedMemoryRingBuffer<int, 64> producer("test_ring_buffer", true);

    // Attach consumer
    hft::shm::SharedMemoryRingBuffer<int, 64> consumer("test_ring_buffer", false);

    // Producer writes data
    EXPECT_TRUE(producer.write(42));
    EXPECT_EQ(producer.size(), 1);
    EXPECT_FALSE(producer.empty());

    // Consumer reads data
    int value;
    EXPECT_TRUE(consumer.read(value));
    EXPECT_EQ(value, 42);
    EXPECT_EQ(consumer.size(), 0);
    EXPECT_TRUE(consumer.empty());
}

// Test SharedMemoryRingBuffer capacity limits
TEST_F(SharedMemoryTest, SharedMemoryRingBufferCapacity) {
    hft::shm::SharedMemoryRingBuffer<int, 4> buffer("test_ring_buffer", true);

    // Fill buffer
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.write(i));
    }

    EXPECT_EQ(buffer.size(), 4);
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.empty());

    // Try to write to full buffer
    EXPECT_FALSE(buffer.write(999));

    // Read one item
    int value;
    EXPECT_TRUE(buffer.read(value));
    EXPECT_EQ(value, 0);
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_FALSE(buffer.full());

    // Should be able to write again
    EXPECT_TRUE(buffer.write(999));
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_TRUE(buffer.full());
}

// Test SharedMemoryRingBuffer wraparound
TEST_F(SharedMemoryTest, SharedMemoryRingBufferWraparound) {
    hft::shm::SharedMemoryRingBuffer<int, 4> buffer("test_ring_buffer", true);

    // Fill and empty buffer multiple times to test wraparound
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Fill buffer
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(buffer.write(cycle * 10 + i));
        }
        EXPECT_TRUE(buffer.full());

        // Empty buffer
        for (int i = 0; i < 4; ++i) {
            int value;
            EXPECT_TRUE(buffer.read(value));
            EXPECT_EQ(value, cycle * 10 + i);
        }
        EXPECT_TRUE(buffer.empty());
    }
}

// Test SharedMemoryRingBuffer with complex data (MarketTick)
TEST_F(SharedMemoryTest, SharedMemoryRingBufferMarketTick) {
    hft::shm::SharedMemoryRingBuffer<hft::MarketTick, 64> producer("test_market_data", true);
    hft::shm::SharedMemoryRingBuffer<hft::MarketTick, 64> consumer("test_market_data", false);

    // Create test market tick
    hft::MarketTick tick;
    tick.timestamp = hft::now();
    tick.bid_price = 50000.0;
    tick.ask_price = 50001.0;
    tick.trade_volume = 100;

    // Producer writes tick
    EXPECT_TRUE(producer.write(tick));

    // Consumer reads tick
    hft::MarketTick received_tick;
    EXPECT_TRUE(consumer.read(received_tick));

    EXPECT_EQ(received_tick.timestamp, tick.timestamp);
    EXPECT_EQ(received_tick.bid_price, tick.bid_price);
    EXPECT_EQ(received_tick.ask_price, tick.ask_price);
    EXPECT_EQ(received_tick.trade_volume, tick.trade_volume);
}

// Test SharedMemoryRingBuffer thread safety (producer-consumer pattern)
TEST_F(SharedMemoryTest, SharedMemoryRingBufferThreadSafety) {
    hft::shm::SharedMemoryRingBuffer<int, 1024> buffer("test_ring_buffer", true);

    std::atomic<bool> producer_done(false);
    std::atomic<int> consumed_count(0);
    const int total_items = 1000;

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            while (!buffer.write(i)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        int expected_value = 0;
        while (true) {
            int value;
            if (buffer.read(value)) {
                EXPECT_EQ(value, expected_value++);
                consumed_count.fetch_add(1, std::memory_order_relaxed);
            } else if (producer_done.load(std::memory_order_acquire) && buffer.empty()) {
                break;
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed_count.load(), total_items);
}

// Test SharedMemoryRingBuffer performance
TEST_F(SharedMemoryTest, SharedMemoryRingBufferPerformance) {
    hft::shm::SharedMemoryRingBuffer<int, 1024> buffer("test_ring_buffer", true);

    const int iterations = 1000;

    // Measure write performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        EXPECT_TRUE(buffer.write(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Measure read performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        int value;
        EXPECT_TRUE(buffer.read(value));
        EXPECT_EQ(value, i);
    }
    end = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Performance should be reasonable (less than 10 microseconds per operation on average)
    EXPECT_LT(write_duration.count() / static_cast<double>(iterations), 10.0);
    EXPECT_LT(read_duration.count() / static_cast<double>(iterations), 10.0);
}

// Test SharedMemoryRingBuffer error conditions
TEST_F(SharedMemoryTest, SharedMemoryRingBufferErrors) {
    // Test attaching to non-existent segment
    try {
        hft::shm::SharedMemoryRingBuffer<int, 64> buffer("non_existent_segment", false);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error&) {
        // Expected
    } catch (...) {
        FAIL() << "Expected std::runtime_error";
    }

    // Test creating segment that already exists (should handle gracefully)
    {
        hft::shm::SharedMemoryRingBuffer<int, 64> first("test_ring_buffer", true);
        // Second creation should work (implementation handles existing segments)
        hft::shm::SharedMemoryRingBuffer<int, 64> second("test_ring_buffer", true);
    }
}

// Test SharedMarketDataQueue typedef
TEST_F(SharedMemoryTest, SharedMarketDataQueue) {
    hft::shm::SharedMarketDataQueue producer("test_market_data", true);
    hft::shm::SharedMarketDataQueue consumer("test_market_data", false);

    hft::MarketTick tick;
    tick.timestamp = hft::now();
    tick.bid_price = 45000.0;
    tick.ask_price = 45001.0;
    tick.trade_volume = 50;

    EXPECT_TRUE(producer.write(tick));

    hft::MarketTick received;
    EXPECT_TRUE(consumer.read(received));

    EXPECT_EQ(received.bid_price, tick.bid_price);
    EXPECT_EQ(received.ask_price, tick.ask_price);
    EXPECT_EQ(received.trade_volume, tick.trade_volume);
}

// Test multiple consumers (not officially supported but should not crash)
TEST_F(SharedMemoryTest, SharedMemoryRingBufferMultipleConsumers) {
    hft::shm::SharedMemoryRingBuffer<int, 64> producer("test_ring_buffer", true);
    hft::shm::SharedMemoryRingBuffer<int, 64> consumer1("test_ring_buffer", false);
    hft::shm::SharedMemoryRingBuffer<int, 64> consumer2("test_ring_buffer", false);

    // Write one item
    EXPECT_TRUE(producer.write(123));

    // Both consumers can read (race condition, but should not crash)
    int value1, value2;
    bool read1 = consumer1.read(value1);
    bool read2 = consumer2.read(value2);

    // At least one should succeed
    EXPECT_TRUE(read1 || read2);

    if (read1) EXPECT_EQ(value1, 123);
    if (read2) EXPECT_EQ(value2, 123);
}

// Test edge cases
TEST_F(SharedMemoryTest, SharedMemoryRingBufferEdgeCases) {
    hft::shm::SharedMemoryRingBuffer<int, 2> buffer("test_ring_buffer", true);

    // Test empty read
    int dummy;
    EXPECT_FALSE(buffer.read(dummy));

    // Test single item operations
    EXPECT_TRUE(buffer.write(1));
    EXPECT_TRUE(buffer.read(dummy));
    EXPECT_EQ(dummy, 1);

    // Test alternating operations
    EXPECT_TRUE(buffer.write(2));
    EXPECT_TRUE(buffer.write(3));
    EXPECT_TRUE(buffer.full());

    EXPECT_TRUE(buffer.read(dummy));
    EXPECT_EQ(dummy, 2);
    EXPECT_TRUE(buffer.read(dummy));
    EXPECT_EQ(dummy, 3);
    EXPECT_TRUE(buffer.empty());
}

} // namespace