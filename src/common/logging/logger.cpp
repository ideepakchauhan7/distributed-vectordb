#include "src/common/logging/logger.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace vectordb {
namespace common {

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
std::string Logger::node_id_ = "unknown";

void Logger::Initialize(const std::string& node_id, const std::string& log_file_path) {
    node_id_ = node_id;
    
    // Initialize spdlog's async thread pool
    // 8192 queue size, 1 backing thread.
    spdlog::init_thread_pool(8192, 1);
    
    // Create multiple sinks:
    // 1. Console sink (for easy debugging when running interactively)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    // 2. Rotating file sink (100MB per file, max 10 files)
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path, 1024 * 1024 * 100, 10);
    
    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
    
    // Create the async logger
    logger_ = std::make_shared<spdlog::async_logger>(
        "vectordb", 
        sinks.begin(), 
        sinks.end(), 
        spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block // Block the calling thread if the queue fills up
    );
    
    // Create a JSON-like log pattern
    // Result looks like: {"time": "2026-06-25T16:00:00.123", "node_id": "node-1", "level": "INFO", "data": {"component": "raft", "msg": "Leader elected"}}
    std::string pattern = std::string("{\"time\": \"%Y-%m-%dT%H:%M:%S.%f\", \"node_id\": \"") + node_id_ + "\", \"level\": \"%^%l%$\", \"data\": %v}";
    logger_->set_pattern(pattern);
    
    // Register it globally
    spdlog::register_logger(logger_);
    spdlog::set_default_logger(logger_);
    
    // Default level
    logger_->set_level(spdlog::level::info);
}

void Logger::Shutdown() {
    // Flush all pending logs before exiting
    if (logger_) {
        logger_->flush();
    }
    spdlog::shutdown();
}

} // namespace common
} // namespace vectordb
