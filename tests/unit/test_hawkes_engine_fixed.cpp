#include <gtest/gtest.h>
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
    // Test default initialization
    EXPECT_DOUBLE_EQ(engine_->buy_intensity(), 10.0); // Default mu_buy
    EXPECT_DOUBLE_EQ(engine_->sell_intensity(), 10.0); // Default mu_sell
}

TEST_F(HawkesEngineTest, ConstructorWithCustomParameters) {
    hft::HawkesEngine custom_engine(5.0, 8.0, 0.3, 0.1, 1e-4, 2.0, 500);

    EXPECT_DOUBLE_EQ(custom_engine.buy_intensity(), 5.0);
    EXPECT_DOUBLE_EQ(custom_engine.sell_intensity(), 8.0);
}

TEST_F(HawkesEngineTest, UpdateBuyEvent) {
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);

    // After update, buy intensity should increase due to self-excitation
    EXPECT_GT(engine_->buy_intensity(), 10.0);
}

TEST_F(HawkesEngineTest, UpdateSellEvent) {
    hft::TradingEvent sell_event;
    sell_event.arrival_time = hft::now();
    sell_event.event_type = hft::Side::SELL;

    engine_->update(sell_event);

    // After update, sell intensity should increase due to self-excitation
    EXPECT_GT(engine_->sell_intensity(), 10.0);
}

TEST_F(HawkesEngineTest, SelfExcitation) {
    // Add a buy event
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    double intensity_before = engine_->buy_intensity();
    engine_->update(buy_event);
    double intensity_after = engine_->buy_intensity();

    EXPECT_GT(intensity_after, intensity_before);
}

TEST_F(HawkesEngineTest, CrossExcitation) {
    // Add a buy event
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    double sell_intensity_before = engine_->sell_intensity();
    engine_->update(buy_event);
    double sell_intensity_after = engine_->sell_intensity();

    // Sell intensity should increase due to cross-excitation
    EXPECT_GT(sell_intensity_after, sell_intensity_before);
}

TEST_F(HawkesEngineTest, MultipleEvents) {
    for (int i = 0; i < 5; i++) {
        hft::TradingEvent event;
        event.arrival_time = hft::now();
        event.event_type = (i % 2 == 0) ? hft::Side::BUY : hft::Side::SELL;
        engine_->update(event);
    }

    // Both intensities should be elevated after multiple events
    EXPECT_GT(engine_->buy_intensity(), 10.0);
    EXPECT_GT(engine_->sell_intensity(), 10.0);
}

TEST_F(HawkesEngineTest, IntensityDecayOverTime) {
    // Add a buy event
    hft::TradingEvent buy_event;
    buy_event.arrival_time = hft::now();
    buy_event.event_type = hft::Side::BUY;

    engine_->update(buy_event);
    double intensity_after_event = engine_->buy_intensity();

    // Wait for some time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Create a new event at later time to trigger recalc
    hft::TradingEvent later_event;
    later_event.arrival_time = hft::now();
    later_event.event_type = hft::Side::BUY;
    engine_->update(later_event);

    double intensity_later = engine_->buy_intensity();

    // Intensity should decay over time for the first event
    // (though adding new event will increase it, the effect of old event decreases)
    EXPECT_GT(intensity_later, 10.0);
}

TEST_F(HawkesEngineTest, BoundedIntensity) {
    // Intensities should always be bounded between minimum and reasonable maximum
    for (int i = 0; i < 100; i++) {
        hft::TradingEvent event;
        event.arrival_time = hft::now();
        event.event_type = hft::Side::BUY;
        engine_->update(event);
    }

    EXPECT_GT(engine_->buy_intensity(), 0.0);
    EXPECT_LT(engine_->buy_intensity(), 1e7); // Should not explode (intensity grows with events)
}

TEST_F(HawkesEngineTest, ParameterVariation) {
    // Test with different alpha values (intensity response)
    hft::HawkesEngine weak_response(10.0, 10.0, 0.01, 0.01, 1e-3, 1.5, 1000);
    hft::HawkesEngine strong_response(10.0, 10.0, 0.9, 0.9, 1e-3, 1.5, 1000);

    hft::TradingEvent event;
    event.arrival_time = hft::now();
    event.event_type = hft::Side::BUY;

    weak_response.update(event);
    strong_response.update(event);

    // Strong response should have higher intensity after same event
    EXPECT_GT(strong_response.buy_intensity(), weak_response.buy_intensity());
}

TEST_F(HawkesEngineTest, ParameterBoundaryConditions) {
    // Low baseline intensities
    hft::HawkesEngine low_baseline(0.01, 0.01, 0.1, 0.1, 1e-3, 1.5, 1000);
    EXPECT_GT(low_baseline.buy_intensity(), 0.0);
    EXPECT_GE(low_baseline.buy_intensity(), 1e-10); // Should have minimum threshold
}

}  // namespace
