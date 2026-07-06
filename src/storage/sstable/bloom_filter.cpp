#include "src/storage/sstable/bloom_filter.h"
#include <algorithm>
#include <cmath>

namespace vectordb {
namespace storage {

BloomFilter::BloomFilter(size_t expected_keys, int bits_per_key) {
    // Total bits = expected_keys * bits_per_key, minimum 64 bits
    num_bits_ = std::max<size_t>(expected_keys * bits_per_key, 64);

    // Round up to the nearest byte
    size_t num_bytes = (num_bits_ + 7) / 8;
    num_bits_ = num_bytes * 8; // Align to byte boundary
    bits_.resize(num_bytes, 0);

    // Optimal number of hash functions: k = ln(2) * (m/n)
    // For 10 bits/key: k = ln(2) * 10 ≈ 6.93 → 7
    num_hash_functions_ = static_cast<int>(std::round(std::log(2.0) * bits_per_key));
    num_hash_functions_ = std::max(1, std::min(num_hash_functions_, 30));
}

BloomFilter::BloomFilter(std::vector<uint8_t> bits, size_t num_bits, int num_hash_functions)
    : bits_(std::move(bits))
    , num_bits_(num_bits)
    , num_hash_functions_(num_hash_functions) {}

uint32_t BloomFilter::HashKey(const std::string& key, uint32_t seed) const {
    // FNV-1a hash seeded with a variable seed for each hash function
    uint32_t hash = 2166136261u ^ seed;
    for (char c : key) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u; // FNV prime
    }
    return hash;
}

void BloomFilter::Add(const std::string& key) {
    // Double-hashing: h_i(key) = (h1 + i * h2) % num_bits
    uint32_t h1 = HashKey(key, 0xbc9f1d34);
    uint32_t h2 = HashKey(key, 0x9747b28c);

    for (int i = 0; i < num_hash_functions_; ++i) {
        uint32_t bit_pos = (h1 + i * h2) % num_bits_;
        bits_[bit_pos / 8] |= (1 << (bit_pos % 8));
    }
}

bool BloomFilter::MayContain(const std::string& key) const {
    uint32_t h1 = HashKey(key, 0xbc9f1d34);
    uint32_t h2 = HashKey(key, 0x9747b28c);

    for (int i = 0; i < num_hash_functions_; ++i) {
        uint32_t bit_pos = (h1 + i * h2) % num_bits_;
        if ((bits_[bit_pos / 8] & (1 << (bit_pos % 8))) == 0) {
            return false; // Definitely NOT in the set
        }
    }
    return true; // Possibly in the set
}

} // namespace storage
} // namespace vectordb
