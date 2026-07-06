#include "src/storage/sstable/sstable_reader.h"
#include "src/common/serialization/binary_serializer.h"
#include <fstream>
#include <filesystem>

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::ErrorOr;
using common::BinarySerializer;

ErrorOr<std::unique_ptr<SSTableReader>> SSTableReader::Open(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        return Status(StatusCode::kStorageIoError, "SSTable file not found: " + filepath);
    }

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Status(StatusCode::kStorageIoError, "Cannot open SSTable: " + filepath);
    }

    auto file_size = file.tellg();
    if (static_cast<size_t>(file_size) < SSTableWriter::kFooterSize) {
        return Status(StatusCode::kStorageCorruption, "SSTable too small for footer: " + filepath);
    }

    // ── Step 1: Read footer (last 48 bytes) ─────────────────────────
    file.seekg(-static_cast<std::streamoff>(SSTableWriter::kFooterSize), std::ios::end);
    std::vector<uint8_t> footer_buf(SSTableWriter::kFooterSize);
    file.read(reinterpret_cast<char*>(footer_buf.data()), SSTableWriter::kFooterSize);

    size_t offset = 0;
    ASSIGN_OR_RETURN(auto bloom_offset,  BinarySerializer::ReadUint64(footer_buf, offset));
    ASSIGN_OR_RETURN(auto bloom_size,    BinarySerializer::ReadUint64(footer_buf, offset));
    ASSIGN_OR_RETURN(auto index_offset,  BinarySerializer::ReadUint64(footer_buf, offset));
    ASSIGN_OR_RETURN(auto index_size,    BinarySerializer::ReadUint64(footer_buf, offset));
    ASSIGN_OR_RETURN(auto num_entries,   BinarySerializer::ReadUint64(footer_buf, offset));
    ASSIGN_OR_RETURN(auto magic,         BinarySerializer::ReadUint64(footer_buf, offset));

    if (magic != SSTableWriter::kMagicNumber) {
        return Status(StatusCode::kStorageCorruption, "Invalid SSTable magic number: " + filepath);
    }

    // ── Step 2: Read bloom filter block ─────────────────────────────
    file.seekg(static_cast<std::streamoff>(bloom_offset));
    std::vector<uint8_t> bloom_buf(bloom_size);
    file.read(reinterpret_cast<char*>(bloom_buf.data()), static_cast<std::streamsize>(bloom_size));
    if (!file.good()) {
        return Status(StatusCode::kStorageCorruption, "Failed to read bloom filter block");
    }

    size_t bloom_off = 0;
    ASSIGN_OR_RETURN(auto num_bits,      BinarySerializer::ReadUint32(bloom_buf, bloom_off));
    ASSIGN_OR_RETURN(auto num_hash_fns,  BinarySerializer::ReadUint32(bloom_buf, bloom_off));

    std::vector<uint8_t> bloom_data(bloom_buf.begin() + bloom_off, bloom_buf.end());
    BloomFilter bloom(std::move(bloom_data), num_bits, static_cast<int>(num_hash_fns));

    // ── Step 3: Read index block ────────────────────────────────────
    file.seekg(static_cast<std::streamoff>(index_offset));
    std::vector<uint8_t> index_buf(index_size);
    file.read(reinterpret_cast<char*>(index_buf.data()), static_cast<std::streamsize>(index_size));
    if (!file.good()) {
        return Status(StatusCode::kStorageCorruption, "Failed to read index block");
    }

    size_t idx_off = 0;
    ASSIGN_OR_RETURN(auto num_index_entries, BinarySerializer::ReadUint32(index_buf, idx_off));

    std::vector<IndexEntry> index;
    index.reserve(num_index_entries);

    for (uint32_t i = 0; i < num_index_entries; ++i) {
        IndexEntry ie;

        ASSIGN_OR_RETURN(auto key_len, BinarySerializer::ReadUint32(index_buf, idx_off));
        if (idx_off + key_len > index_buf.size()) {
            return Status(StatusCode::kStorageCorruption, "Index key extends past block");
        }
        ie.last_key = std::string(reinterpret_cast<const char*>(index_buf.data() + idx_off), key_len);
        idx_off += key_len;

        ASSIGN_OR_RETURN(ie.block_offset, BinarySerializer::ReadUint64(index_buf, idx_off));
        ASSIGN_OR_RETURN(ie.block_size,   BinarySerializer::ReadUint32(index_buf, idx_off));

        index.push_back(std::move(ie));
    }

    // ── Build the reader ────────────────────────────────────────────
    auto reader = std::unique_ptr<SSTableReader>(new SSTableReader());
    reader->filepath_ = filepath;
    reader->bloom_ = std::move(bloom);
    reader->index_ = std::move(index);
    reader->num_entries_ = static_cast<size_t>(num_entries);

    return reader;
}

ErrorOr<Block> SSTableReader::ReadBlock(uint64_t block_offset, uint32_t block_size) const {
    std::ifstream file(filepath_, std::ios::binary);
    if (!file.is_open()) {
        return Status(StatusCode::kStorageIoError, "Cannot open SSTable for block read: " + filepath_);
    }

    file.seekg(static_cast<std::streamoff>(block_offset));
    std::vector<uint8_t> block_data(block_size);
    file.read(reinterpret_cast<char*>(block_data.data()), static_cast<std::streamsize>(block_size));

    if (file.gcount() < static_cast<std::streamsize>(block_size)) {
        return Status(StatusCode::kStorageCorruption, "Incomplete block read");
    }

    return Block::Deserialize(block_data);
}

ErrorOr<std::optional<BlockEntry>> SSTableReader::Get(const std::string& key) const {
    // ── Fast path: Bloom filter rejects the key with zero disk I/O ──
    if (!bloom_.MayContain(key)) {
        return std::optional<BlockEntry>(std::nullopt);
    }

    // ── Find the candidate block via the index ──────────────────────
    // Binary search: find the first index entry whose last_key >= key
    int lo = 0;
    int hi = static_cast<int>(index_.size()) - 1;
    int candidate = -1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (index_[mid].last_key >= key) {
            candidate = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    if (candidate == -1) {
        // Key is greater than all keys in this SSTable
        return std::optional<BlockEntry>(std::nullopt);
    }

    // ── Read the candidate block from disk ──────────────────────────
    ASSIGN_OR_RETURN(auto block, ReadBlock(index_[candidate].block_offset, 
                                            index_[candidate].block_size));

    // ── Binary search within the block ──────────────────────────────
    auto result = block.Get(key);
    return result;
}

ErrorOr<std::vector<BlockEntry>> SSTableReader::ReadAll() const {
    std::vector<BlockEntry> all_entries;
    all_entries.reserve(num_entries_);

    for (const auto& idx : index_) {
        ASSIGN_OR_RETURN(auto block, ReadBlock(idx.block_offset, idx.block_size));
        for (auto& entry : block.Entries()) {
            all_entries.push_back(entry);
        }
    }

    return all_entries;
}

} // namespace storage
} // namespace vectordb
