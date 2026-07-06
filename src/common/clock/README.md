# Clock & Time Synchronization Subsystem (`common/clock`)

## 📐 System Design Philosophy

Handling time natively using standard OS system calls (e.g., `std::chrono::system_clock`) in a distributed database introduces two catastrophic problems:

1. **Testability:** RAFT consensus heavily relies on timeouts (e.g., "if no heartbeat is received in 500ms, become candidate"). If tests rely on real CPU time, unit tests become incredibly slow and flaky. 
2. **Clock Skew:** Perfect time synchronization across physical servers is impossible. Even with NTP, clocks will drift. If Node A and Node B generate events, relying on their physical hardware clocks to determine which event happened "first" violates strict causality.

### ⚙️ How It Works

This subsystem provides three distinct tools to solve time-related issues:

#### 1. The `Clock` Interface
Instead of calling `sleep()` or `now()` directly, all components take a pointer to the abstract `Clock` interface.
- **`PhysicalClock`**: Used in production. Wraps `std::chrono`.
- **`FakeClock`**: Used in unit tests. Time starts at 0 and ONLY advances when the test explicitly calls `clock.Advance(500)`. This allows us to simulate years of database uptime in milliseconds.

#### 2. Hybrid Logical Clock (HLC)
Derived from the Google Spanner and CockroachDB design, the HLC provides causally consistent timestamps without requiring perfect hardware clock synchronization.

An HLC timestamp is a single 64-bit integer packed with two values:
- **High 48 bits**: The physical time (in microseconds).
- **Low 16 bits**: A logical counter.

When an event occurs on a node, it updates its HLC. If the physical clock hasn't moved forward, it increments the logical counter. When a node receives an RPC from another node, it calls `hlc.Update(remote_time)`. This guarantees that if Event A caused Event B, `Timestamp(A) < Timestamp(B)`, regardless of hardware clock drift.
