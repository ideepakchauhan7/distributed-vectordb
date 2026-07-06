#pragma once
#include "src/common/clock/clock.h"

namespace vectordb {
namespace common {

class PhysicalClock : public Clock {
public:
    uint64_t NowMicros() const override;
    void SleepForMicros(uint64_t micros) override;
};

} // namespace common
} // namespace vectordb
