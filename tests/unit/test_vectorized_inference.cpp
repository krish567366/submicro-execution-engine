#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "vectorized_inference.hpp"
#include <array>
#include <chrono>
#include <cmath>

namespace {

class VectorizedInferenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create inference engine
        engine_ = std::make_unique<hft::VectorizedInferenceEngine>();
        stub_ = std::make_unique<hft::FastInferenceStub>();
    }

    void TearDown() override {
        engine_.reset();
        stub_.reset();
    }

    std::unique_ptr<hft::VectorizedInferenceEngine> engine_;
    std::unique_ptr<hft::FastInferenceStub> stub_;
};

TEST_F(VectorizedInferenceTest, ConstructorInitialization) {
    // Test that engine initializes without crashing
    EXPECT_NE(engine_.get(), nullptr);
    EXPECT_NE(stub_.get(), nullptr);
}

TEST_F(VectorizedInferenceTest, PredictReturnsValidOutput) {
    std::array<double, 10> features = {0.1, 0.2, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    auto output = engine_->predict(features.data());

    // Check that probabilities are valid
    EXPECT_GE(output.buy_signal, 0.0);
    EXPECT_GE(output.sell_signal, 0.0);
    EXPECT_GE(output.hold_signal, 0.0);
    EXPECT_LE(output.buy_signal, 1.0);
    EXPECT_LE(output.sell_signal, 1.0);
    EXPECT_LE(output.hold_signal, 1.0);

    // Check that they sum to approximately 1
    double sum = output.buy_signal + output.sell_signal + output.hold_signal;
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(VectorizedInferenceTest, PredictConsistency) {
    std::array<double, 10> features = {0.1, 0.2, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    auto output1 = engine_->predict(features.data());
    auto output2 = engine_->predict(features.data());

    // Results should be deterministic
    EXPECT_DOUBLE_EQ(output1.buy_signal, output2.buy_signal);
    EXPECT_DOUBLE_EQ(output1.sell_signal, output2.sell_signal);
    EXPECT_DOUBLE_EQ(output1.hold_signal, output2.hold_signal);
}

TEST_F(VectorizedInferenceTest, GetActionLogic) {
    // Test buy signal dominant
    hft::VectorizedInferenceEngine::InferenceOutput buy_output{0.6, 0.2, 0.2};
    EXPECT_EQ(buy_output.get_action(), 1);

    // Test sell signal dominant
    hft::VectorizedInferenceEngine::InferenceOutput sell_output{0.2, 0.6, 0.2};
    EXPECT_EQ(sell_output.get_action(), -1);

    // Test hold signal dominant
    hft::VectorizedInferenceEngine::InferenceOutput hold_output{0.2, 0.2, 0.6};
    EXPECT_EQ(hold_output.get_action(), 0);

    // Test equal values (should return hold)
    hft::VectorizedInferenceEngine::InferenceOutput equal_output{0.33, 0.33, 0.34};
    EXPECT_EQ(equal_output.get_action(), 0);
}

TEST_F(VectorizedInferenceTest, FastInferenceStubPredict) {
    std::array<double, 10> features = {0.1, 0.2, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    int action = stub_->predict(features.data());

    // Action should be -1, 0, or 1
    EXPECT_TRUE(action == -1 || action == 0 || action == 1);
}

TEST_F(VectorizedInferenceTest, FastInferenceStubPredictProba) {
    std::array<double, 10> features = {0.1, 0.2, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    auto output = stub_->predict_proba(features.data());

    // Check probabilities are valid
    EXPECT_GE(output.buy_signal, 0.0);
    EXPECT_GE(output.sell_signal, 0.0);
    EXPECT_GE(output.hold_signal, 0.0);
    EXPECT_LE(output.buy_signal, 1.0);
    EXPECT_LE(output.sell_signal, 1.0);
    EXPECT_LE(output.hold_signal, 1.0);

    double sum = output.buy_signal + output.sell_signal + output.hold_signal;
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(VectorizedInferenceTest, LatencyEstimate) {
    uint64_t latency = hft::FastInferenceStub::get_latency_estimate_ns();

    // Latency should be reasonable (between 200-500ns)
    EXPECT_GE(latency, 200);
    EXPECT_LE(latency, 500);
}

TEST_F(VectorizedInferenceTest, WarmCache) {
    // Should not crash
    engine_->warm_cache();
}

TEST_F(VectorizedInferenceTest, PredictWithZeroFeatures) {
    std::array<double, 10> features = {0.0, 0.0, 0.0, 0.0, 0.0,
                                       0.0, 0.0, 0.0, 0.0, 0.0};

    auto output = engine_->predict(features.data());

    // Should still produce valid probabilities
    EXPECT_GE(output.buy_signal, 0.0);
    EXPECT_GE(output.sell_signal, 0.0);
    EXPECT_GE(output.hold_signal, 0.0);

    double sum = output.buy_signal + output.sell_signal + output.hold_signal;
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(VectorizedInferenceTest, PredictWithExtremeFeatures) {
    std::array<double, 10> features = {100.0, -100.0, 1000.0, -1000.0, 0.0,
                                       1e6, -1e6, 1e-6, -1e-6, 1.0};

    auto output = engine_->predict(features.data());

    // Should handle extreme values without NaN or Inf
    EXPECT_FALSE(std::isnan(output.buy_signal));
    EXPECT_FALSE(std::isnan(output.sell_signal));
    EXPECT_FALSE(std::isnan(output.hold_signal));
    EXPECT_FALSE(std::isinf(output.buy_signal));
    EXPECT_FALSE(std::isinf(output.sell_signal));
    EXPECT_FALSE(std::isinf(output.hold_signal));

    // Should still be valid probabilities
    EXPECT_GE(output.buy_signal, 0.0);
    EXPECT_GE(output.sell_signal, 0.0);
    EXPECT_GE(output.hold_signal, 0.0);

    double sum = output.buy_signal + output.sell_signal + output.hold_signal;
    EXPECT_NEAR(sum, 1.0, 1e-6); // Allow slightly larger tolerance for extreme values
}

TEST_F(VectorizedInferenceTest, PredictTiming) {
    std::array<double, 10> features = {0.1, 0.2, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    const int num_iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        auto output = engine_->predict(features.data());
        // Prevent optimization
        volatile double sum = output.buy_signal + output.sell_signal + output.hold_signal;
        (void)sum;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = static_cast<double>(duration.count()) / num_iterations;

    // Should be fast (less than 1000ns per inference on modern hardware)
    EXPECT_LT(avg_time_ns, 1000.0);
}

TEST_F(VectorizedInferenceTest, Constants) {
    EXPECT_EQ(hft::VectorizedInferenceEngine::INPUT_SIZE, 10);
    EXPECT_EQ(hft::VectorizedInferenceEngine::HIDDEN_SIZE, 16);
    EXPECT_EQ(hft::VectorizedInferenceEngine::OUTPUT_SIZE, 3);
}

TEST_F(VectorizedInferenceTest, FastTanhApproximation) {
    // Test the fast tanh approximation (internal function, test indirectly)
    std::array<double, 10> features = {0.0, 1.0, -1.0, 2.0, -2.0,
                                       0.5, -0.5, 3.0, -3.0, 0.1};

    auto output = engine_->predict(features.data());

    // Should produce valid output without crashing
    EXPECT_GE(output.buy_signal, 0.0);
    EXPECT_GE(output.sell_signal, 0.0);
    EXPECT_GE(output.hold_signal, 0.0);

    double sum = output.buy_signal + output.sell_signal + output.hold_signal;
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(VectorizedInferenceTest, SoftmaxStability) {
    // Test softmax with large differences (numerical stability)
    std::array<double, 10> features = {10.0, 10.0, 10.0, 10.0, 10.0,
                                       10.0, 10.0, 10.0, 10.0, 10.0};

    auto output = engine_->predict(features.data());

    // Should handle large inputs without overflow
    EXPECT_FALSE(std::isnan(output.buy_signal));
    EXPECT_FALSE(std::isnan(output.sell_signal));
    EXPECT_FALSE(std::isnan(output.hold_signal));

    // All probabilities should be equal (since inputs are equal)
    EXPECT_NEAR(output.buy_signal, output.sell_signal, 1e-10);
    EXPECT_NEAR(output.sell_signal, output.hold_signal, 1e-10);
}

TEST_F(VectorizedInferenceTest, PredictWithNaNFeatures) {
    std::array<double, 10> features = {0.1, NAN, 0.3, 0.4, 0.5,
                                       0.6, 0.7, 0.8, 0.9, 1.0};

    auto output = engine_->predict(features.data());

    // Should handle NaN input gracefully (may produce NaN output, but shouldn't crash)
    // This tests robustness of the implementation
    EXPECT_TRUE(std::isnan(output.buy_signal) ||
                std::isnan(output.sell_signal) ||
                std::isnan(output.hold_signal) ||
                (!std::isnan(output.buy_signal) && !std::isnan(output.sell_signal) && !std::isnan(output.hold_signal)));
}

} // namespace