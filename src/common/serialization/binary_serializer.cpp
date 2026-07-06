#include "src/common/serialization/binary_serializer.h"
#include <cstring>

namespace vectordb {
namespace common {

#define CHECK_BOUNDS(buffer, offset, required) \
    if (offset + required > buffer.size()) { \
        return Status(StatusCode::kStorageCorruption, "BinarySerializer: Buffer overflow/underflow detected"); \
    }

// Write methods
void BinarySerializer::WriteUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    size_t old_size = buffer.size();
    buffer.resize(old_size + sizeof(uint32_t));
    std::memcpy(buffer.data() + old_size, &value, sizeof(uint32_t));
}

void BinarySerializer::WriteUint64(std::vector<uint8_t>& buffer, uint64_t value) {
    size_t old_size = buffer.size();
    buffer.resize(old_size + sizeof(uint64_t));
    std::memcpy(buffer.data() + old_size, &value, sizeof(uint64_t));
}

void BinarySerializer::WriteFloat(std::vector<uint8_t>& buffer, float value) {
    size_t old_size = buffer.size();
    buffer.resize(old_size + sizeof(float));
    std::memcpy(buffer.data() + old_size, &value, sizeof(float));
}

void BinarySerializer::WriteString(std::vector<uint8_t>& buffer, const std::string& value) {
    WriteUint32(buffer, static_cast<uint32_t>(value.size()));
    size_t old_size = buffer.size();
    buffer.resize(old_size + value.size());
    std::memcpy(buffer.data() + old_size, value.data(), value.size());
}

void BinarySerializer::WriteFloatVector(std::vector<uint8_t>& buffer, const std::vector<float>& vec) {
    WriteUint32(buffer, static_cast<uint32_t>(vec.size()));
    size_t bytes = vec.size() * sizeof(float);
    size_t old_size = buffer.size();
    buffer.resize(old_size + bytes);
    std::memcpy(buffer.data() + old_size, vec.data(), bytes);
}

// Read methods
ErrorOr<uint32_t> BinarySerializer::ReadUint32(const std::vector<uint8_t>& buffer, size_t& offset) {
    CHECK_BOUNDS(buffer, offset, sizeof(uint32_t));
    uint32_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    return value;
}

ErrorOr<uint64_t> BinarySerializer::ReadUint64(const std::vector<uint8_t>& buffer, size_t& offset) {
    CHECK_BOUNDS(buffer, offset, sizeof(uint64_t));
    uint64_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    return value;
}

ErrorOr<float> BinarySerializer::ReadFloat(const std::vector<uint8_t>& buffer, size_t& offset) {
    CHECK_BOUNDS(buffer, offset, sizeof(float));
    float value;
    std::memcpy(&value, buffer.data() + offset, sizeof(float));
    offset += sizeof(float);
    return value;
}

ErrorOr<std::string> BinarySerializer::ReadString(const std::vector<uint8_t>& buffer, size_t& offset) {
    ASSIGN_OR_RETURN(uint32_t length, ReadUint32(buffer, offset));
    CHECK_BOUNDS(buffer, offset, length);
    std::string value(reinterpret_cast<const char*>(buffer.data() + offset), length);
    offset += length;
    return value;
}

ErrorOr<std::vector<float>> BinarySerializer::ReadFloatVector(const std::vector<uint8_t>& buffer, size_t& offset) {
    ASSIGN_OR_RETURN(uint32_t length, ReadUint32(buffer, offset));
    size_t bytes = length * sizeof(float);
    CHECK_BOUNDS(buffer, offset, bytes);
    
    std::vector<float> vec(length);
    std::memcpy(vec.data(), buffer.data() + offset, bytes);
    offset += bytes;
    return vec;
}

} // namespace common
} // namespace vectordb
