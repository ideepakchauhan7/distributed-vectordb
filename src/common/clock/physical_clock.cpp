#include "src/common/clock/physical_clock.h"
#include <chrono>
#include <thread>

namespace vectordb {
namespace common {

uint64_t PhysicalClock::NowMicros() const {
    // We use system_clock here because HLC usually represents wall-clock time 
    // to interoperate with external systems.
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

void PhysicalClock::SleepForMicros(uint64_t micros) {
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

} // namespace common
} // namespace vectordb
