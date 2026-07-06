#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

#include "src/common/error/error_or.h"
#include "src/storage/wal/wal.h"
#include "src/storage/wal/wal_entry.h"
#include "src/storage/memtable/memtable.h"
#include "src/storage/sstable/sstable_writer.h"
#include "src/storage/sstable/sstable_reader.h"
#include "src/storage/compaction/compaction_manager.h"
#include "src/storage/cache/block_cache.h"
#include "src/storage/checkpoint/checkpoint_manager.h"
#include "src/storage/recovery/recovery_manager.h"

namespace vectordb {
namespace storage {

/**
 * @struct StorageConfig
 * @brief All tunable parameters for the StorageEngine.
 */
struct StorageConfig {
    std::string data_dir = "/tmp/vectordb_data";
    std::string wal_dir  = "/tmp/vectordb_data/wal";
    std::string sst_dir  = "/tmp/vectordb_data/sst";

    size_t memtable_size_bytes = 64 * 1024 * 1024; // 64 MB — flush MemTable when this is exceeded
    size_t block_cache_mb      = 128;               // 128 MB LRU block cache
};

/**
 * @class StorageEngine
 * @brief The unified entry-point for all storage operations.
 *
 * Implements the LSM-Tree (Log-Structured Merge Tree) write path:
 *
 *   Write:  WAL (fsync) → MemTable → [if full: flush to SSTable + compact]
 *   Read:   Active MemTable → Immutable MemTable → BlockCache → SSTables (L0→LN)
 *   Delete: Writes a tombstone through the standard write path
 *
 * On startup, it automatically:
 *   1. Loads the last checkpoint (SSTable catalogue + last_flushed_lsn)
 *   2. Runs WAL recovery to rebuild the MemTable from unflushed writes
 *   3. Starts a background compaction thread
 *
 * Thread safety:
 *   - Writes serialize via wal_mutex_ (WAL append must be sequential)
 *   - Reads use shared_lock on memtable_mutex_ (multiple concurrent reads)
 *   - MemTable swap (during flush) briefly takes an exclusive lock
 *   - CompactionManager has its own internal locking
 */
class StorageEngine {
public:
    /**
     * @brief Opens (or creates) the storage engine at the given config.
     *        Performs WAL recovery and starts background compaction.
     */
    static common::ErrorOr<std::unique_ptr<StorageEngine>> Open(const StorageConfig& config);

    /**
     * @brief Shuts down the engine: stops background threads, flushes
     *        the active MemTable, and writes a final checkpoint.
     */
    ~StorageEngine();

    // Non-copyable, non-movable (owns threads and file handles)
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    /**
     * @brief Write a key-value pair.
     * Guarantees: WAL is fsync'd before returning OK.
     * @return Status::Ok() or a storage error.
     */
    common::Status Put(const std::string& key, const std::vector<uint8_t>& value);

    /**
     * @brief Delete a key by writing a tombstone.
     * @return Status::Ok() or a storage error.
     */
    common::Status Delete(const std::string& key);

    /**
     * @brief Look up a value by key.
     * Searches: Active MemTable → Immutable MemTable → BlockCache → SSTables
     * @return The value bytes if found, std::nullopt if not found, or an error.
     */
    common::ErrorOr<std::optional<std::vector<uint8_t>>> Get(const std::string& key);

    /**
     * @brief Synchronously flush the active MemTable to a Level 0 SSTable.
     * The engine does this automatically when the MemTable is full.
     * Exposed for testing and manual checkpointing.
     */
    common::Status FlushMemTable();

    // --- Observability ---
    size_t wal_writes() const { return wal_writes_.load(); }
    size_t memtable_flushes() const { return memtable_flushes_.load(); }
    size_t cache_hits() const { return block_cache_.hits(); }
    size_t cache_misses() const { return block_cache_.misses(); }

private:
    explicit StorageEngine(const StorageConfig& config);

    // --- Core Recovery (called by Open()) ---
    common::Status Recover();

    // --- Internal Write Helper ---
    // Called by Put and Delete. Always holds wal_mutex_.
    common::Status WriteEntry(WALEntryType type,
                               const std::string& key,
                               const std::vector<uint8_t>& value);

    // --- Background Compaction Thread ---
    void CompactionLoop();

    // --- SSTable Read Helper ---
    // Searches through all SSTable files (L0 → LN) for a key.
    common::ErrorOr<std::optional<std::vector<uint8_t>>> GetFromSSTables(const std::string& key);

    // --- Config ---
    StorageConfig config_;

    // --- WAL (Write-Ahead Log) ---
    // Serialized writes: only one thread appends to the WAL at a time.
    std::mutex wal_mutex_;
    std::unique_ptr<WAL> wal_;

    // --- MemTables ---
    // Active memtable accepts new writes; immutable is being flushed to SSTable.
    mutable std::shared_mutex memtable_mutex_;
    std::unique_ptr<MemTable> active_memtable_;
    std::unique_ptr<MemTable> immutable_memtable_; // nullptr when not flushing

    // --- SSTable Management ---
    CompactionManager compaction_manager_;
    BlockCache block_cache_;
    CheckpointManager checkpoint_manager_;

    // --- Background Compaction Thread ---
    std::thread compaction_thread_;
    std::atomic<bool> shutdown_{false};
    std::condition_variable compaction_cv_;
    std::mutex compaction_cv_mutex_;

    // --- Metrics ---
    std::atomic<size_t> wal_writes_{0};
    std::atomic<size_t> memtable_flushes_{0};

    // --- SSTable sequence counter ---
    std::atomic<uint64_t> next_sst_sequence_{1};
};

} // namespace storage
} // namespace vectordb
