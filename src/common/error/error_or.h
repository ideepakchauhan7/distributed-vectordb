#pragma once

#include <variant>
#include <utility>
#include <stdexcept>
#include "src/common/error/status.h"

namespace vectordb {
namespace common {

/**
 * @class ErrorOr
 * @brief A vocabulary type holding either a value of type T or a Status representing an error.
 * 
 * Similar to std::expected (C++23) or absl::StatusOr, this class ensures that we
 * return values explicitly handling potential errors without relying on exceptions.
 * 
 * Usage:
 * @code
 * ErrorOr<int> Divide(int a, int b) {
 *     if (b == 0) return Status(StatusCode::kInvalidArgument, "Division by zero");
 *     return a / b;
 * }
 * 
 * auto result = Divide(10, 2);
 * if (!result.IsOk()) {
 *     LOG_ERROR(result.status().ToString());
 *     return;
 * }
 * int value = result.value();
 * @endcode
 */
template <typename T>
class ErrorOr {
public:
    /// @brief Constructs from a value. Status will implicitly be OK.
    ErrorOr(T value) : state_(std::move(value)) {}

    /// @brief Constructs from a Status. Status must NOT be OK.
    ErrorOr(Status status) : state_(std::move(status)) {
        if (std::get<Status>(state_).IsOk()) {
            throw std::invalid_argument("ErrorOr created with OK status but no value.");
        }
    }

    /// @brief Checks if the result contains a value (i.e., the operation was successful).
    bool IsOk() const {
        return std::holds_alternative<T>(state_);
    }

    /// @brief Returns the contained status. Returns Status::Ok() if it holds a value.
    Status status() const {
        if (IsOk()) {
            return Status::Ok();
        }
        return std::get<Status>(state_);
    }

    /// @brief Returns a reference to the contained value.
    /// @throws std::runtime_error if it contains an error.
    T& value() {
        if (!IsOk()) {
            throw std::runtime_error("Attempted to access value of an ErrorOr containing an error: " + status().ToString());
        }
        return std::get<T>(state_);
    }

    /// @brief Returns a const reference to the contained value.
    /// @throws std::runtime_error if it contains an error.
    const T& value() const {
        if (!IsOk()) {
            throw std::runtime_error("Attempted to access value of an ErrorOr containing an error: " + status().ToString());
        }
        return std::get<T>(state_);
    }

    /// @brief Dereference operator to access the value. Undefined behavior if IsOk() is false.
    T* operator->() { return &std::get<T>(state_); }
    const T* operator->() const { return &std::get<T>(state_); }

    T& operator*() { return std::get<T>(state_); }
    const T& operator*() const { return std::get<T>(state_); }

private:
    std::variant<T, Status> state_;
};

} // namespace common
} // namespace vectordb

/**
 * @brief Helper macros for concise error handling.
 * ASSIGN_OR_RETURN allows you to cleanly unwrap ErrorOr<T> or return the Status if it failed.
 */
#define RETURN_IF_ERROR(expr) \
    do { \
        auto _status = (expr); \
        if (!_status.IsOk()) return _status; \
    } while (0)

#define ASSIGN_OR_RETURN_IMPL(lhs, rexpr, name) \
    auto name = (rexpr); \
    if (!name.IsOk()) return name.status(); \
    lhs = std::move(name.value());

#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define ASSIGN_OR_RETURN(lhs, rexpr) \
    ASSIGN_OR_RETURN_IMPL(lhs, rexpr, CONCAT(_error_or_result_, __LINE__))
