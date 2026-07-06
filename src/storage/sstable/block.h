#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/// Target block size: 4KB to match OS page size and SSD sector size
static constexpr size_t kTargetBlockSize = 4096;

/**
 * @struct BlockEntry
 * @brief A single key-value pair within a data block.
 */
struct BlockEntry {
    std::string key;
    std::vector<uint8_t> value;
    bool is_tombstone{false};

    /// Returns the on-disk encoded size of this entry
    size_t EncodedSize() const {
        // key_len(4) + key + value_len(4) + value + tombstone(1)
        return 4 + key.size() + 4 + value.size() + 1;
    }
};

/**
 * @class Block
 * @brief A fixed-size container of sorted key-value entries.
 *
 * On-disk binary layout:
 * ┌───────────────┬──────────┬──────────┬─────┬──────────┐
 * │ num_entries(4)│ Entry 0  │ Entry 1  │ ... │ Entry N  │
 * └───────────────┴──────────┴──────────┴─────┴──────────┘
 *
 * Each entry:
 * ┌──────────┬──────────┬──────────┬──────────┬───────────┐
 * │key_len(4)│ key data │val_len(4)│ val data │tombstone(1)│
 * └──────────┴──────────┴──────────┴──────────┴───────────┘
 */
class Block {
public:
    Block() = default;

    /**
     * @brief Attempts to add an entry to the block.
     * @return true if the entry was added, false if it would exceed kTargetBlockSize.
     */
    bool Add(const BlockEntry& entry);

    bool IsEmpty() const { return entries_.empty(); }
    size_t CurrentSize() const { return current_size_; }
    size_t EntryCount() const { return entries_.size(); }
    std::string LastKey() const;
    const std::vector<BlockEntry>& Entries() const { return entries_; }

    /// Serialize this block to a byte buffer using BinarySerializer.
    std::vector<uint8_t> Serialize() const;

    /// Deserialize a block from a byte buffer.
    static common::ErrorOr<Block> Deserialize(const std::vector<uint8_t>& data);

    /// Point lookup within this block (binary search, since keys are sorted).
    std::optional<BlockEntry> Get(const std::string& key) const;

private:
    std::vector<BlockEntry> entries_;
    size_t current_size_{4}; // Start at 4 bytes for the num_entries header
};

} // namespace storage
} // namespace vectordb
