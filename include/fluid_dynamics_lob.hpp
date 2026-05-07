#pragma once

#include "common_types.hpp"
#include <array>
#include <cmath>

namespace hft {
namespace physics {

/**
 * Navier-Stokes Order Flow (Fluid Dynamics Model)
 * 
 * Concept (Visionary):
 * Treat Liquidity as an Incompressible Fluid.
 * Price is a particle moving through this fluid.
 * 
 * Equations:
 * 1. Pressure Field P(x): Density of Orders at price x.
 * 2. Velocity Field v(x): Rate of inserts/cancels at price x.
 * 3. Viscosity: Market friction (Fees + Latency).
 * 
 * Bernoulli's Principle in Trading:
 * "Where speed (velocity of cancels) increases, pressure (liquidity) decreases."
 * -> High Cancel Rate = Low Real Liquidity = Price moves easily through it.
 */
class FluidDynamicsLOB {
public:
    static constexpr int GRID_SIZE = 128; // Levels around mid
    
    struct FluidCell {
        double density;   // Volume at level
        double velocity;  // Change in volume (flow)
        double pressure;  // Derived metric
    };

    FluidDynamicsLOB() {
        // Init grid
    }

    // Update the fluid state with new L2 data
    inline void update_flow(int level_idx, double new_volume, double dt_sec) {
        if (level_idx < 0 || level_idx >= GRID_SIZE) return;
        
        FluidCell& cell = grid_[level_idx];
        
        // Calculate Velocity (Flux)
        double d_vol = new_volume - cell.density;
        cell.velocity = d_vol / dt_sec;
        
        // Update Density
        cell.density = new_volume;
        
        // Calculate Pressure
        // Equation of State: P = rho * c^2 (Ideal gas?)
        // Let's use simpler: P = Density / Volatility? 
        // No, Pressure = Resistance to price movement.
        // P ~ Density.
        cell.pressure = cell.density; 
    }

    /**
     * Compute "Reynolds Number" of the Order Book
     * 
     * Laminar Flow (Re_low): Orderly market, trend following involves minimal turbulence.
     * Turbulent Flow (Re_high): Chaotic fills, high slippage risk.
     * 
     * Re = (Density * Velocity * Length) / Viscosity
     */
    inline double compute_reynolds_number(double spread_width) {
        double max_v = 0.0;
        double avg_rho = 0.0;
        
        for (int i=0; i<GRID_SIZE; ++i) {
            max_v = std::max(max_v, std::abs(grid_[i].velocity));
            avg_rho += grid_[i].density;
        }
        avg_rho /= GRID_SIZE;
        
        if (avg_rho < 1e-5) return 0.0;
        
        double viscosity = 0.01; // Constant representing fees
        
        return (avg_rho * max_v * spread_width) / viscosity;
    }

    // Gradient Descent Prediction
    // Price moves towards path of Least Resistance (Lowest Pressure Gradient)
    inline double predict_breakout_direction() {
        double bid_pressure = 0;
        double ask_pressure = 0;
        
        // Integrate pressure
        for (int i=0; i<GRID_SIZE/2; ++i) bid_pressure += grid_[i].pressure;
        for (int i=GRID_SIZE/2; i<GRID_SIZE; ++i) ask_pressure += grid_[i].pressure;
        
        // Return normalized vector [-1, 1]
        return (bid_pressure - ask_pressure) / (bid_pressure + ask_pressure);
    }

private:
    std::array<FluidCell, GRID_SIZE> grid_;
};

}
}
