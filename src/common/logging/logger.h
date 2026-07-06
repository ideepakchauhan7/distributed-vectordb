#pragma once

#include <string>
#include <memory>
// Define SPDLOG_ACTIVE_LEVEL before including spdlog so we can compile out TRACE logs in release if we want
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE 
#include <spdlog/spdlog.h>
#include "src/common/logging/log_level.h"

namespace vectordb {
namespace common {

/**
 * @class Logger
 * @brief Provides globally accessible, highly performant asynchronous logging.
 * 
 * Uses spdlog under the hood to ensure logging never blocks the database's 
 * hot paths (like RAFT consensus and vector search loops).
 */
class Logger {
public:
    /**
     * @brief Initializes the global async logger. Must be called once on process startup.
     * @param node_id The ID of this node, injected into every log message.
     * @param log_file_path The file path for the rotating log file.
     */
    static void Initialize(const std::string& node_id, const std::string& log_file_path);

    /**
     * @brief Flushes all async logs and shuts down the worker thread.
     */
    static void Shutdown();

    /**
     * @brief Accessor for the underlying spdlog logger.
     */
    static std::shared_ptr<spdlog::logger> Get() { return logger_; }

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static std::string node_id_;
};

/**
 * @brief Helper macros to enforce a JSON-like structure for all logs.
 * We wrap the message in JSON fields so that log aggregators (like Loki/Elasticsearch)
 * can automatically parse them.
 */
#define LOG_INFO(component, msg) \
    ::vectordb::common::Logger::Get()->info("{{\"component\": \"{}\", \"msg\": \"{}\"}}", component, msg)

#define LOG_WARN(component, msg) \
    ::vectordb::common::Logger::Get()->warn("{{\"component\": \"{}\", \"msg\": \"{}\"}}", component, msg)

#define LOG_ERROR(component, msg) \
    ::vectordb::common::Logger::Get()->error("{{\"component\": \"{}\", \"msg\": \"{}\"}}", component, msg)

#define LOG_FATAL(component, msg) \
    do { \
        ::vectordb::common::Logger::Get()->critical("{{\"component\": \"{}\", \"msg\": \"{}\"}}", component, msg); \
        ::vectordb::common::Logger::Shutdown(); \
        std::abort(); \
    } while (0)

} // namespace common
} // namespace vectordb
