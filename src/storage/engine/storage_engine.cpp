#include "src/storage/engine/storage_engine.h"
#include <filesystem>
#include <chrono>
#include <iostream>

namespace vectordb {
namespace storage {

// ─────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────

StorageEngine::StorageEngine(const StorageConfig& config)
    : config_(config)
    , compaction_manager_(config.sst_dir)
    , block_cache_(config.block_cache_mb)
    , checkpoint_manager_(config.data_dir) {
}

StorageEngine::~StorageEngine() {
    // 1. Signal background thread to stop
    shutdown_.store(true);
    compaction_cv_.notify_all();
    if (compaction_thread_.joinable()) {
        compaction_thread_.join();
    }

    // 2. Flush any remaining data in the active MemTable
    if (active_memtable_ && !active_memtable_->IsEmpty()) {
        auto status = FlushMemTable();
        if (!status.IsOk()) {
            std::cerr << "[StorageEngine] Warning: final flush failed: " 
                      << status.message() << std::endl;
        }
    }
}

// ─────────────────────────────────────────────────────
// Open (Static Factory)
// ─────────────────────────────────────────────────────

common::ErrorOr<std::unique_ptr<StorageEngine>> StorageEngine::Open(const StorageConfig& config) {
    // Use raw new so the private constructor is accessible
    auto engine = std::unique_ptr<StorageEngine>(new StorageEngine(config));

    // Ensure all directories exist
    std::filesystem::create_directories(config.data_dir);
    std::filesystem::create_directories(config.wal_dir);
    std::filesystem::create_directories(config.sst_dir);

    // Run recovery (load checkpoint → replay WAL)
    auto status = engine->Recover();
    if (!status.IsOk()) {
        return status;
    }

    // Start background compaction thread
    engine->compaction_thread_ = std::thread([&e = *engine]() {
        e.CompactionLoop();
    });

    return engine;
}

// ─────────────────────────────────────────────────────
// Recovery
// ─────────────────────────────────────────────────────

common::Status StorageEngine::Recover() {
    // Step 1: Load the last checkpoint to get the SSTable catalogue
    auto cp_result = checkpoint_manager_.LoadCheckpoint();
    if (!cp_result.IsOk()) {
        return cp_result.status();
    }
    const Checkpoint& cp = cp_result.value();

    // Step 2: Restore the CompactionManager's catalogue from the checkpoint
    for (const auto& meta : cp.sstables) {
        auto s = compaction_manager_.AddFile(meta);
        if (!s.IsOk()) return s;
    }

    // Step 3: Initialise the MemTable
    active_memtable_ = std::make_unique<MemTable>();

    // Step 4: Open the WAL (create one if it doesn't exist)
    std::string wal_path = config_.wal_dir + "/wal_current.log";
    auto wal_result = WAL::Open(wal_path);
    if (!wal_result.IsOk()) {
        return wal_result.status();
    }
    wal_ = std::move(wal_result.value());

    // Step 5: Replay WAL entries that happened after the last flush
    auto recover_result = RecoveryManager::RecoverMemTable(
        wal_path, *active_memtable_, cp.last_flushed_lsn);
    if (!recover_result.IsOk()) {
        return recover_result.status();
    }

    return common::Status::Ok();
}

// ─────────────────────────────────────────────────────
// Put
// ─────────────────────────────────────────────────────

common::Status StorageEngine::Put(const std::string& key,
                                   const std::vector<uint8_t>& value) {
    return WriteEntry(WALEntryType::kPut, key, value);
}

// ─────────────────────────────────────────────────────
// Delete
// ─────────────────────────────────────────────────────

common::Status StorageEngine::Delete(const std::string& key) {
    return WriteEntry(WALEntryType::kDelete, key, {});
}

// ─────────────────────────────────────────────────────
// Internal Write (shared by Put and Delete)
// ─────────────────────────────────────────────────────

common::Status StorageEngine::WriteEntry(WALEntryType type,
                                          const std::string& key,
                                          const std::vector<uint8_t>& value) {
    // STEP 1: Write to WAL (must succeed before touching MemTable)
    {
        std::lock_guard<std::mutex> wal_lock(wal_mutex_);

        WALEntry entry;
        entry.type  = type;
        entry.key   = key;
        entry.value = value;

        auto result = wal_->Append(entry);
        if (!result.IsOk()) {
            return result.status();
        }
        wal_writes_.fetch_add(1);
    }

    // STEP 2: Write to MemTable (WAL is already safely on disk)
    bool needs_flush = false;
    {
        std::unique_lock<std::shared_mutex> lock(memtable_mutex_);

        if (type == WALEntryType::kPut) {
            active_memtable_->Put(key, value);
        } else {
            active_memtable_->Delete(key);
        }

        // Check if we've crossed the MemTable size threshold
        needs_flush = (active_memtable_->ApproximateMemoryUsage() >= config_.memtable_size_bytes);
    }

    // STEP 3: Trigger flush if MemTable is full (outside the lock)
    if (needs_flush) {
        auto status = FlushMemTable();
        if (!status.IsOk()) { std::cerr << "SSTableWriter::Write failed code " << static_cast<int>(status.code()) << " msg " << status.message() << std::endl; return status; }
    }

    return common::Status::Ok();
}

// ─────────────────────────────────────────────────────
// Get
// ─────────────────────────────────────────────────────

common::ErrorOr<std::optional<std::vector<uint8_t>>>
StorageEngine::Get(const std::string& key) {
    // STEP 1: Check active MemTable (fastest — in RAM)
    {
        std::shared_lock<std::shared_mutex> lock(memtable_mutex_);

        auto result = active_memtable_->Get(key);
        if (result.has_value()) {
            return result;  // Found in active MemTable
        }

        // STEP 2: Check immutable MemTable (if a flush is in progress)
        if (immutable_memtable_) {
            result = immutable_memtable_->Get(key);
            if (result.has_value()) {
                return result;
            }
        }
    }

    // STEP 3: Search SSTables on disk (L0 → LN)
    return GetFromSSTables(key);
}

// ─────────────────────────────────────────────────────
// SSTable Read
// ─────────────────────────────────────────────────────

common::ErrorOr<std::optional<std::vector<uint8_t>>>
StorageEngine::GetFromSSTables(const std::string& key) {
    // Get all SSTable files, sorted from most recent (L0) to oldest
    auto all_files = compaction_manager_.AllFiles();

    // Sort: L0 files first (newest), then L1, L2, … (oldest)
    // Within L0, higher sequence_number = newer
    std::sort(all_files.begin(), all_files.end(),
              [](const SSTableMeta& a, const SSTableMeta& b) {
                  if (a.level != b.level) return a.level < b.level;
                  return a.sequence_number > b.sequence_number;
              });

    for (const auto& meta : all_files) {
        // Quick key range check before even opening the file
        if (key < meta.smallest_key || key > meta.largest_key) {
            continue;
        }

        // Check the BlockCache first
        std::string cache_key = BlockCache::MakeCacheKey(meta.filepath, 0 /*index_offset*/);
        // (A production implementation caches individual blocks; here we open the reader)

        auto reader_result = SSTableReader::Open(meta.filepath);
        if (!reader_result.IsOk()) {
            return reader_result.status();
        }
        auto& reader = reader_result.value();

        auto entry_result = reader->Get(key);
        if (!entry_result.IsOk()) {
            return entry_result.status();
        }

        auto& entry = entry_result.value();
        if (entry.has_value()) {
            // Key found. If it's a tombstone, it's deleted.
            if (entry->is_tombstone) {
                return std::optional<std::vector<uint8_t>>(std::nullopt);
            }
            return std::optional<std::vector<uint8_t>>(entry->value);
        }
    }

    // Key not found in any SSTable
    return std::optional<std::vector<uint8_t>>(std::nullopt);
}

// ─────────────────────────────────────────────────────
// FlushMemTable
// ─────────────────────────────────────────────────────

common::Status StorageEngine::FlushMemTable() {
    // STEP 1: Atomically swap the active MemTable with a fresh one.
    //         The old one becomes the "immutable" MemTable.
    std::unique_ptr<MemTable> to_flush;
    uint64_t flush_lsn;
    {
        std::unique_lock<std::shared_mutex> lock(memtable_mutex_);

        // If the MemTable is empty, nothing to do
        if (active_memtable_->IsEmpty()) return common::Status::Ok();

        to_flush = std::move(active_memtable_);
        immutable_memtable_.reset(); // Release old immutable (if any) 
        immutable_memtable_ = nullptr;
        active_memtable_ = std::make_unique<MemTable>();
    }

    // Capture current WAL LSN as the checkpoint marker
    {
        std::lock_guard<std::mutex> wal_lock(wal_mutex_);
        flush_lsn = wal_->current_sequence_number();
    }

    // STEP 2: Write the immutable MemTable to a new Level 0 SSTable
    uint64_t seq = next_sst_sequence_.fetch_add(1);
    std::string sst_path = config_.sst_dir + "/level0_" + 
                           std::to_string(seq) + ".sst";

    auto status = SSTableWriter::Write(sst_path, to_flush->Begin());
    if (!status.IsOk()) { std::cerr << "SSTableWriter::Write failed code " << static_cast<int>(status.code()) << " msg " << status.message() << std::endl; return status; }

    // STEP 3: Build SSTableMeta and register with CompactionManager
    SSTableMeta meta;
    meta.filepath = sst_path;
    meta.level    = 0;
    meta.sequence_number = seq;
    meta.file_size_bytes = std::filesystem::file_size(sst_path);

    // Get key range by reading the newly written file
    {
        auto reader_result = SSTableReader::Open(sst_path);
        if (reader_result.IsOk()) {
            auto entries_result = reader_result.value()->ReadAll();
            if (entries_result.IsOk() && !entries_result.value().empty()) {
                meta.smallest_key = entries_result.value().front().key;
                meta.largest_key  = entries_result.value().back().key;
            }
        }
    }

    auto add_status = compaction_manager_.AddFile(meta);
    if (!add_status.IsOk()) { std::cerr << "AddFile failed" << std::endl; return add_status; }

    // STEP 4: Write a checkpoint recording the new SSTable and the flush LSN
    Checkpoint cp;
    cp.last_flushed_lsn = flush_lsn;
    cp.sstables = compaction_manager_.AllFiles();
    auto cp_status = checkpoint_manager_.SaveCheckpoint(cp);
    if (!cp_status.IsOk()) { std::cerr << "SaveCheckpoint failed" << std::endl; return cp_status; }

    // STEP 5: Clear the immutable MemTable reference (done flushing)
    {
        std::unique_lock<std::shared_mutex> lock(memtable_mutex_);
        immutable_memtable_ = nullptr;
    }

    memtable_flushes_.fetch_add(1);

    // STEP 6: Wake the compaction thread — we just added a new L0 file
    compaction_cv_.notify_one();

    return common::Status::Ok();
}

// ─────────────────────────────────────────────────────
// Background Compaction Loop
// ─────────────────────────────────────────────────────

void StorageEngine::CompactionLoop() {
    while (!shutdown_.load()) {
        // Wait until woken (after a MemTable flush) or 5 seconds elapse
        {
            std::unique_lock<std::mutex> lock(compaction_cv_mutex_);
            compaction_cv_.wait_for(lock, std::chrono::seconds(5),
                                    [this] { return shutdown_.load() || 
                                             compaction_manager_.NeedsCompaction(); });
        }

        if (shutdown_.load()) break;

        if (compaction_manager_.NeedsCompaction()) {
            auto result = compaction_manager_.RunCompaction();
            if (!result.IsOk()) {
                std::cerr << "[StorageEngine] Compaction error: " 
                          << result.status().message() << std::endl;
            }

            // Update checkpoint after compaction (SSTable catalogue changed)
            Checkpoint cp;
            {
                std::lock_guard<std::mutex> wal_lock(wal_mutex_);
                cp.last_flushed_lsn = wal_->current_sequence_number();
            }
            cp.sstables = compaction_manager_.AllFiles();
            checkpoint_manager_.SaveCheckpoint(cp);
        }
    }
}

} // namespace storage
} // namespace vectordb
