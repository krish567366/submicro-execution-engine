#pragma once

#include "common_types.hpp"
#include <type_traits>

namespace hft {
namespace dispatch {

/**
 * Compile-Time Strategy Dispatcher
 * 
 * Replacing 'virtual void on_tick()' with 'template<typename Strategy> void dispatch()'
 * eliminates indirect calls and branch mispredictions.
 */

/* Concept-like check (C++17) */
template<typename T>
struct is_strategy {
private:
    template<typename U> static auto test(int) -> decltype(std::declval<U>().on_tick(std::declval<const MarketTick&>()), std::true_type());
    template<typename> static std::false_type test(...);
public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

// Dispatcher Engine
template<typename... Strategies>
class StrategyDispatcher {
public:
    // tuple of strategies?
    // or just static methods? Assuming stateful instances.
    
    std::tuple<Strategies...> strategies_;

    template<size_t Index = 0>
    inline void on_tick(const MarketTick& tick) {
        // Unfold at compile time
        if constexpr (Index < sizeof...(Strategies)) {
            // Hot path inline call
            std::get<Index>(strategies_).on_tick(tick);
            
            // Recurse to next strategy
            on_tick<Index + 1>(tick);
        }
    }
    
    // Dispatch order updates
    template<size_t Index = 0>
    inline void on_order_update(const Order& order) {
        if constexpr (Index < sizeof...(Strategies)) {
            std::get<Index>(strategies_).on_order_update(order);
            on_order_update<Index + 1>(order);
        }
    }
};

}
}