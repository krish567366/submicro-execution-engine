#pragma once

#include "common_types.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <numa.h> // libnuma required
#include <sched.h>
#include <iostream>

namespace hft {
namespace hardware {

/**
 * NUMA (Non-Uniform Memory Access) Topology Mapper
 * 
 * Purpose:
 * Identifies which CPU Cores and Memory Nodes are closest to a specific PCIe device (the NIC).
 * 
 * Logic:
 * 1. Read `/sys/class/net/<interface>/device/numa_node`.
 * 2. Pin Thread to cores on that node.
 * 3. Allocate hugepages on that node.
 */
class NumaTopology {
public:
    static int get_nic_numa_node(const std::string& interface) {
        std::string path = "/sys/class/net/" + interface + "/device/numa_node";
        std::ifstream f(path);
        if (!f.is_open()) return 0; // Default to node 0
        int node;
        f >> node;
        if (node < 0) node = 0;
        return node;
    }

    static void pin_thread_to_node(int node_id) {
        if (numa_available() < 0) return;

        // Run on this node
        if (numa_run_on_node(node_id) < 0) {
            std::cerr << "Failed to pin to NUMA node " << node_id << std::endl;
        }
        
        // Allocate memory from this node preferentially
        numa_set_preferred(node_id);
    }

    static void isolate_core(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        
        if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
            std::cerr << "Failed to isolate core " << cpu_id << std::endl;
        }
    }
};

}
}
