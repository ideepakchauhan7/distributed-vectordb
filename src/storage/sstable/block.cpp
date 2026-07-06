#include "src/storage/sstable/block.h"
#include "src/common/serialization/binary_serializer.h"

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::ErrorOr;
using common::BinarySerializer;

bool Block::Add(const BlockEntry& entry) {
    size_t entry_size = entry.EncodedSize();
    // Allow at least one entry per block, even if it exceeds target size
    if (!entries_.empty() && (current_size_ + entry_size > kTargetBlockSize)) {
        return false;
    }
    entries_.push_back(entry);
    current_size_ += entry_size;
    return true;
}

std::string Block::LastKey() const {
    if (entries_.empty()) return "";
    return entries_.back().key;
}

std::vector<uint8_t> Block::Serialize() const {
    std::vector<uint8_t> buffer;
    buffer.reserve(current_size_);

    // Header: number of entries
    BinarySerializer::WriteUint32(buffer, static_cast<uint32_t>(entries_.size()));

    // Each entry
    for (const auto& entry : entries_) {
        // Key
        BinarySerializer::WriteUint32(buffer, static_cast<uint32_t>(entry.key.size()));
        buffer.insert(buffer.end(), entry.key.begin(), entry.key.end());

        // Value
        BinarySerializer::WriteUint32(buffer, static_cast<uint32_t>(entry.value.size()));
        buffer.insert(buffer.end(), entry.value.begin(), entry.value.end());

        // Tombstone flag
        buffer.push_back(entry.is_tombstone ? 1 : 0);
    }

    return buffer;
}

ErrorOr<Block> Block::Deserialize(const std::vector<uint8_t>& data) {
    size_t offset = 0;

    // Read number of entries
    ASSIGN_OR_RETURN(auto num_entries, BinarySerializer::ReadUint32(data, offset));

    Block block;
    for (uint32_t i = 0; i < num_entries; ++i) {
        BlockEntry entry;

        // Read key
        ASSIGN_OR_RETURN(auto key_len, BinarySerializer::ReadUint32(data, offset));
        if (offset + key_len > data.size()) {
            return Status(StatusCode::kStorageCorruption, "Block key extends past data");
        }
        entry.key = std::string(reinterpret_cast<const char*>(data.data() + offset), key_len);
        offset += key_len;

        // Read value
        ASSIGN_OR_RETURN(auto val_len, BinarySerializer::ReadUint32(data, offset));
        if (offset + val_len > data.size()) {
            return Status(StatusCode::kStorageCorruption, "Block value extends past data");
        }
        entry.value.assign(data.data() + offset, data.data() + offset + val_len);
        offset += val_len;

        // Read tombstone flag
        if (offset >= data.size()) {
            return Status(StatusCode::kStorageCorruption, "Block tombstone flag missing");
        }
        entry.is_tombstone = (data[offset++] != 0);

        block.entries_.push_back(std::move(entry));
        block.current_size_ += entry.EncodedSize();
    }

    return block;
}

std::optional<BlockEntry> Block::Get(const std::string& key) const {
    // Binary search since entries are sorted by key
    int lo = 0;
    int hi = static_cast<int>(entries_.size()) - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = entries_[mid].key.compare(key);

        if (cmp == 0) {
            return entries_[mid];
        } else if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return std::nullopt;
}

} // namespace storage
} // namespace vectordb
