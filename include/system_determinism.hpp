#pragma once

#include "common_types.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <iostream>

// Forward declare from jitter_profiler.hpp to avoid duplication
namespace hft { namespace timing { uint64_t read_tsc(); }}

namespace hft {
namespace determinism {

/**
 * System Flight Recorder
 * 
 * Purpose: Ensure Bit-Exact Replayability of Production Incidents.
 * 
 * Problem:
 * HFT systems are non-deterministic due to:
 * 1. Network Interrupt timing
 * 2. Thread scheduling
 * 3. RDTSC (Time Stamp Counter) reads
 * 4. System Calls (gettimeofday)
 * 
 * Solution:
 * 1. Log every "Non-Deterministic Input" (NDI) to a binary journal.
 * 2. In Replay Mode, intercept these calls and return values from the journal.
 */

// Event Header
enum class EventType : uint8_t {
    PACKET_RX = 1,
    RDTSC_READ = 2,
    CHECK_TIME = 3
};

#pragma pack(push, 1)
struct JournalEntry {
    EventType type;
    uint64_t seq;
    uint64_t data; // RDTSC value or Packet Length
    // For packets, data follows immediately
};
#pragma pack(pop)

class FlightRecorder {
public:
    enum class Mode {
        PASSIVE,    // Do nothing (default)
        RECORD,     // Record all non-deterministic events
        REPLAY      // Replay events instead of executing them
    };

    static FlightRecorder& instance() {
        static FlightRecorder inst;
        return inst;
    }

    void set_mode(Mode m, const std::string& filepath) {
        mode_ = m;
        if (mode_ == Mode::RECORD) {
            file_.open(filepath, std::ios::binary | std::ios::out | std::ios::trunc);
        } else if (mode_ == Mode::REPLAY) {
            file_.open(filepath, std::ios::binary | std::ios::in);
        }
    }

    // Intercept rdtsc
    inline uint64_t get_tsc() {
        if (mode_ == Mode::PASSIVE) {
            return hft::timing::read_tsc();
        } else if (mode_ == Mode::RECORD) {
            uint64_t tsc = hft::timing::read_tsc();
            log_event(EventType::RDTSC_READ, tsc);
            return tsc;
        } else {
            // REPLAY
            return read_expected_event(EventType::RDTSC_READ);
        }
    }

    // Intercept Packet Arrival
    // Returns packet length (or 0 if no packet in replay stream yet)
    inline size_t check_packet_arrival(size_t actual_len) {
        if (mode_ == Mode::PASSIVE) return actual_len;
        
        if (mode_ == Mode::RECORD) {
            if (actual_len > 0) log_event(EventType::PACKET_RX, actual_len);
            return actual_len;
        } else {
            // REPLAY
            // Peek next event from log
            if (!file_.is_open() || file_.eof()) return 0;
            
            auto pos = file_.tellg();
            JournalEntry e;
            file_.read(reinterpret_cast<char*>(&e), sizeof(e));
            
            if (file_.gcount() != sizeof(e)) {
                return 0;  // End of replay
            }
            
            // If next event is packet arrival, consume it and return length
            if (e.type == EventType::PACKET_RX) {
                return static_cast<size_t>(e.data);
            } else {
                // Not a packet event yet, rewind and return 0
                file_.seekg(pos);
                return 0;
            }
        }
    }

private:
    Mode mode_ = Mode::PASSIVE;
    std::fstream file_;
    uint64_t seq_ = 0;

    void log_event(EventType type, uint64_t data) {
        JournalEntry e{type, seq_++, data};
        file_.write(reinterpret_cast<char*>(&e), sizeof(e));
        // Flush? No, too slow. OS page cache handles safely enough unless kernel panic.
    }

    uint64_t read_expected_event(EventType expected_type) {
        JournalEntry e;
        file_.read(reinterpret_cast<char*>(&e), sizeof(e));
        if (e.type != expected_type) {
            std::cerr << "REPLAY DIVERGENCE! Exp: " << (int)expected_type << " Got: " << (int)e.type << std::endl;
            exit(1);
        }
        return e.data;
    }
    
    FlightRecorder() = default;
};

// Global interceptor macro
#define HFT_RDTSC() hft::determinism::FlightRecorder::instance().get_tsc()

}
}
