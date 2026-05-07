#include <gtest/gtest.h>
#include "model_store.hpp"
#include <thread>
#include <chrono>
#include <filesystem>

// Test fixture for Model Store tests
class ModelStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file path for testing
        temp_config_path_ = "/tmp/test_model_store_config.json";
        store_ = std::make_unique<ModelStore>(temp_config_path_);
    }

    void TearDown() override {
        store_.reset();
        // Clean up temp file if it exists
        std::filesystem::remove(temp_config_path_);
    }

    std::string temp_config_path_;
    std::unique_ptr<ModelStore> store_;
};

// Test initialization
TEST_F(ModelStoreTest, Initialization) {
    EXPECT_TRUE(store_->initialize()); // Should succeed and load defaults
}

// Test default parameter loading
TEST_F(ModelStoreTest, DefaultParameters) {
    store_->initialize();

    // Test Hawkes parameters
    auto hawkes = store_->get_hawkes_parameters("default");
    ASSERT_TRUE(hawkes.has_value());
    EXPECT_DOUBLE_EQ(hawkes->alpha_self, 0.3);
    EXPECT_DOUBLE_EQ(hawkes->alpha_cross, 0.1);
    EXPECT_DOUBLE_EQ(hawkes->beta, 0.5);
    EXPECT_DOUBLE_EQ(hawkes->gamma, 2.0);
    EXPECT_DOUBLE_EQ(hawkes->lambda_base, 5.0);
    EXPECT_DOUBLE_EQ(hawkes->calibration_r_squared, 0.85);
    EXPECT_EQ(hawkes->calibration_samples, 1000000ULL);

    // Test Avellaneda-Stoikov parameters
    auto as_params = store_->get_as_parameters("default");
    ASSERT_TRUE(as_params.has_value());
    EXPECT_DOUBLE_EQ(as_params->gamma, 0.1);
    EXPECT_DOUBLE_EQ(as_params->sigma, 0.5);
    EXPECT_DOUBLE_EQ(as_params->kappa, 1.5);
    EXPECT_DOUBLE_EQ(as_params->time_horizon_seconds, 600.0);
    EXPECT_EQ(as_params->max_position, 1000);

    // Test risk parameters
    auto risk_params = store_->get_risk_parameters("default");
    ASSERT_TRUE(risk_params.has_value());
    EXPECT_EQ(risk_params->max_position, 1000);
    EXPECT_EQ(risk_params->position_limit_breach_threshold, 800);
    EXPECT_DOUBLE_EQ(risk_params->normal_volatility_threshold, 0.5);
    EXPECT_DOUBLE_EQ(risk_params->elevated_volatility_threshold, 1.0);
    EXPECT_DOUBLE_EQ(risk_params->high_stress_volatility_threshold, 2.0);
    EXPECT_DOUBLE_EQ(risk_params->normal_multiplier, 1.0);
    EXPECT_DOUBLE_EQ(risk_params->elevated_multiplier, 0.7);
    EXPECT_DOUBLE_EQ(risk_params->high_stress_multiplier, 0.4);
    EXPECT_DOUBLE_EQ(risk_params->halted_multiplier, 0.0);

    // Test inference parameters
    auto inference_params = store_->get_inference_parameters("default");
    ASSERT_TRUE(inference_params.has_value());
    EXPECT_EQ(inference_params->layer1_weights.size(), 8 * 16);
    EXPECT_EQ(inference_params->layer2_weights.size(), 16 * 8);
    EXPECT_EQ(inference_params->output_weights.size(), 8);
    EXPECT_EQ(inference_params->feature_means.size(), 8);
    EXPECT_EQ(inference_params->feature_stds.size(), 8);
    EXPECT_DOUBLE_EQ(inference_params->validation_accuracy, 0.75);
}

// Test parameter retrieval for non-existent symbols
TEST_F(ModelStoreTest, NonExistentParameters) {
    store_->initialize();

    auto hawkes = store_->get_hawkes_parameters("nonexistent");
    EXPECT_FALSE(hawkes.has_value());

    auto as_params = store_->get_as_parameters("nonexistent");
    EXPECT_FALSE(as_params.has_value());

    auto risk_params = store_->get_risk_parameters("nonexistent");
    EXPECT_FALSE(risk_params.has_value());

    auto inference_params = store_->get_inference_parameters("nonexistent");
    EXPECT_FALSE(inference_params.has_value());
}

// Test parameter updates
TEST_F(ModelStoreTest, ParameterUpdates) {
    store_->initialize();

    // Update Hawkes parameters
    HawkesParameters new_hawkes;
    new_hawkes.alpha_self = 0.4;
    new_hawkes.alpha_cross = 0.15;
    new_hawkes.beta = 0.7;
    new_hawkes.gamma = 2.2;
    new_hawkes.lambda_base = 7.0;
    new_hawkes.calibration_r_squared = 0.90;
    new_hawkes.calibration_samples = 2000000ULL;

    EXPECT_TRUE(store_->update_hawkes_parameters("test_symbol", new_hawkes, "test_user", "Updated for testing"));

    auto retrieved = store_->get_hawkes_parameters("test_symbol");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_DOUBLE_EQ(retrieved->alpha_self, 0.4);
    EXPECT_DOUBLE_EQ(retrieved->alpha_cross, 0.15);
    EXPECT_DOUBLE_EQ(retrieved->beta, 0.7);
    EXPECT_DOUBLE_EQ(retrieved->gamma, 2.2);
    EXPECT_DOUBLE_EQ(retrieved->lambda_base, 7.0);
    EXPECT_DOUBLE_EQ(retrieved->calibration_r_squared, 0.90);
    EXPECT_EQ(retrieved->calibration_samples, 2000000ULL);
    EXPECT_EQ(retrieved->version.updated_by, "test_user");
    EXPECT_EQ(retrieved->version.comment, "Updated for testing");
    EXPECT_EQ(retrieved->version.version_id, 1ULL); // Should be version 1

    // Update Avellaneda-Stoikov parameters
    AvellanedaStoikovParameters new_as;
    new_as.gamma = 0.2;
    new_as.sigma = 0.7;
    new_as.kappa = 2.0;
    new_as.time_horizon_seconds = 800.0;
    new_as.max_position = 1500;
    new_as.backtest_sharpe = 3.0;
    new_as.backtest_pnl = 200000.0;

    EXPECT_TRUE(store_->update_as_parameters("test_symbol", new_as, "test_user", "AS parameters updated"));

    auto retrieved_as = store_->get_as_parameters("test_symbol");
    ASSERT_TRUE(retrieved_as.has_value());
    EXPECT_DOUBLE_EQ(retrieved_as->gamma, 0.2);
    EXPECT_DOUBLE_EQ(retrieved_as->sigma, 0.7);
    EXPECT_DOUBLE_EQ(retrieved_as->kappa, 2.0);
    EXPECT_DOUBLE_EQ(retrieved_as->time_horizon_seconds, 800.0);
    EXPECT_EQ(retrieved_as->max_position, 1500);
    EXPECT_DOUBLE_EQ(retrieved_as->backtest_sharpe, 3.0);
    EXPECT_DOUBLE_EQ(retrieved_as->backtest_pnl, 200000.0);
    EXPECT_EQ(retrieved_as->version.version_id, 2ULL); // Should be version 2
}

// Test calibration quality tracking
TEST_F(ModelStoreTest, CalibrationQuality) {
    store_->initialize();

    // Add some test parameters
    HawkesParameters hawkes;
    hawkes.calibration_r_squared = 0.88;
    hawkes.version.updated_at = 1000000000LL; // 1 second since epoch
    hawkes.version.version_id = 5;

    AvellanedaStoikovParameters as_params;
    as_params.backtest_sharpe = 2.8;
    as_params.version.version_id = 6;

    store_->update_hawkes_parameters("quality_test", hawkes, "system", "Quality test");
    store_->update_as_parameters("quality_test", as_params, "system", "Quality test");

    auto quality_metrics = store_->get_calibration_quality();
    ASSERT_FALSE(quality_metrics.empty());

    // Find our test symbol
    auto it = std::find_if(quality_metrics.begin(), quality_metrics.end(),
                          [](const CalibrationQuality& q) { return q.symbol == "quality_test"; });
    ASSERT_NE(it, quality_metrics.end());

    EXPECT_DOUBLE_EQ(it->hawkes_r_squared, 0.88);
    EXPECT_DOUBLE_EQ(it->as_sharpe, 2.8);
    EXPECT_EQ(it->version_id, 1ULL);
}

// Test recalibration checks
TEST_F(ModelStoreTest, RecalibrationChecks) {
    store_->initialize();

    // Test with default parameters (should NOT need recalibration - they are fresh)
    EXPECT_FALSE(store_->needs_recalibration("default"));

    // Test with non-existent symbol
    EXPECT_TRUE(store_->needs_recalibration("nonexistent"));

    // Add fresh parameters
    HawkesParameters fresh_hawkes;
    fresh_hawkes.version.updated_at = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count(); // Current time
    fresh_hawkes.version.version_id = 10;

    store_->update_hawkes_parameters("fresh", fresh_hawkes, "system", "Fresh parameters");

    // Should not need recalibration (very fresh)
    EXPECT_FALSE(store_->needs_recalibration("fresh", 3600)); // 1 hour threshold

    // Should still not need recalibration with very short threshold (age is ~0)
    EXPECT_FALSE(store_->needs_recalibration("fresh", 0)); // 0 seconds threshold
}

// Test version tracking
TEST_F(ModelStoreTest, VersionTracking) {
    store_->initialize();

    // First update
    HawkesParameters params1;
    store_->update_hawkes_parameters("version_test", params1, "user1", "First update");
    auto retrieved1 = store_->get_hawkes_parameters("version_test");
    ASSERT_TRUE(retrieved1.has_value());
    EXPECT_EQ(retrieved1->version.version_id, 1ULL);
    EXPECT_EQ(retrieved1->version.updated_by, "user1");
    EXPECT_EQ(retrieved1->version.comment, "First update");

    // Second update
    HawkesParameters params2;
    store_->update_hawkes_parameters("version_test", params2, "user2", "Second update");
    auto retrieved2 = store_->get_hawkes_parameters("version_test");
    ASSERT_TRUE(retrieved2.has_value());
    EXPECT_EQ(retrieved2->version.version_id, 2ULL);
    EXPECT_EQ(retrieved2->version.updated_by, "user2");
    EXPECT_EQ(retrieved2->version.comment, "Second update");

    // Versions should be different
    EXPECT_LT(retrieved1->version.version_id, retrieved2->version.version_id);
}

// Test thread safety (basic concurrent access)
TEST_F(ModelStoreTest, ThreadSafety) {
    store_->initialize();

    std::atomic<bool> ready(false);
    std::atomic<int> errors(0);

    // Add timing measurement
    auto start_time = std::chrono::high_resolution_clock::now();

    auto reader = [&]() {
        while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));

        for (int i = 0; i < 1000; ++i) {  // Increased to 1000 operations
            auto params = store_->get_hawkes_parameters("default");
            if (!params.has_value()) {
                errors++;
            }
            // Add some artificial work
            volatile double work = 0.0;
            for (int j = 0; j < 100; ++j) {
                work += std::sin(j) * std::cos(j);
            }
        }
    };

    auto writer = [&]() {
        while (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));

        for (int i = 0; i < 1000; ++i) {  // Increased to 1000 operations
            HawkesParameters params;
            params.alpha_self = 0.1 + i * 0.001; // Slightly different each time
            if (!store_->update_hawkes_parameters("thread_test", params, "thread", "Concurrent update")) {
                errors++;
            }
            // Add some artificial work
            volatile double work = 0.0;
            for (int j = 0; j < 100; ++j) {
                work += std::sqrt(j) * std::log(j + 1);
            }
        }
    };

    std::thread reader_thread(reader);
    std::thread writer_thread(writer);

    ready = true;

    reader_thread.join();
    writer_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "ThreadSafety test took: " << duration.count() << " ms (with 1000 operations each + artificial work)" << std::endl;

    EXPECT_EQ(errors, 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}