#include "src/storage/sstable/sstable_writer.h"
#include "src/storage/sstable/block.h"
#include "src/storage/sstable/bloom_filter.h"
#include "src/common/serialization/binary_serializer.h"
#include <fstream>
#include <filesystem>

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::BinarySerializer;

// ─── Internal: Index entry stored during writing ─────────────────────

struct IndexEntry {
    std::string last_key;   // Last key in the data block
    uint64_t block_offset;  // Byte offset of the block in the file
    uint32_t block_size;    // Byte size of the serialized block
};

Status SSTableWriter::Write(const std::string& filepath, SkipList::Iterator iter) {
    // Create parent directories if needed
    auto parent = std::filesystem::path(filepath).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Status(StatusCode::kStorageIoError, "Cannot create SSTable: " + filepath);
    }

    // ── Phase 1: Count keys for bloom filter sizing ─────────────────
    // We iterate once to count, but our SkipList::Iterator is forward-only.
    // Instead, we'll use a generous initial estimate and grow the bloom filter.
    // For simplicity, we build all entries into a vector first (they're already in memory).
    std::vector<BlockEntry> all_entries;
    for (; iter.Valid(); iter.Next()) {
        BlockEntry be;
        be.key = iter.Key();
        be.is_tombstone = iter.Value().is_tombstone;
        be.value = iter.Value().data;
        all_entries.push_back(std::move(be));
    }

    if (all_entries.empty()) {
        file.close();
        std::filesystem::remove(filepath);
        return Status(StatusCode::kInvalidArgument, "Cannot write empty SSTable");
    }

    // ── Phase 2: Build bloom filter ─────────────────────────────────
    BloomFilter bloom(all_entries.size(), 10); // 10 bits per key → ~1% FPR
    for (const auto& entry : all_entries) {
        bloom.Add(entry.key);
    }

    // ── Phase 3: Build data blocks and write them ───────────────────
    std::vector<IndexEntry> index;
    Block current_block;
    uint64_t file_offset = 0;
    size_t total_entries = 0;

    auto flush_block = [&]() -> Status {
        if (current_block.IsEmpty()) return Status::Ok();

        auto serialized = current_block.Serialize();
        file.write(reinterpret_cast<const char*>(serialized.data()),
                   static_cast<std::streamsize>(serialized.size()));
        if (!file.good()) {
            return Status(StatusCode::kStorageIoError, "SSTable block write failed");
        }

        IndexEntry idx;
        idx.last_key = current_block.LastKey();
        idx.block_offset = file_offset;
        idx.block_size = static_cast<uint32_t>(serialized.size());
        index.push_back(std::move(idx));

        total_entries += current_block.EntryCount();
        file_offset += serialized.size();
        current_block = Block(); // Reset
        return Status::Ok();
    };

    for (const auto& entry : all_entries) {
        if (!current_block.Add(entry)) {
            // Current block is full — flush it
            auto status = flush_block();
            if (!status.IsOk()) return status;

            // Add the entry to the fresh block
            current_block.Add(entry);
        }
    }
    // Flush the last partial block
    auto status = flush_block();
    if (!status.IsOk()) return status;

    // ── Phase 4: Write bloom filter block ───────────────────────────
    uint64_t bloom_offset = file_offset;

    // Bloom filter on-disk format:
    //   num_bits(4) + num_hash_functions(4) + raw_bit_array
    std::vector<uint8_t> bloom_block;
    BinarySerializer::WriteUint32(bloom_block, static_cast<uint32_t>(bloom.NumBits()));
    BinarySerializer::WriteUint32(bloom_block, static_cast<uint32_t>(bloom.NumHashFunctions()));
    bloom_block.insert(bloom_block.end(), bloom.Data().begin(), bloom.Data().end());

    file.write(reinterpret_cast<const char*>(bloom_block.data()),
               static_cast<std::streamsize>(bloom_block.size()));
    if (!file.good()) {
        return Status(StatusCode::kStorageIoError, "SSTable bloom write failed");
    }
    uint32_t bloom_size = static_cast<uint32_t>(bloom_block.size());
    file_offset += bloom_size;

    // ── Phase 5: Write index block ──────────────────────────────────
    uint64_t index_offset = file_offset;

    std::vector<uint8_t> index_block;
    BinarySerializer::WriteUint32(index_block, static_cast<uint32_t>(index.size()));
    for (const auto& idx : index) {
        // last_key_len(4) + last_key + block_offset(8) + block_size(4)
        BinarySerializer::WriteUint32(index_block, static_cast<uint32_t>(idx.last_key.size()));
        index_block.insert(index_block.end(), idx.last_key.begin(), idx.last_key.end());
        BinarySerializer::WriteUint64(index_block, idx.block_offset);
        BinarySerializer::WriteUint32(index_block, idx.block_size);
    }

    file.write(reinterpret_cast<const char*>(index_block.data()),
               static_cast<std::streamsize>(index_block.size()));
    if (!file.good()) {
        return Status(StatusCode::kStorageIoError, "SSTable index write failed");
    }
    uint32_t index_size = static_cast<uint32_t>(index_block.size());
    file_offset += index_size;

    // ── Phase 6: Write footer (48 bytes) ────────────────────────────
    // bloom_offset(8) + bloom_size(8) + index_offset(8) + index_size(8) +
    // num_entries(8) + magic(8)
    std::vector<uint8_t> footer;
    footer.reserve(kFooterSize);
    BinarySerializer::WriteUint64(footer, bloom_offset);
    BinarySerializer::WriteUint64(footer, static_cast<uint64_t>(bloom_size));
    BinarySerializer::WriteUint64(footer, index_offset);
    BinarySerializer::WriteUint64(footer, static_cast<uint64_t>(index_size));
    BinarySerializer::WriteUint64(footer, static_cast<uint64_t>(total_entries));
    BinarySerializer::WriteUint64(footer, kMagicNumber);

    file.write(reinterpret_cast<const char*>(footer.data()),
               static_cast<std::streamsize>(footer.size()));
    if (!file.good()) {
        return Status(StatusCode::kStorageIoError, "SSTable footer write failed");
    }

    file.flush();
    file.close();
    return Status::Ok();
}

} // namespace storage
} // namespace vectordb
