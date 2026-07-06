#pragma once

#include <string>
#include <memory>
#include <map>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>

namespace vectordb {
namespace common {

/**
 * @class MetricsRegistry
 * @brief Singleton wrapper around prometheus-cpp. Manages the HTTP endpoint
 * and serves as the central point for registering counters, gauges, and histograms.
 */
class MetricsRegistry {
public:
    /**
     * @brief Initializes the CivetWeb HTTP server to expose /metrics
     * @param bind_address The IP:Port to bind to (e.g., "0.0.0.0:8080")
     */
    static void Initialize(const std::string& bind_address);
    static void Shutdown();

    /**
     * @brief Creates or retrieves a Counter metric. Counters only ever go UP.
     */
    static prometheus::Counter& AddCounter(const std::string& name, const std::string& help, const std::map<std::string, std::string>& labels);
    
    /**
     * @brief Creates or retrieves a Gauge metric. Gauges can go UP or DOWN.
     */
    static prometheus::Gauge& AddGauge(const std::string& name, const std::string& help, const std::map<std::string, std::string>& labels);
    
    /**
     * @brief Creates or retrieves a Histogram metric for distribution analysis (like latency).
     */
    static prometheus::Histogram& AddHistogram(const std::string& name, const std::string& help, const std::map<std::string, std::string>& labels, const std::vector<double>& bucket_boundaries);

private:
    static std::shared_ptr<prometheus::Registry> registry_;
    static std::unique_ptr<prometheus::Exposer> exposer_;
    
    // We cache the Metric Families so we don't try to register the same metric name twice
    static std::map<std::string, prometheus::Family<prometheus::Counter>*> counter_families_;
    static std::map<std::string, prometheus::Family<prometheus::Gauge>*> gauge_families_;
    static std::map<std::string, prometheus::Family<prometheus::Histogram>*> histogram_families_;
};

} // namespace common
} // namespace vectordb
