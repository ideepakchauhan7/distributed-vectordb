#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace vectordb {
namespace storage {

/**
 * @brief Type of operation recorded in the WAL.
 */
enum class WALEntryType : uint8_t {
    kPut    = 0x01,  // Insert or update a key-value pair
    kDelete = 0x02,  // Delete a key
};

/**
 * @struct WALEntry
 * @brief Represents a single record in the Write-Ahead Log.
 *
 * On-disk binary layout:
 * ┌──────────┬──────────┬──────────┬───────────────────┐
 * │ Length   │ CRC32    │ Type     │ Payload           │
 * │ (4 bytes)│ (4 bytes)│ (1 byte) │ (variable)        │
 * └──────────┴──────────┴──────────┴───────────────────┘
 *
 * Payload for kPut:    [key_len(4)] [key_data] [value_len(4)] [value_data]
 * Payload for kDelete: [key_len(4)] [key_data]
 */
struct WALEntry {
    uint64_t sequence_number;      // Monotonically increasing LSN
    WALEntryType type;
    std::string key;
    std::vector<uint8_t> value;    // Empty for deletes

    // Total serialized size (excluding the length/crc header)
    size_t PayloadSize() const;
};

// Header size: 4 (length) + 4 (crc32) = 8 bytes
static constexpr size_t kWALHeaderSize = 8;

} // namespace storage
} // namespace vectordb
