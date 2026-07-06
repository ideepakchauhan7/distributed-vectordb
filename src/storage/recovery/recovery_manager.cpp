#include "src/storage/recovery/recovery_manager.h"
#include "src/storage/wal/wal.h"
#include <filesystem>

namespace vectordb {
namespace storage {

using common::Status;
using common::StatusCode;
using common::ErrorOr;

ErrorOr<size_t> RecoveryManager::RecoverMemTable(
    const std::string& wal_path,
    MemTable& memtable,
    uint64_t last_flushed_lsn) {
    
    // If there is no WAL file yet, there's nothing to recover. This is normal on first boot.
    if (!std::filesystem::exists(wal_path)) {
        return 0;
    }

    // Open the WAL file (this handles missing directories gracefully)
    auto wal_or = WAL::Open(wal_path);
    if (!wal_or.IsOk()) {
        return wal_or.status();
    }
    auto& wal = wal_or.value();

    // ReadAll() safely stops at the first corrupted or partial entry,
    // which handles the case where the system crashed mid-write.
    auto entries_or = wal->ReadAll();
    if (!entries_or.IsOk()) {
        return entries_or.status();
    }
    
    size_t applied_count = 0;

    for (const auto& entry : entries_or.value()) {
        // Skip operations that have already been durably flushed to an SSTable.
        // This makes recovery idempotent.
        if (entry.sequence_number <= last_flushed_lsn) {
            continue;
        }

        // Apply the operation to the MemTable
        if (entry.type == WALEntryType::kPut) {
            memtable.Put(entry.key, entry.value);
            applied_count++;
        } else if (entry.type == WALEntryType::kDelete) {
            memtable.Delete(entry.key);
            applied_count++;
        } else {
            return Status(StatusCode::kStorageCorruption, "Unknown WAL entry type during recovery");
        }
    }

    return applied_count;
}

} // namespace storage
} // namespace vectordb
