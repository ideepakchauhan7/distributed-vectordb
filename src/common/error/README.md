# Error Handling Subsystem (`common/error`)

## 📐 System Design Philosophy

In a high-performance distributed database like this VectorDB, **C++ exceptions are strictly prohibited**. Exceptions introduce hidden control flows, incur severe performance penalties on the "unhappy path", and are incredibly difficult to trace across thread pools and network RPC boundaries.

Instead, we use a paradigm popularized by Google (seen in `absl::Status` and `grpc::Status`): **Return Values as Error States**.

Every function that can fail must explicitly return its failure state. This forces developers to handle or propagate errors at the exact call site, guaranteeing system resilience.

## ⚙️ How It Works

This component provides three main pillars:

### 1. `StatusCode` (`status_code.h`)
A unified enumeration of every possible failure mode across the entire database. 
- **1-99**: General & Network errors (mapped 1:1 with gRPC status codes).
- **100-199**: Storage engine errors (e.g., IO, Corruption, Disk Full).
- **200-299**: RAFT consensus errors (e.g., Not Leader, Log Compacted).
- **300-399**: Vector Engine errors (e.g., Dimension Mismatch).

### 2. `Status` (`status.h`)
A lightweight object that carries a `StatusCode` and an optional human-readable `message`. Functions that perform an action but return no data return a `Status`.
```cpp
Status FlushToDisk() {
    if (disk_full) return Status(StatusCode::kStorageFull, "Disk is 100% full");
    return Status::Ok();
}
```

### 3. `ErrorOr<T>` (`error_or.h`)
A type-safe variant (similar to `std::expected` in C++23) that holds **either** a successful value `T` **or** a failure `Status`.
```cpp
ErrorOr<Vector> GetVector(int id) {
    if (!Exists(id)) return Status(StatusCode::kVectorNotFound, "ID missing");
    return ReadFromDisk(id);
}
```

### 4. Boilerplate Reduction Macros
To prevent the codebase from drowning in `if (!status.IsOk())` checks, we use macros to elegantly unwrap or propagate errors:
```cpp
// If GetVector fails, this returns the failure upwards immediately.
// If it succeeds, 'v' is initialized with the vector data.
ASSIGN_OR_RETURN(Vector v, GetVector(id));
```
