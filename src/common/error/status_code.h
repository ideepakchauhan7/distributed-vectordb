#pragma once

#include <string>

namespace vectordb {
namespace common {

/**
 * @brief Represents all possible error codes across the distributed vector database.
 * 
 * We use a unified enum for all subsystems to ensure consistent error handling
 * and easy mapping to external interfaces (like gRPC).
 */
enum class StatusCode {
    kOk = 0,

    // General Errors (1-99)
    kUnknown = 1,
    kInvalidArgument = 2,
    kDeadlineExceeded = 3,
    kNotFound = 4,
    kAlreadyExists = 5,
    kPermissionDenied = 6,
    kResourceExhausted = 7,
    kFailedPrecondition = 8,
    kAborted = 9,
    kOutOfRange = 10,
    kUnimplemented = 11,
    kInternal = 12,
    kUnavailable = 13,
    kDataLoss = 14,
    kUnauthenticated = 15,

    // Storage Errors (100-199)
    kStorageIoError = 100,
    kStorageCorruption = 101,
    kStorageFull = 102,
    kStorageNotFound = 103,

    // RAFT Errors (200-299)
    kRaftNotLeader = 200,
    kRaftLogCompacted = 201,
    kRaftElectionInProgress = 202,
    kRaftTimeout = 203,
    kRaftInvalidTerm = 204,

    // Vector Engine Errors (300-399)
    kVectorDimensionMismatch = 300,
    kVectorNotFound = 301,
    kVectorIndexFull = 302,

    // Network / Router Errors (400-499)
    kNetworkTimeout = 400,
    kNetworkUnavailable = 401,
    kRouterShardNotFound = 402,
};

/**
 * @brief Converts a StatusCode to its string representation.
 */
std::string StatusCodeToString(StatusCode code);

} // namespace common
} // namespace vectordb
