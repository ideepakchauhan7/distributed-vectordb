#pragma once
#include <string>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <memory>
#include "src/storage/wal/wal_entry.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @class WAL
 * @brief Append-only Write-Ahead Log for crash recovery.
 *
 * Every mutation (put/delete) is appended to the WAL before being applied
 * to the MemTable. On crash, the WAL is replayed to reconstruct state.
 *
 * Thread-safety: All public methods are mutex-protected.
 */
class WAL {
public:
    /**
     * @brief Opens or creates a WAL file at the given path.
     * @param path File path for the WAL (e.g., "/data/wal/wal_00001.log")
     */
    static common::ErrorOr<std::unique_ptr<WAL>> Open(const std::string& path);

    ~WAL();

    /**
     * @brief Appends an entry to the WAL and fsyncs to disk.
     * @return The sequence number assigned to this entry, or an error.
     */
    common::ErrorOr<uint64_t> Append(const WALEntry& entry);

    /**
     * @brief Reads all valid entries from the WAL file.
     * Stops at the first corrupted/partial entry (truncates it).
     */
    common::ErrorOr<std::vector<WALEntry>> ReadAll();

    /**
     * @brief Syncs all buffered data to disk.
     */
    common::Status Sync();

    /**
     * @brief Returns the current sequence number (last written LSN).
     */
    uint64_t current_sequence_number() const { return next_sequence_number_ - 1; }

    /**
     * @brief Returns the file path of this WAL.
     */
    const std::string& path() const { return path_; }

private:
    explicit WAL(const std::string& path, std::ofstream file, uint64_t next_seq);

    // Serialization helpers
    static std::vector<uint8_t> SerializeEntry(const WALEntry& entry);
    static common::ErrorOr<WALEntry> DeserializeEntry(const std::vector<uint8_t>& data);
    static uint32_t ComputeCRC32(const uint8_t* data, size_t length);

    std::string path_;
    std::ofstream write_file_;
    std::mutex mutex_;
    uint64_t next_sequence_number_;
};

} // namespace storage
} // namespace vectordb
