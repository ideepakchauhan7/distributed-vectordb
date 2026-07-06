#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "src/storage/sstable/block.h"
#include "src/storage/sstable/bloom_filter.h"
#include "src/storage/sstable/sstable_writer.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @class SSTableReader
 * @brief Reads an immutable SSTable file for point lookups and sequential scans.
 *
 * On Open(), loads the footer, index block, and bloom filter into memory.
 * Data blocks are read from disk on-demand during Get() or ReadAll().
 *
 * Read path for a point lookup:
 *   1. Check BloomFilter → if NO, return not-found immediately (zero disk I/O)
 *   2. Binary search the index to find which data block *might* contain the key
 *   3. Read that single data block from disk
 *   4. Binary search within the block for the key
 */
class SSTableReader {
public:
    /**
     * @brief Opens an SSTable file and loads metadata (footer + index + bloom).
     * @param filepath Path to the .sst file.
     * @return A ready-to-query SSTableReader or an error.
     */
    static common::ErrorOr<std::unique_ptr<SSTableReader>> Open(const std::string& filepath);

    /**
     * @brief Point lookup for a key.
     * @return The BlockEntry if found, std::nullopt if not found, or an error.
     */
    common::ErrorOr<std::optional<BlockEntry>> Get(const std::string& key) const;

    /**
     * @brief Reads ALL entries from the SSTable sequentially (for compaction).
     * @return A sorted vector of all BlockEntries in the file.
     */
    common::ErrorOr<std::vector<BlockEntry>> ReadAll() const;

    size_t NumEntries() const { return num_entries_; }
    const std::string& FilePath() const { return filepath_; }

private:
    SSTableReader() : bloom_(0) {} // Placeholder bloom, replaced during Open()

    struct IndexEntry {
        std::string last_key;
        uint64_t block_offset;
        uint32_t block_size;
    };

    /// Read a single data block from disk at the given offset and size
    common::ErrorOr<Block> ReadBlock(uint64_t offset, uint32_t size) const;

    std::string filepath_;
    BloomFilter bloom_;
    std::vector<IndexEntry> index_;
    size_t num_entries_{0};
};

} // namespace storage
} // namespace vectordb
