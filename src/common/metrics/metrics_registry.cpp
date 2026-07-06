#include "src/common/metrics/metrics_registry.h"
#include <mutex>

namespace vectordb {
namespace common {

std::shared_ptr<prometheus::Registry> MetricsRegistry::registry_ = nullptr;
std::unique_ptr<prometheus::Exposer> MetricsRegistry::exposer_ = nullptr;

std::map<std::string, prometheus::Family<prometheus::Counter>*> MetricsRegistry::counter_families_;
std::map<std::string, prometheus::Family<prometheus::Gauge>*> MetricsRegistry::gauge_families_;
std::map<std::string, prometheus::Family<prometheus::Histogram>*> MetricsRegistry::histogram_families_;

static std::mutex registry_mutex;

void MetricsRegistry::Initialize(const std::string& bind_address) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    if (!registry_) {
        registry_ = std::make_shared<prometheus::Registry>();
        // Exposer handles spinning up the background CivetWeb HTTP server
        exposer_ = std::make_unique<prometheus::Exposer>(bind_address, 1);
        exposer_->RegisterCollectable(registry_);
    }
}

void MetricsRegistry::Shutdown() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    exposer_.reset();
    registry_.reset();
}

prometheus::Counter& MetricsRegistry::AddCounter(
    const std::string& name, 
    const std::string& help, 
    const std::map<std::string, std::string>& labels) {
    
    std::lock_guard<std::mutex> lock(registry_mutex);
    if (counter_families_.find(name) == counter_families_.end()) {
        auto& family = prometheus::BuildCounter().Name(name).Help(help).Register(*registry_);
        counter_families_[name] = &family;
    }
    return counter_families_[name]->Add(labels);
}

prometheus::Gauge& MetricsRegistry::AddGauge(
    const std::string& name, 
    const std::string& help, 
    const std::map<std::string, std::string>& labels) {
    
    std::lock_guard<std::mutex> lock(registry_mutex);
    if (gauge_families_.find(name) == gauge_families_.end()) {
        auto& family = prometheus::BuildGauge().Name(name).Help(help).Register(*registry_);
        gauge_families_[name] = &family;
    }
    return gauge_families_[name]->Add(labels);
}

prometheus::Histogram& MetricsRegistry::AddHistogram(
    const std::string& name, 
    const std::string& help, 
    const std::map<std::string, std::string>& labels,
    const std::vector<double>& bucket_boundaries) {
    
    std::lock_guard<std::mutex> lock(registry_mutex);
    if (histogram_families_.find(name) == histogram_families_.end()) {
        auto& family = prometheus::BuildHistogram().Name(name).Help(help).Register(*registry_);
        histogram_families_[name] = &family;
    }
    return histogram_families_[name]->Add(labels, bucket_boundaries);
}

} // namespace common
} // namespace vectordb
