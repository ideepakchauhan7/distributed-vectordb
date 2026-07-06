#pragma once

#include <string>
#include <utility>
#include "src/common/error/status_code.h"

namespace vectordb {
namespace common {

/**
 * @class Status
 * @brief Represents the result of an operation, containing a code and an optional error message.
 * 
 * In this codebase, we avoid exceptions. Functions that can fail return a Status
 * (or ErrorOr<T> if they also return a value).
 */
class Status {
public:
    /// @brief Creates a success status.
    Status() : code_(StatusCode::kOk) {}

    /// @brief Creates a status with a specific code and error message.
    Status(StatusCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    /// @brief Creates a success status explicitly.
    static Status Ok() { return Status(); }

    /// @brief Checks if the status represents success.
    bool IsOk() const { return code_ == StatusCode::kOk; }

    /// @brief Returns the status code.
    StatusCode code() const { return code_; }

    /// @brief Returns the error message.
    const std::string& message() const { return message_; }

    /// @brief Returns a string representation of the status (e.g., for logging).
    std::string ToString() const;

private:
    StatusCode code_;
    std::string message_;
};

} // namespace common
} // namespace vectordb
