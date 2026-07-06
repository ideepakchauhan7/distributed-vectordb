#include "src/common/error/status.h"
#include <sstream>

namespace vectordb {
namespace common {

std::string StatusCodeToString(StatusCode code) {
    switch (code) {
        case StatusCode::kOk: return "OK";
        case StatusCode::kUnknown: return "Unknown";
        case StatusCode::kInvalidArgument: return "InvalidArgument";
        case StatusCode::kDeadlineExceeded: return "DeadlineExceeded";
        case StatusCode::kNotFound: return "NotFound";
        case StatusCode::kAlreadyExists: return "AlreadyExists";
        case StatusCode::kPermissionDenied: return "PermissionDenied";
        case StatusCode::kResourceExhausted: return "ResourceExhausted";
        case StatusCode::kFailedPrecondition: return "FailedPrecondition";
        case StatusCode::kAborted: return "Aborted";
        case StatusCode::kOutOfRange: return "OutOfRange";
        case StatusCode::kUnimplemented: return "Unimplemented";
        case StatusCode::kInternal: return "Internal";
        case StatusCode::kUnavailable: return "Unavailable";
        case StatusCode::kDataLoss: return "DataLoss";
        case StatusCode::kUnauthenticated: return "Unauthenticated";
        
        case StatusCode::kStorageIoError: return "StorageIoError";
        case StatusCode::kStorageCorruption: return "StorageCorruption";
        case StatusCode::kStorageFull: return "StorageFull";
        case StatusCode::kStorageNotFound: return "StorageNotFound";
        
        case StatusCode::kRaftNotLeader: return "RaftNotLeader";
        case StatusCode::kRaftLogCompacted: return "RaftLogCompacted";
        case StatusCode::kRaftElectionInProgress: return "RaftElectionInProgress";
        case StatusCode::kRaftTimeout: return "RaftTimeout";
        case StatusCode::kRaftInvalidTerm: return "RaftInvalidTerm";
        
        case StatusCode::kVectorDimensionMismatch: return "VectorDimensionMismatch";
        case StatusCode::kVectorNotFound: return "VectorNotFound";
        case StatusCode::kVectorIndexFull: return "VectorIndexFull";
        
        case StatusCode::kNetworkTimeout: return "NetworkTimeout";
        case StatusCode::kNetworkUnavailable: return "NetworkUnavailable";
        case StatusCode::kRouterShardNotFound: return "RouterShardNotFound";
        
        default: return "Unknown Code";
    }
}

std::string Status::ToString() const {
    if (IsOk()) {
        return "OK";
    }
    std::stringstream ss;
    ss << StatusCodeToString(code_) << ": " << message_;
    return ss.str();
}

} // namespace common
} // namespace vectordb
