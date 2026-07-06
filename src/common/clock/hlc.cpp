#include "src/common/clock/hlc.h"
#include <algorithm>

namespace vectordb {
namespace common {

uint64_t HLC::Now() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t physical_time = clock_->NowMicros();
    uint64_t current_physical = GetPhysicalTime(current_time_);
    uint64_t current_logical = GetLogicalTime(current_time_);

    if (physical_time > current_physical) {
        // Physical clock has advanced
        current_time_ = (physical_time << 16) | 0;
    } else {
        // Physical clock is equal or behind (clock skew), increment logical counter
        current_time_ = (current_physical << 16) | (current_logical + 1);
    }
    
    return current_time_;
}

uint64_t HLC::Update(uint64_t remote_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t physical_time = clock_->NowMicros();
    uint64_t current_physical = GetPhysicalTime(current_time_);
    uint64_t current_logical = GetLogicalTime(current_time_);
    
    uint64_t remote_physical = GetPhysicalTime(remote_time);
    uint64_t remote_logical = GetLogicalTime(remote_time);

    uint64_t max_physical = std::max({physical_time, current_physical, remote_physical});

    if (max_physical == physical_time) {
        if (physical_time > current_physical && physical_time > remote_physical) {
            current_time_ = (physical_time << 16) | 0;
            return current_time_;
        }
    }

    uint64_t max_logical = 0;
    if (max_physical == current_physical) {
        max_logical = std::max(max_logical, current_logical);
    }
    if (max_physical == remote_physical) {
        max_logical = std::max(max_logical, remote_logical);
    }

    current_time_ = (max_physical << 16) | (max_logical + 1);
    return current_time_;
}

} // namespace common
} // namespace vectordb
