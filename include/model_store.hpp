#pragma once

#include "common_types.hpp"
#include <atomic>
#include <array>
#include <memory>

namespace hft {
namespace ml {

/**
 * Atomic Model Store (Double Buffering)
 * 
 * Purpose: Update ML model weights during live trading without locking the inference thread.
 * 
 * Mechanism:
 * - Two model buffers (Active, Idle).
 * - Reader gets pointer to Active.
 * - Writer updates Idle, then creates atomic memory fence, then swaps index.
 * 
 * Safety Limits:
 * - Single Reader (Strategy Thread).
 * - Single Writer (Training Thread).
 * - Reader must not hold pointer across ticks.
 */
template<typename ModelType>
class ModelStore {
public:
    ModelStore() {
        active_idx_.store(0);
    }

    // Reader: Get Reference to Hot Model
    // Valid only for current tick scope!
    inline ModelType& get_model() {
        // Relaxed load is fine if single reader thread always sees causal chain
        // Acquire ensures we see latest weights if changed
        int idx = active_idx_.load(std::memory_order_acquire);
        return models_[idx];
    }

    // Writer: Update logic
    template<typename F>
    void update_weights(F&& update_func) {
        int current = active_idx_.load(std::memory_order_relaxed);
        int next = 1 - current;
        
        // 1. Prepare Next Model
        update_func(models_[next]);
        
        // 2. Publish (Release barrier ensures weights are visible before index swap)
        active_idx_.store(next, std::memory_order_release);
    }

private:
    std::array<ModelType, 2> models_;
    alignas(64) std::atomic<int> active_idx_;
};

}
}