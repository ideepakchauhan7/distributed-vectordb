#include "src/common/utils/hash.h"

namespace vectordb {
namespace common {

// Magic constants for FNV-1a 64-bit
static constexpr uint64_t kFnvPrime = 1099511628211ULL;
static constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;

uint64_t Hash::FNV1a64(const uint8_t* data, size_t length) {
    uint64_t hash = kFnvOffsetBasis;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t Hash::FNV1a64(const std::string& data) {
    return FNV1a64(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

} // namespace common
} // namespace vectordb
