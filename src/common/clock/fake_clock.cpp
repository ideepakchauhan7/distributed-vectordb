#include "src/common/clock/fake_clock.h"

namespace vectordb {
namespace common {

uint64_t FakeClock::NowMicros() const {
    return current_micros_.load();
}

void FakeClock::SleepForMicros(uint64_t micros) {
    // In a fake clock, sleeping just advances time.
    Advance(micros);
}

void FakeClock::Advance(uint64_t micros) {
    current_micros_.fetch_add(micros);
}

} // namespace common
} // namespace vectordb
