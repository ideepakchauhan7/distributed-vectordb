#pragma once
#include <cstdint>

namespace vectordb {
namespace common {

/**
 * @class Clock
 * @brief Abstract interface for time generation. 
 * Allows injecting FakeClock for deterministic RAFT testing.
 */
class Clock {
public:
    virtual ~Clock() = default;

    /**
     * @brief Returns current time since epoch in microseconds.
     */
    virtual uint64_t NowMicros() const = 0;
    
    /**
     * @brief Sleeps for the given microseconds.
     */
    virtual void SleepForMicros(uint64_t micros) = 0;
};

} // namespace common
} // namespace vectordb
