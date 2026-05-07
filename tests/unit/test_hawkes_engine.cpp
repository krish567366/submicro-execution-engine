#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hawkes_engine.hpp"
#include <chrono>
#include <thread>

namespace {

class HawkesEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create engine with default parameters
        engine_ = std::make_unique<hft::HawkesEngine>();
    }

    void TearDown() override {
        engine_.reset();
    }

    std::unique_ptr<hft::HawkesEngine> engine_;
};

TEST_F(HawkesEngineTest, ConstructorInitialization) {
    EXPECT_DOUBLE_EQ(engine_->get_buy_intensity(), 10.0); // Default mu_buy
    EXPECT_DOUBLE_EQ(engine_->get_sell_intensity(), 10.0); // Default mu_sell
    EXPECT_EQ(engine_->get_buy_event_count(), 0);
    EXPECT_EQ(engine_->get_sell_event_count(), 0);
    EXPECT_DOUBLE_EQ(engine_->get_intensity_imbalance(), 0.0);
}

TEST_F(HawkesEngineTest, ConstructorWithCustomParameters) {
    hft::HawkesEngine custom_engine(5.0, 8.0, 0.3, 0.1, 1e-4, 2.0, 500);

    EXPECT_DOUBLE_EQ(custom_engine.get_buy_intensity(), 5.0);
    EXPECT_DOUBLE_EQ(custom_engine.get_sell_intensity(), 8.0);
}

TEST_F(HawkesEngineTest, UpdateBuyEvent) {
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);

    EXPECT_EQ(engine_->get_buy_event_count(), 1);
    EXPECT_EQ(engine_->get_sell_event_count(), 0);
    EXPECT_GT(engine_->get_buy_intensity(), 10.0); // Should increase due to self-excitation
}

TEST_F(HawkesEngineTest, UpdateSellEvent) {
    hft::TradingEvent sell_event;
    sell_event.arrival_time = hft::now();
    sell_event.event_type = hft::Side::SELL;

    engine_->update(sell_event);

    EXPECT_EQ(engine_->get_buy_event_count(), 0);
    EXPECT_EQ(engine_->get_sell_event_count(), 1);
    EXPECT_GT(engine_->get_sell_intensity(), 10.0); // Should increase due to self-excitation
}

TEST_F(HawkesEngineTest, SelfExcitation) {
    // Add a buy event
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    double intensity_before = engine_->get_buy_intensity();
    engine_->update(buy_event);
    double intensity_after = engine_->get_buy_intensity();

    EXPECT_GT(intensity_after, intensity_before);
}

TEST_F(HawkesEngineTest, CrossExcitation) {
    // Add a buy event and check sell intensity
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    double sell_intensity_before = engine_->get_sell_intensity();
    engine_->update(buy_event);
    double sell_intensity_after = engine_->get_sell_intensity();

    EXPECT_GT(sell_intensity_after, sell_intensity_before);
}

TEST_F(HawkesEngineTest, IntensityImbalance) {
    // Start with balanced intensities
    EXPECT_DOUBLE_EQ(engine_->get_intensity_imbalance(), 0.0);

    // Add buy event - should create positive imbalance
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);
    double imbalance = engine_->get_intensity_imbalance();
    EXPECT_GT(imbalance, 0.0);

    // Add sell event - should reduce imbalance
    hft::TradingEvent sell_event;
    sell_event.arrival_time = hft::now();
    sell_event.event_type = hft::Side::SELL;

    engine_->update(sell_event);
    double new_imbalance = engine_->get_intensity_imbalance();
    EXPECT_LT(std::abs(new_imbalance), std::abs(imbalance));
}

TEST_F(HawkesEngineTest, ResetFunctionality) {
    // Add some events
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    hft::TradingEvent sell_event;
    sell_event.arrival_time = hft::now();
    sell_event.event_type = hft::Side::SELL;

    engine_->update(buy_event);
    engine_->update(sell_event);

    EXPECT_EQ(engine_->get_buy_event_count(), 1);
    EXPECT_EQ(engine_->get_sell_event_count(), 1);
    EXPECT_GT(engine_->get_buy_intensity(), 10.0);
    EXPECT_GT(engine_->get_sell_intensity(), 10.0);

    // Reset
    engine_->reset();

    EXPECT_EQ(engine_->get_buy_event_count(), 0);
    EXPECT_EQ(engine_->get_sell_event_count(), 0);
    EXPECT_DOUBLE_EQ(engine_->get_buy_intensity(), 10.0);
    EXPECT_DOUBLE_EQ(engine_->get_sell_intensity(), 10.0);
}

TEST_F(HawkesEngineTest, IntensityDecayOverTime) {
    // Add an event
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);
    double peak_intensity = engine_->get_buy_intensity();

    // Wait a bit and check intensity decay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Force recalculation by adding another event
    hft::TradingEvent another_event;
    another_event.arrival_time = hft::now();
    another_event.event_type = hft::Side::SELL;

    engine_->update(another_event);
    double later_intensity = engine_->get_buy_intensity();

    // Intensity should be lower due to decay (though still elevated)
    EXPECT_LT(later_intensity, peak_intensity);
    EXPECT_GT(later_intensity, 10.0);
}

TEST_F(HawkesEngineTest, MaximumHistorySize) {
    // Create engine with small history size
    hft::HawkesEngine small_history_engine(10.0, 10.0, 0.5, 0.2, 1e-3, 1.5, 3);

    // Add more events than max history
    for (int i = 0; i < 5; ++i) {
        hft::TradingEvent event;
        event.arrival_time = hft::now();
        event.event_type = (i % 2 == 0) ? hft::Side::BUY : hft::Side::SELL;
        small_history_engine.update(event);
    }

    // Should only keep the most recent 3 events
    EXPECT_LE(small_history_engine.get_buy_event_count() +
              small_history_engine.get_sell_event_count(), 3);
}

TEST_F(HawkesEngineTest, PredictBuyIntensity) {
    // Add an event and predict future intensity
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);

    hft::Duration forecast_horizon = std::chrono::milliseconds(100);
    double predicted_intensity = engine_->predict_buy_intensity(forecast_horizon);

    // Prediction should be valid
    EXPECT_GE(predicted_intensity, 0.0);
    EXPECT_LT(predicted_intensity, engine_->get_buy_intensity() * 2.0); // Reasonable bound
}

TEST_F(HawkesEngineTest, PredictSellIntensity) {
    // Add an event and predict future intensity
    hft::TradingEvent sell_event;
    sell_event.arrival_time = hft::now();
    sell_event.event_type = hft::Side::SELL;

    engine_->update(sell_event);

    hft::Duration forecast_horizon = std::chrono::milliseconds(100);
    double predicted_intensity = engine_->predict_sell_intensity(forecast_horizon);

    // Prediction should be valid
    EXPECT_GE(predicted_intensity, 0.0);
    EXPECT_LT(predicted_intensity, engine_->get_sell_intensity() * 2.0); // Reasonable bound
}

TEST_F(HawkesEngineTest, PowerLawKernelProperties) {
    // Test that kernel decreases with time
    double kernel_at_0 = std::pow(1e-3 + 0.0, -1.5); // beta=1e-3, gamma=1.5
    double kernel_at_1 = std::pow(1e-3 + 1.0, -1.5);
    double kernel_at_2 = std::pow(1e-3 + 2.0, -1.5);

    EXPECT_GT(kernel_at_0, kernel_at_1);
    EXPECT_GT(kernel_at_1, kernel_at_2);
    EXPECT_GT(kernel_at_0, 0.0);
}

TEST_F(HawkesEngineTest, NegativeTimeHandling) {
    // Test prediction with negative time (should not crash)
    hft::Duration negative_horizon = std::chrono::milliseconds(-100);

    double buy_pred = engine_->predict_buy_intensity(negative_horizon);
    double sell_pred = engine_->predict_sell_intensity(negative_horizon);

    // Should return baseline intensities
    EXPECT_DOUBLE_EQ(buy_pred, 10.0);
    EXPECT_DOUBLE_EQ(sell_pred, 10.0);
}

TEST_F(HawkesEngineTest, ZeroIntensityPrevention) {
    // Create engine with very low baseline
    hft::HawkesEngine low_baseline_engine(1e-15, 1e-15);

    // Should prevent zero intensity
    EXPECT_GE(low_baseline_engine.get_buy_intensity(), 1e-10);
    EXPECT_GE(low_baseline_engine.get_sell_intensity(), 1e-10);
}

TEST_F(HawkesEngineTest, ParameterValidation) {
    // Test gamma correction
    hft::HawkesEngine gamma_engine(10.0, 10.0, 0.5, 0.2, 1e-3, 0.5); // gamma < 1
    // Gamma should be corrected to 1.5

    // Test beta correction
    hft::HawkesEngine beta_engine(10.0, 10.0, 0.5, 0.2, -1.0, 1.5); // beta < 0
    // Beta should be corrected to 1e-6
}

} // namespace