#include "production_kernel.hpp"
#include "jitter_profiler.hpp"
#include <iostream>
#include <csignal>
#include <getopt.h>

std::unique_ptr<hft::ProductionKernel> kernel;

void signal_handler(int signum) {
    std::cout << "\n[SIGNAL] Received " << signum << ". Stopping production kernel..." << std::endl;
    if (kernel) {
        kernel->stop();
    }
}

void print_usage() {
    std::cout << "Usage: trading_prod [options]\n"
              << "Options:\n"
              << "  -c, --core <id>       CPU core to pin for hot path (default: 1)\n"
              << "  -i, --interface <dev> Network interface for ef_vi (default: eth0)\n"
              << "  -g, --gamma <val>      Risk aversion (default: 0.1)\n"
              << "  -v, --vpin <val>      Toxicity threshold (default: 0.7)\n"
              << "  -h, --help            Show this help\n"
              << std::endl;
}

int main(int argc, char** argv) {
    // 1. Setup Signal Handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 2. Default Config
    hft::ProductionKernel::Config config;
    config.cpu_core_id = 1;
    config.interface_name = "eth0";
    config.mm_params.risk_aversion = 0.1;
    config.mm_params.rebate_value = 0.002;
    config.mm_params.intensity_k = 0.5;
    config.mm_params.ofi_k = 0.5;
    config.mm_params.inventory_cap = 5000;

    // 3. Parse CLI Args
    static struct option long_options[] = {
        {"core", required_argument, 0, 'c'},
        {"interface", required_argument, 0, 'i'},
        {"gamma", required_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "c:i:g:h", long_options, &opt_index)) != -1) {
        switch (c) {
            case 'c': config.cpu_core_id = std::stoi(optarg); break;
            case 'i': config.interface_name = optarg; break;
            case 'g': config.mm_params.risk_aversion = std::stod(optarg); break;
            case 'h': print_usage(); return 0;
            default: break;
        }
    }

    // 4. Global Hardware Readiness Checks
    std::cout << "=== SUB-MICRO PRODUCTION TRADING SYSTEM ===" << std::endl;
    
#if defined(__linux__)
    // TODO: Verify Hugepages are allocated
    // TODO: Verify core is isolated via isolcpus
#endif

    // 5. Instantiate Kernel
    kernel = std::make_unique<hft::ProductionKernel>(config);

    // 6. Launch Hot Path
    try {
        kernel->run();
    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL] Kernel Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[SHUTDOWN] Production system terminated cleanly." << std::endl;
    return 0;
}
