#pragma once

#include <string>
#include <cstdint>
#include "src/storage/memtable/memtable.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @class RecoveryManager
 * @brief Handles crash recovery by replaying the Write-Ahead Log (WAL).
 *
 * When the database starts up (or restarts after a crash), the MemTable
 * is completely empty because it lives only in RAM. The RecoveryManager
 * reads the WAL from disk, filters out operations that have already been
 * durably flushed to SSTables, and replays the remaining operations into
 * a new MemTable to restore the database to its pre-crash state.
 */
class RecoveryManager {
public:
    /**
     * @brief Replays valid WAL entries into the provided MemTable.
     * 
     * @param wal_path The file path of the Write-Ahead Log to replay.
     * @param memtable The MemTable instance to populate.
     * @param last_flushed_lsn The highest LSN that was successfully flushed 
     *        to an SSTable. Entries with LSN <= this value are skipped.
     * @return The number of entries successfully applied to the MemTable, or an error.
     */
    static common::ErrorOr<size_t> RecoverMemTable(
        const std::string& wal_path,
        MemTable& memtable,
        uint64_t last_flushed_lsn = 0);
};

} // namespace storage
} // namespace vectordb
