# Binary Serialization Subsystem (`common/serialization`)

## 📐 System Design Philosophy

While Protocol Buffers (gRPC) are excellent for routing network requests and serializing complex, nested metadata, they are fundamentally unsuited for persisting massive arrays of raw floating-point numbers to disk.

Protobuf uses a "tag-and-wire-type" encoding scheme. If you attempt to serialize a 1024-dimensional float vector, Protobuf will prefix *every single float* with a byte indicating its type, inflating the payload size by 25% and drastically reducing memory bandwidth efficiency during disk flushes.

Our **Custom Binary Serializer** solves this by enforcing rigid, zero-padded C++ memory layouts, allowing us to serialize data using raw `std::memcpy`.

## ⚙️ How It Works

### 1. `std::memcpy` for Extreme Throughput
When writing a `std::vector<float>` to the Write-Ahead Log (WAL), the serializer writes exactly 4 bytes representing the length of the array, followed instantly by a raw, unadulterated `memcpy` of the entire vector contiguous memory block. This is executed at the absolute limits of the CPU's L1 cache bandwidth.

### 2. Exception-Free Error Recovery (`ErrorOr`)
Standard binary deserialization is notoriously unsafe. If the database crashes mid-write, the WAL file on disk will be truncated. 

When the system restarts and attempts to read a 1024-float vector from a buffer that only has 500 floats remaining, standard C++ `std::vector::at()` or naive pointers would throw an exception or segfault.

We integrate directly with our Phase 1 `ErrorOr` paradigm:
```cpp
#define CHECK_BOUNDS(buffer, offset, required) \
    if (offset + required > buffer.size()) { \
        return Status(StatusCode::kStorageCorruption, "Buffer underflow"); \
    }
```
Every read method explicitly bounds-checks the offset and returns a graceful `StatusCode::kStorageCorruption`, allowing the RAFT engine to discard the truncated log entry and request the missing data from a peer, rather than crashing the node.
