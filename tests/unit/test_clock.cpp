#include "src/common/clock/physical_clock.h"
#include "src/common/clock/fake_clock.h"
#include "src/common/clock/hlc.h"
#include <iostream>
#include <cassert>

using namespace vectordb::common;

void TestHLC() {
    FakeClock clock(100);
    HLC hlc(&clock);

    // Test local event
    uint64_t t1 = hlc.Now();
    if (HLC::GetPhysicalTime(t1) != 100 || HLC::GetLogicalTime(t1) != 0) { std::cerr << "T1 failed\n"; exit(1); }

    // Test fast local events (physical clock hasn't changed)
    uint64_t t2 = hlc.Now();
    if (HLC::GetPhysicalTime(t2) != 100 || HLC::GetLogicalTime(t2) != 1) { std::cerr << "T2 failed\n"; exit(1); }

    // Test physical clock advance
    clock.Advance(50);
    uint64_t t3 = hlc.Now();
    if (HLC::GetPhysicalTime(t3) != 150 || HLC::GetLogicalTime(t3) != 0) { std::cerr << "T3 failed\n"; exit(1); }

    // Test remote update (remote is ahead)
    uint64_t remote_time = (200ULL << 16) | 5;
    uint64_t t4 = hlc.Update(remote_time);
    if (HLC::GetPhysicalTime(t4) != 200 || HLC::GetLogicalTime(t4) != 6) { std::cerr << "T4 failed\n"; exit(1); }

    std::cout << "All HLC tests passed." << std::endl;
}

int main() {
    std::cout << "Testing Clocks..." << std::endl;
    TestHLC();
    return 0;
}
