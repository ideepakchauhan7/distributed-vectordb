#include "src/common/metrics/metrics_registry.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace vectordb::common;

int main() {
    // 1. Initialize the HTTP server on port 8080
    std::cout << "Starting Prometheus metrics server on 127.0.0.1:8080..." << std::endl;
    MetricsRegistry::Initialize("127.0.0.1:8080");

    // 2. Define a Counter (only goes up)
    auto& inserts_total = MetricsRegistry::AddCounter(
        "vectordb_inserts_total",
        "Total number of vectors inserted",
        {{"node_id", "node-1"}}
    );

    // 3. Define a Gauge (can go up and down)
    auto& active_connections = MetricsRegistry::AddGauge(
        "vectordb_active_connections",
        "Current number of active gRPC connections",
        {{"node_id", "node-1"}}
    );

    // 4. Define a Histogram (tracks latency buckets)
    auto& search_latency = MetricsRegistry::AddHistogram(
        "vectordb_search_latency_ms",
        "Latency of vector searches in milliseconds",
        {{"node_id", "node-1"}},
        prometheus::Histogram::BucketBoundaries{1.0, 5.0, 10.0, 50.0, 100.0}
    );

    std::cout << "Simulating database traffic..." << std::endl;
    
    // Simulate events
    inserts_total.Increment();
    inserts_total.Increment();
    
    active_connections.Set(15.0);
    
    search_latency.Observe(3.5);
    search_latency.Observe(12.0);
    search_latency.Observe(2.0);

    // Let the HTTP server run briefly so we could curl it if we wanted to
    std::cout << "Metrics populated. Server running for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    MetricsRegistry::Shutdown();
    std::cout << "Metrics server shut down gracefully." << std::endl;
    
    return 0;
}
