#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "src/common/error/error_or.h"

namespace vectordb {
namespace common {

/**
 * @class BinarySerializer
 * @brief High-performance zero-padding binary serialization for WAL and storage.
 * 
 * Optimized for floats and vectors. Much faster and memory efficient than 
 * protobuf for persisting raw floating-point arrays to disk.
 * Exception-free implementation using ErrorOr.
 */
class BinarySerializer {
public:
    // Write methods (Append to buffer)
    static void WriteUint32(std::vector<uint8_t>& buffer, uint32_t value);
    static void WriteUint64(std::vector<uint8_t>& buffer, uint64_t value);
    static void WriteFloat(std::vector<uint8_t>& buffer, float value);
    static void WriteString(std::vector<uint8_t>& buffer, const std::string& value);
    static void WriteFloatVector(std::vector<uint8_t>& buffer, const std::vector<float>& vec);

    // Read methods (Parse from buffer and advance offset)
    static ErrorOr<uint32_t> ReadUint32(const std::vector<uint8_t>& buffer, size_t& offset);
    static ErrorOr<uint64_t> ReadUint64(const std::vector<uint8_t>& buffer, size_t& offset);
    static ErrorOr<float> ReadFloat(const std::vector<uint8_t>& buffer, size_t& offset);
    static ErrorOr<std::string> ReadString(const std::vector<uint8_t>& buffer, size_t& offset);
    static ErrorOr<std::vector<float>> ReadFloatVector(const std::vector<uint8_t>& buffer, size_t& offset);
};

} // namespace common
} // namespace vectordb
