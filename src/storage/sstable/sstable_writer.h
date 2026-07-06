#pragma once

#include <string>
#include <memory>
#include "src/storage/memtable/skip_list.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @class SSTableWriter
 * @brief Writes a sorted stream of key-value pairs to an immutable SSTable file.
 *
 * SSTable on-disk layout:
 * ┌───────────────────────────────────┐
 * │         Data Block 0 (~4KB)       │  ← Sorted key-value pairs
 * ├───────────────────────────────────┤
 * │         Data Block 1 (~4KB)       │
 * ├───────────────────────────────────┤
 * │              ...                  │
 * ├───────────────────────────────────┤
 * │         Data Block N (~4KB)       │
 * ├───────────────────────────────────┤
 * │         Bloom Filter Block        │  ← "Is key X possibly in this file?"
 * ├───────────────────────────────────┤
 * │         Index Block               │  ← Maps last_key → {offset, size}
 * ├───────────────────────────────────┤
 * │         Footer (48 bytes)         │  ← Offsets to bloom + index + metadata
 * └───────────────────────────────────┘
 *
 * Usage:
 *   auto it = memtable.Begin();
 *   auto status = SSTableWriter::Write("/data/sst_00001.sst", it);
 */
class SSTableWriter {
public:
    /**
     * @brief Writes an SSTable file from a sorted SkipList iterator.
     * @param filepath Path to the output .sst file.
     * @param iter A SkipList::Iterator positioned at the first element.
     * @return Status::Ok() on success, or an error status.
     */
    static common::Status Write(const std::string& filepath, SkipList::Iterator iter);

    /// Magic number written in the footer to validate file format
    static constexpr uint64_t kMagicNumber = 0x0053535461626C65; // "\0SStable"
    /// Fixed footer size in bytes
    static constexpr size_t kFooterSize = 48;
};

} // namespace storage
} // namespace vectordb
