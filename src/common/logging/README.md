# Asynchronous Logging Subsystem (`common/logging`)

## 📐 System Design Philosophy

In a high-throughput distributed database, synchronous logging (e.g., `std::cout` or basic file I/O) is catastrophic. If the RAFT leader writes a log message during an election, and the disk stalls for 10ms, the node might lose its leadership. 

To solve this, we use **True Asynchronous Logging** via the `spdlog` library. 

Furthermore, searching through unstructured text logs across 50 nodes is impossible. We utilize **JSON Structured Logging** so that log aggregators (like Loki or Elasticsearch) can automatically index every field.

## ⚙️ How It Works

### 1. Background Worker Thread
When `Logger::Initialize()` is called, we allocate a lock-free queue (capacity: 8192 messages) and spawn exactly 1 background worker thread. 
When a hot-path thread calls `LOG_INFO`, the message is instantly pushed to the queue in nanoseconds, and the thread continues executing. The background worker drains the queue and performs the slow disk I/O.

### 2. Dual Sinks
The logger multiplexes the output to two destinations simultaneously:
1. **Console**: Color-coded, human-readable output for local development.
2. **Rotating File Sink**: Writes to `vectordb.log`. It automatically caps the file at 100MB and keeps a maximum of 10 historical files, guaranteeing the database never crashes a server due to log bloat.

### 3. JSON Enforcement Macros
We expose four primary macros: `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, and `LOG_FATAL`.
These macros mandate two arguments: `component` and `message`. The underlying `spdlog` pattern formatter wraps this data, along with the `node_id` and an ISO-8601 microsecond timestamp, into a valid JSON string.

```cpp
LOG_INFO("raft", "Election timeout triggered. Attempting to become leader.");
```
Outputs:
```json
{"time": "2026-06-25T16:22:14.645459", "node_id": "router-node-1", "level": "info", "data": {"component": "raft", "msg": "Election timeout triggered. Attempting to become leader."}}
```
