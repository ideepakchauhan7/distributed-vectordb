#pragma once

namespace vectordb {
namespace common {

/**
 * @brief Represents the severity of a log message.
 */
enum class LogLevel {
    kTrace,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kFatal
};

} // namespace common
} // namespace vectordb
