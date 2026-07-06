#pragma once
#include "src/common/clock/clock.h"
#include <mutex>
#include <cstdint>

namespace vectordb {
namespace common {

/**
 * @class HLC (Hybrid Logical Clock)
 * @brief Generates causally consistent timestamps for distributed events.
 * 
 * Represents a 64-bit timestamp: 
 * [ 48 bits: Physical time in micros ] [ 16 bits: Logical counter ]
 */
class HLC {
public:
    explicit HLC(Clock* clock) : clock_(clock), current_time_(0) {}

    // Generate a new timestamp for a local event
    uint64_t Now();

    // Update the clock with a timestamp received from another node
    uint64_t Update(uint64_t remote_time);

    // Helpers
    static uint64_t GetPhysicalTime(uint64_t hlc_time) { return hlc_time >> 16; }
    static uint64_t GetLogicalTime(uint64_t hlc_time) { return hlc_time & 0xFFFF; }

private:
    Clock* clock_;
    std::mutex mutex_;
    uint64_t current_time_; // The packed 64-bit HLC time
};

} // namespace common
} // namespace vectordb
