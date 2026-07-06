#pragma once
#include <string>
#include <cstdint>

namespace vectordb {
namespace common {

/**
 * @class Hash
 * @brief High-performance non-cryptographic hashing algorithms.
 */
class Hash {
public:
    /**
     * @brief Computes a 64-bit FNV-1a hash of a string.
     * Extremely fast hash function with an excellent avalanche effect.
     * Used for consistently hashing vector IDs or tenant keys to determine 
     * which database shard they belong to.
     */
    static uint64_t FNV1a64(const std::string& data);
    
    /**
     * @brief Computes a 64-bit FNV-1a hash of a raw byte array.
     */
    static uint64_t FNV1a64(const uint8_t* data, size_t length);
};

} // namespace common
} // namespace vectordb
