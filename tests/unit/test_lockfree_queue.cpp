#include "lockfree_queue.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>

// Unit tests for Lock-Free Queue (SPSC Queue)

void test_basic_operations() {
    std::cout << "Testing basic SPSC queue operations..." << std::endl;

    hft::SPSCQueue<int, 8> queue;

    // Test empty queue
    assert(queue.empty());
    assert(queue.size() == 0);

    // Test push and pop
    int value;
    assert(queue.push(42));
    assert(!queue.empty());
    assert(queue.size() == 1);

    assert(queue.pop(value));
    assert(value == 42);
    assert(queue.empty());
    assert(queue.size() == 0);

    std::cout << "Basic operations passed" << std::endl;
}

void test_capacity_limits() {
    std::cout << "Testing capacity limits..." << std::endl;

    hft::SPSCQueue<int, 4> queue; // Capacity = 3 (power of 2 - 1)

    // Fill the queue
    assert(queue.push(1));
    assert(queue.push(2));
    assert(queue.push(3));
    assert(queue.size() == 3);

    // Queue should be full now
    assert(!queue.push(4)); // Should fail
    assert(queue.size() == 3);

    // Empty the queue
    int value;
    assert(queue.pop(value)); assert(value == 1);
    assert(queue.pop(value)); assert(value == 2);
    assert(queue.pop(value)); assert(value == 3);
    assert(queue.empty());

    std::cout << "Capacity limits passed" << std::endl;
}

void test_fifo_order() {
    std::cout << "Testing FIFO ordering..." << std::endl;

    hft::SPSCQueue<int, 8> queue;

    // Push values in order
    for (int i = 1; i <= 5; ++i) {
        assert(queue.push(i));
    }

    // Pop values and verify order
    int value;
    for (int i = 1; i <= 5; ++i) {
        assert(queue.pop(value));
        assert(value == i);
    }

    assert(queue.empty());

    std::cout << "FIFO ordering passed" << std::endl;
}

void test_empty_pop() {
    std::cout << "Testing empty queue pop..." << std::endl;

    hft::SPSCQueue<int, 8> queue;

    int value;
    assert(!queue.pop(value)); // Should fail on empty queue

    std::cout << "Empty queue pop passed" << std::endl;
}

void test_wraparound() {
    std::cout << "Testing buffer wraparound..." << std::endl;

    hft::SPSCQueue<int, 4> queue; // Small buffer to test wraparound

    // Fill and partially empty to test wraparound
    assert(queue.push(1));
    assert(queue.push(2));
    assert(queue.push(3));

    int value;
    assert(queue.pop(value)); assert(value == 1);
    assert(queue.pop(value)); assert(value == 2);

    // Now push more to test wraparound
    assert(queue.push(4));
    assert(queue.push(5));

    assert(queue.pop(value)); assert(value == 3);
    assert(queue.pop(value)); assert(value == 4);
    assert(queue.pop(value)); assert(value == 5);

    assert(queue.empty());

    std::cout << "Buffer wraparound passed" << std::endl;
}

void test_complex_types() {
    std::cout << "Testing complex data types..." << std::endl;

    struct TestStruct {
        int a;
        double b;
        bool operator==(const TestStruct& other) const {
            return a == other.a && b == other.b;
        }
    };

    hft::SPSCQueue<TestStruct, 8> queue;

    TestStruct input{42, 3.14};
    TestStruct output;

    assert(queue.push(input));
    assert(queue.pop(output));
    assert(input == output);

    std::cout << "Complex data types passed" << std::endl;
}

void test_concurrent_access() {
    std::cout << "Testing concurrent producer-consumer access..." << std::endl;

    hft::SPSCQueue<int, 1024> queue;
    const int num_items = 10000;

    std::vector<int> produced;
    std::vector<int> consumed;

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < num_items; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield(); // Wait for space
            }
            produced.push_back(i);
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        for (int i = 0; i < num_items; ++i) {
            int value;
            while (!queue.pop(value)) {
                std::this_thread::yield(); // Wait for data
            }
            consumed.push_back(value);
        }
    });

    producer.join();
    consumer.join();

    // Verify all items were transferred correctly
    assert(produced.size() == num_items);
    assert(consumed.size() == num_items);
    assert(produced == consumed);

    std::cout << "Concurrent access passed" << std::endl;
}

int main() {
    std::cout << "Running Lock-Free Queue Unit Tests" << std::endl;
    std::cout << "===================================" << std::endl;

    try {
        test_basic_operations();
        test_capacity_limits();
        test_fifo_order();
        test_empty_pop();
        test_wraparound();
        test_complex_types();
        test_concurrent_access();

        std::cout << std::endl;
        std::cout << " All lock-free queue tests passed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << " Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << " Test failed with unknown exception" << std::endl;
        return 1;
    }
}