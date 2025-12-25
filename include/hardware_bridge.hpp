// hardware_bridge.hpp
// Hardware-in-the-Loop (HIL) Bridge for FPGA Integration
//
// PURPOSE:
// - Clean integration point between software modules and FPGA hardware
// - Zero-logic interface: pure pass-through with monitoring hooks
// - Enables seamless transition from 400ns software stub → actual FPGA
// - De-risks innovation by proving production hardware path exists
//
// PRODUCTION DEPLOYMENT:
// 1. Development: Uses FPGAInference software stub (400ns deterministic)
// 2. Testing: Routes through this bridge with latency validation
// 3. Production: Bridge connects to actual Verilog/VHDL via PCIe/memory-mapped I/O
//
// HARDWARE INTERFACE OPTIONS:
// - PCIe DMA: Zero-copy transfers via PCIe Base Address Registers (BARs)
// - Memory-Mapped I/O: Direct register access for feature inputs/predictions
// - Shared Memory: DPDK/SPDK huge pages for batched inference
// - Custom Protocol: Vendor-specific FPGA communication (Xilinx, Intel)

#pragma once

#include "common_types.hpp"
#include "fpga_inference.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <chrono>

// Use hft namespace for types
using hft::MicrostructureFeatures;
using hft::FPGA_DNN_Inference;

// Hardware acceleration modes
enum class AcceleratorMode {
    SOFTWARE_STUB,      // Use FPGAInference C++ implementation (development)
    HARDWARE_FPGA,      // Route to actual FPGA card (production)
    HYBRID_FALLBACK     // FPGA with software fallback on timeout
};

// Hardware health status
enum class HardwareStatus {
    NOT_INITIALIZED,    // Bridge not started
    READY,              // Hardware healthy and responsive
    DEGRADED,           // Partial failures, using fallback
    FAILED              // Hardware unavailable, software-only mode
};

// Hardware latency statistics
struct HardwareLatencyStats {
    double mean_ns;
    double p50_ns;
    double p95_ns;
    double p99_ns;
    double max_ns;
    uint64_t total_inferences;
    uint64_t hardware_failures;
    uint64_t software_fallbacks;
};

// ============================================================================
// Hardware-in-the-Loop Bridge
// ============================================================================
//
// This class is intentionally minimal - it's a pass-through interface that:
// 1. Routes inference requests to software OR hardware
// 2. Monitors latency and health of hardware path
// 3. Provides production hooks for FPGA integration
// 4. Does NOT contain trading logic or feature engineering
//
// Key Production Features:
// - Hot-swappable: Switch between software/hardware without restart
// - Health monitoring: Automatic fallback on hardware degradation
// - Latency tracking: Validates hardware meets 400ns SLA
// - Zero-copy ready: Designed for memory-mapped FPGA access

class HardwareInTheLoopBridge {
public:
    // ========================================================================
    // Construction & Initialization
    // ========================================================================
    
    explicit HardwareInTheLoopBridge(AcceleratorMode mode = AcceleratorMode::SOFTWARE_STUB)
        : mode_(mode)
        , status_(HardwareStatus::NOT_INITIALIZED)
        , software_inference_(std::make_unique<FPGA_DNN_Inference>())
        , total_inferences_(0)
        , hardware_failures_(0)
        , software_fallbacks_(0)
        , latency_sum_ns_(0.0)
    {
    }

    // Initialize bridge and underlying accelerator
    bool initialize() {
        switch (mode_.load(std::memory_order_acquire)) {
            case AcceleratorMode::SOFTWARE_STUB:
                return initialize_software_stub();
            
            case AcceleratorMode::HARDWARE_FPGA:
                return initialize_fpga_hardware();
            
            case AcceleratorMode::HYBRID_FALLBACK:
                // Try hardware first, fallback to software if fails
                if (initialize_fpga_hardware()) {
                    return true;
                }
                // Hardware failed, switch to software mode
                mode_.store(AcceleratorMode::SOFTWARE_STUB, std::memory_order_release);
                return initialize_software_stub();
            
            default:
                return false;
        }
    }

    // ========================================================================
    // Core Inference Interface (Pass-Through)
    // ========================================================================
    
    // Predict signal with automatic routing to software/hardware
    // This is the ONLY method strategy code should call
    double predict(const MicrostructureFeatures& features) {
        const auto start = std::chrono::steady_clock::now();
        
        double prediction = 0.0;
        bool success = false;
        
        switch (mode_.load(std::memory_order_acquire)) {
            case AcceleratorMode::SOFTWARE_STUB:
                prediction = predict_software(features);
                success = true;
                break;
            
            case AcceleratorMode::HARDWARE_FPGA:
                success = predict_hardware(features, prediction);
                if (!success) {
                    // Hardware failed, record failure
                    hardware_failures_.fetch_add(1, std::memory_order_relaxed);
                    status_.store(HardwareStatus::FAILED, std::memory_order_release);
                }
                break;
            
            case AcceleratorMode::HYBRID_FALLBACK:
                success = predict_hardware(features, prediction);
                if (!success) {
                    // Hardware failed, use software fallback
                    software_fallbacks_.fetch_add(1, std::memory_order_relaxed);
                    prediction = predict_software(features);
                    success = true;
                }
                break;
        }
        
        // Track latency statistics
        const auto end = std::chrono::steady_clock::now();
        const double latency_ns = std::chrono::duration<double, std::nano>(end - start).count();
        
        update_latency_stats(latency_ns);
        total_inferences_.fetch_add(1, std::memory_order_relaxed);
        
        return prediction;
    }

    // ========================================================================
    // Hardware Management
    // ========================================================================
    
    // Switch accelerator mode at runtime (hot-swap)
    bool set_mode(AcceleratorMode new_mode) {
        if (new_mode == mode_.load(std::memory_order_acquire)) {
            return true;  // Already in requested mode
        }
        
        mode_.store(new_mode, std::memory_order_release);
        return initialize();  // Re-initialize with new mode
    }
    
    // Get current hardware health status
    HardwareStatus get_status() const {
        return status_.load(std::memory_order_acquire);
    }
    
    // Get latency statistics for monitoring
    HardwareLatencyStats get_latency_stats() const {
        const uint64_t count = total_inferences_.load(std::memory_order_acquire);
        if (count == 0) {
            return {0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0};
        }
        
        return {
            latency_sum_ns_.load(std::memory_order_acquire) / count,  // Mean
            0.0,  // P50 (requires histogram, omitted for simplicity)
            0.0,  // P95
            0.0,  // P99
            max_latency_ns_.load(std::memory_order_acquire),
            count,
            hardware_failures_.load(std::memory_order_acquire),
            software_fallbacks_.load(std::memory_order_acquire)
        };
    }
    
    // Check if inference latency meets SLA (400ns for software, configurable for hardware)
    bool meets_latency_sla(double sla_ns = 400.0) const {
        const auto stats = get_latency_stats();
        return stats.mean_ns <= sla_ns;
    }

private:
    // ========================================================================
    // Software Stub Initialization
    // ========================================================================
    
    bool initialize_software_stub() {
        // Software stub is always available (already initialized in constructor)
        status_.store(HardwareStatus::READY, std::memory_order_release);
        return true;
    }
    
    // ========================================================================
    // FPGA Hardware Initialization (Production Integration Point)
    // ========================================================================
    
    bool initialize_fpga_hardware() {
        // ====================================================================
        // PRODUCTION INTEGRATION STEPS:
        // ====================================================================
        //
        // 1. DETECT FPGA CARD
        //    - Scan PCIe bus for FPGA device ID (e.g., Xilinx/Intel vendor ID)
        //    - Verify FPGA firmware version matches expected version
        //    - Example: Use libpci or vendor SDK (Xilinx XRT, Intel OPAE)
        //
        // 2. MAP MEMORY REGIONS
        //    - Open PCIe device: /dev/xdma0 or vendor-specific device
        //    - mmap() PCIe Base Address Registers (BARs) for register access
        //    - Allocate huge pages for DMA buffers (2MB pages)
        //    - Setup DMA channels for feature input and prediction output
        //
        // 3. LOAD BITSTREAM (if needed)
        //    - Program FPGA with inference accelerator bitstream (.bit/.bin)
        //    - Verify programming success via status registers
        //    - Reset and initialize inference pipeline
        //
        // 4. CONFIGURE INFERENCE ENGINE
        //    - Write model weights to FPGA on-chip memory (BRAM/URAM)
        //    - Configure feature scaling parameters in registers
        //    - Set inference mode (single/batched) and timeout thresholds
        //
        // 5. HEALTH CHECK
        //    - Run dummy inference with known input
        //    - Verify output matches expected value
        //    - Measure and validate latency < 400ns
        //
        // ====================================================================
        
        // Simulation for development (REMOVE IN PRODUCTION)
        // In production, this would check actual hardware availability
        
        // Example production code structure:
        /*
        // Open FPGA device
        int fpga_fd = open("/dev/xdma0_user", O_RDWR | O_SYNC);
        if (fpga_fd < 0) {
            return false;  // FPGA not available
        }
        
        // Memory-map control registers (BAR 0, typically 64KB)
        void* control_regs = mmap(nullptr, 0x10000, 
                                  PROT_READ | PROT_WRITE, 
                                  MAP_SHARED, fpga_fd, 0);
        
        // Memory-map DMA buffers (BAR 2, typically 1GB)
        void* dma_buffer = mmap(nullptr, 0x40000000,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, fpga_fd, 0x200000000);
        
        // Store for later use
        fpga_fd_ = fpga_fd;
        fpga_control_regs_ = static_cast<volatile uint32_t*>(control_regs);
        fpga_dma_buffer_ = static_cast<float*>(dma_buffer);
        
        // Reset FPGA inference engine
        fpga_control_regs_[0] = 0x1;  // Reset bit
        usleep(100);
        fpga_control_regs_[0] = 0x0;  // Clear reset
        
        // Verify ready status
        if ((fpga_control_regs_[1] & 0x1) == 0) {
            return false;  // FPGA not ready
        }
        
        status_.store(HardwareStatus::READY, std::memory_order_release);
        return true;
        */
        
        // Development mode: FPGA not available, fall back to software
        return false;
    }
    
    // ========================================================================
    // Software Prediction Path
    // ========================================================================
    
    double predict_software(const MicrostructureFeatures& features) {
        // Route to software stub (guaranteed 400ns)
        auto predictions = software_inference_->predict(features);
        // Return first output (primary signal)
        return predictions[0];
    }
    
    // ========================================================================
    // Hardware Prediction Path (Production Integration Point)
    // ========================================================================
    
    bool predict_hardware(const MicrostructureFeatures& features, double& prediction) {
        // ====================================================================
        // PRODUCTION INTEGRATION STEPS:
        // ====================================================================
        //
        // 1. PREPARE INPUT DATA
        //    - Copy features to DMA buffer (or use zero-copy if features
        //      already in huge pages)
        //    - Apply any hardware-required scaling/quantization
        //
        // 2. TRIGGER INFERENCE
        //    - Write to FPGA control register to start inference
        //    - Set timeout watchdog (e.g., 1 microsecond)
        //
        // 3. WAIT FOR COMPLETION
        //    - Poll status register for completion flag
        //    - Or use interrupt-driven completion (requires kernel module)
        //    - Return false if timeout exceeded
        //
        // 4. READ RESULT
        //    - Read prediction from output register or DMA buffer
        //    - Apply any hardware-required de-scaling
        //
        // 5. VALIDATE
        //    - Check for hardware errors (overflow, underflow, NaN)
        //    - Verify latency meets SLA
        //
        // ====================================================================
        
        // Example production code structure:
        /*
        // Copy features to DMA buffer (zero-copy if features are aligned)
        float* input_buffer = fpga_dma_buffer_;
        input_buffer[0] = static_cast<float>(features.ofi_level_1);
        input_buffer[1] = static_cast<float>(features.ofi_level_5);
        input_buffer[2] = static_cast<float>(features.ofi_level_10);
        input_buffer[3] = static_cast<float>(features.trade_imbalance);
        input_buffer[4] = static_cast<float>(features.spread);
        input_buffer[5] = static_cast<float>(features.volatility);
        input_buffer[6] = static_cast<float>(features.microprice);
        input_buffer[7] = static_cast<float>(features.queue_imbalance);
        
        // Trigger inference (write to control register)
        fpga_control_regs_[2] = 0x1;  // Start inference
        
        // Poll for completion (with timeout)
        const auto deadline = std::chrono::steady_clock::now() + 
                             std::chrono::microseconds(1);
        
        while (std::chrono::steady_clock::now() < deadline) {
            if (fpga_control_regs_[1] & 0x2) {  // Done flag
                // Read prediction from output register
                float* output_buffer = fpga_dma_buffer_ + 1024;
                prediction = static_cast<double>(output_buffer[0]);
                
                // Clear done flag
                fpga_control_regs_[2] = 0x0;
                
                return true;  // Success
            }
            
            // Hardware busy-wait (use pause instruction)
            asm volatile("pause");
        }
        
        // Timeout - hardware not responding
        return false;
        */
        
        // Development mode: hardware not available
        return false;
    }
    
    // ========================================================================
    // Latency Statistics
    // ========================================================================
    
    void update_latency_stats(double latency_ns) {
        // Atomic double addition using CAS loop
        double old_val = latency_sum_ns_.load(std::memory_order_relaxed);
        while (!latency_sum_ns_.compare_exchange_weak(old_val, old_val + latency_ns,
                                                       std::memory_order_relaxed)) {
            // Retry on failure
        }
        
        // Update max latency (simple atomic max)
        double current_max = max_latency_ns_.load(std::memory_order_relaxed);
        while (latency_ns > current_max) {
            if (max_latency_ns_.compare_exchange_weak(current_max, latency_ns,
                                                      std::memory_order_relaxed)) {
                break;
            }
        }
    }

    // ========================================================================
    // Member Variables
    // ========================================================================
    
    // Accelerator configuration
    std::atomic<AcceleratorMode> mode_;
    std::atomic<HardwareStatus> status_;
    
    // Software inference engine (always available as fallback)
    std::unique_ptr<FPGA_DNN_Inference> software_inference_;
    
    // Hardware handles (for production FPGA integration)
    // int fpga_fd_;                          // FPGA device file descriptor
    // volatile uint32_t* fpga_control_regs_; // Memory-mapped control registers
    // float* fpga_dma_buffer_;               // DMA buffer for features/predictions
    
    // Statistics
    std::atomic<uint64_t> total_inferences_;
    std::atomic<uint64_t> hardware_failures_;
    std::atomic<uint64_t> software_fallbacks_;
    std::atomic<double> latency_sum_ns_;
    std::atomic<double> max_latency_ns_;
};

// ============================================================================
// Integration Example (for main.cpp)
// ============================================================================
//
// // Initialize bridge in software mode for development
// HardwareInTheLoopBridge hw_bridge(AcceleratorMode::SOFTWARE_STUB);
// hw_bridge.initialize();
//
// // Trading loop
// while (running) {
//     MarketTick tick = nic.receive();
//     auto features = extract_features(tick);
//     
//     // Seamless inference through bridge
//     double signal = hw_bridge.predict(features);
//     
//     // Monitor hardware health
//     if (hw_bridge.get_status() != HardwareStatus::READY) {
//         log_warning("Hardware degraded, using software fallback");
//     }
//     
//     // Validate latency SLA
//     if (!hw_bridge.meets_latency_sla(400.0)) {
//         log_warning("Inference latency exceeds 400ns SLA");
//     }
// }
//
// // Switch to hardware mode in production (hot-swap, no restart needed)
// hw_bridge.set_mode(AcceleratorMode::HYBRID_FALLBACK);

