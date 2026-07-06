# Metrics & Telemetry Subsystem (`common/metrics`)

## 📐 System Design Philosophy

You cannot optimize what you cannot measure. A database requires extreme visibility into its internals to diagnose performance bottlenecks and train the ML-Routing layer.

We utilize a **Pull-Based Metrics Model** using `prometheus-cpp`. Instead of pushing metrics to a central server (which adds network overhead and failure points), the database exposes an internal HTTP server. Monitoring tools like Prometheus scrape this endpoint periodically.

## ⚙️ How It Works

### 1. Embedded HTTP Server (CivetWeb)
When `MetricsRegistry::Initialize("0.0.0.0:8080")` is called, a lightweight, embedded C/C++ web server (CivetWeb) is spun up on a background thread. It listens on the specified port and automatically serves the `/metrics` endpoint in standard Prometheus text format.

### 2. The Three Golden Signals
We expose three distinct types of telemetry:

*   **Counters (`AddCounter`)**: Monotonically increasing values. They never go down unless the node restarts.
    *   *Examples:* Total vectors inserted, total gRPC errors, RAFT bytes replicated.
*   **Gauges (`AddGauge`)**: Values that can fluctuate up and down.
    *   *Examples:* Active connections, memory consumption, WAL size on disk.
*   **Histograms (`AddHistogram`)**: Sorts observations into predefined buckets to calculate percentiles (P95, P99).
    *   *Examples:* Vector search latency, RAFT commit latency. This is crucial for verifying our ML-Router's performance gains.

### 3. Singleton Registry (`metrics_registry.cpp`)
Because `prometheus-cpp` will throw exceptions if you try to register two metrics with the identical name, we wrap it in a thread-safe `MetricsRegistry`. When you call `AddCounter`, the registry checks if a metric "family" by that name already exists. If so, it reuses it; if not, it registers a new one.

```cpp
// Create or fetch the latency histogram
auto& search_latency = MetricsRegistry::AddHistogram(
    "vectordb_search_latency_ms",
    "Latency of vector searches",
    {{"node_id", "node-1"}}, // Labels
    {1.0, 5.0, 10.0, 50.0}   // Buckets
);

// Record an event
search_latency.Observe(3.5); 
```
