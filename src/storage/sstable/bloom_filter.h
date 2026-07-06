#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @class BloomFilter
 * @brief Space-efficient probabilistic data structure for set membership testing.
 *
 * Given a set of keys, the bloom filter answers:
 *   - "Definitely NOT in the set" (100% accurate)
 *   - "POSSIBLY in the set" (small false positive rate, ~1% at 10 bits/key)
 *
 * Used by the SSTable reader to avoid reading data blocks from disk
 * when a key is guaranteed not to exist in the file.
 *
 * Uses double-hashing to generate k hash probes from two base hashes:
 *   h_i(key) = (h1(key) + i * h2(key)) % num_bits
 */
class BloomFilter {
public:
    /**
     * @brief Construct a bloom filter sized for an expected number of keys.
     * @param expected_keys Number of keys expected to be inserted.
     * @param bits_per_key Bits allocated per key (10 = ~1% FPR).
     */
    explicit BloomFilter(size_t expected_keys, int bits_per_key = 10);

    /**
     * @brief Construct from pre-existing raw data (for deserialization).
     */
    BloomFilter(std::vector<uint8_t> bits, size_t num_bits, int num_hash_functions);

    void Add(const std::string& key);
    bool MayContain(const std::string& key) const;

    // Accessors for serialization
    const std::vector<uint8_t>& Data() const { return bits_; }
    size_t NumBits() const { return num_bits_; }
    int NumHashFunctions() const { return num_hash_functions_; }

private:
    uint32_t HashKey(const std::string& key, uint32_t seed) const;

    std::vector<uint8_t> bits_;
    size_t num_bits_;
    int num_hash_functions_;
};

} // namespace storage
} // namespace vectordb
