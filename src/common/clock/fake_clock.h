#pragma once
#include "src/common/clock/clock.h"
#include <atomic>

namespace vectordb {
namespace common {

class FakeClock : public Clock {
public:
    explicit FakeClock(uint64_t start_micros = 0) : current_micros_(start_micros) {}

    uint64_t NowMicros() const override;
    void SleepForMicros(uint64_t micros) override;

    // Test-only methods
    void Advance(uint64_t micros);

private:
    std::atomic<uint64_t> current_micros_;
};

} // namespace common
} // namespace vectordb
