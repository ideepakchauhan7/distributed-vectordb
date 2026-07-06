#pragma once

#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>
#include "src/storage/compaction/leveled_compaction.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace storage {

/**
 * @struct CompactionStats
 * @brief Captures metrics for a single compaction run.
 * Used for observability — feed into Phase 1 metrics counters.
 */
struct CompactionStats {
    int source_level{0};
    int target_level{0};
    size_t files_read{0};
    size_t files_written{0};
    uint64_t bytes_read{0};
    uint64_t bytes_written{0};
    uint64_t keys_merged{0};
    uint64_t tombstones_dropped{0};    // Tombstones dropped at the max level
    uint64_t duplicate_keys_dropped{0};
    double duration_ms{0.0};
};

/**
 * @class CompactionManager
 * @brief Orchestrates leveled SSTable merging in the background.
 *
 * Responsibilities:
 *  1. Maintain a catalogue of all live SSTable files across all levels.
 *  2. Decide WHEN to compact (size/count thresholds per level).
 *  3. Execute compaction: read N files → merge-sort → write new files → delete old.
 *  4. All compactions run on a background thread; the write path is NEVER blocked.
 *
 * Thread safety:
 *  - The file catalogue (files_) is protected by a shared_mutex.
 *  - Multiple readers (queries scanning level metadata) and one writer (compaction) are safe.
 *  - Compaction itself is serialized via compaction_mutex_.
 */
class CompactionManager {
public:
    /**
     * @param data_dir  Root directory where SSTable files live.
     * @param max_level Maximum number of levels (default: 7).
     */
    explicit CompactionManager(std::string data_dir, int max_level = leveled::kNumLevels);
    ~CompactionManager() = default;

    CompactionManager(const CompactionManager&) = delete;
    CompactionManager& operator=(const CompactionManager&) = delete;

    /**
     * @brief Registers a newly flushed SSTable file into the Level 0 catalogue.
     * Call this immediately after SSTableWriter::Write() completes.
     */
    common::Status AddFile(SSTableMeta meta);

    /**
     * @brief Removes an SSTable file from the catalogue (called after compaction).
     */
    void RemoveFile(const std::string& filepath);

    /**
     * @brief Returns all files at a given level, sorted by smallest_key.
     */
    std::vector<SSTableMeta> GetFilesAtLevel(int level) const;

    /**
     * @brief Returns the total byte size across all files at a level.
     */
    uint64_t TotalBytesAtLevel(int level) const;

    /**
     * @brief Returns the number of files at Level 0.
     */
    size_t Level0FileCount() const;

    /**
     * @brief Checks compaction triggers and returns true if compaction is needed.
     * Does NOT run compaction — just evaluates triggers.
     */
    bool NeedsCompaction() const;

    /**
     * @brief Synchronously runs one compaction job if needed.
     *
     * Picks the level that most urgently needs compaction, selects files,
     * performs the N-way merge-sort, writes new SSTable(s), removes old files.
     *
     * Returns CompactionStats with detailed metrics, or a Status error.
     *
     * NOTE: In production, call this from a background ThreadPool thread.
     * The function is designed to be safe to call concurrently — only one
     * compaction runs at a time (compaction_mutex_ ensures this).
     */
    common::ErrorOr<CompactionStats> RunCompaction();

    /**
     * @brief Returns a snapshot of the full file catalogue (all levels).
     */
    std::vector<SSTableMeta> AllFiles() const;

private:
    /// Pick which level needs compaction most urgently (-1 = none)
    int PickCompactionLevel() const;

    /// Select the files to compact from source_level and all overlapping files
    /// in target_level
    std::pair<std::vector<SSTableMeta>, std::vector<SSTableMeta>>
    SelectCompactionFiles(int source_level) const;

    /// Core N-way merge: read all input files, merge-sort, write output files
    common::ErrorOr<std::vector<SSTableMeta>> MergeFiles(
        const std::vector<SSTableMeta>& inputs,
        int target_level,
        CompactionStats& stats);

    /// Generate a unique output file path for a newly compacted SSTable
    std::string GenerateOutputPath(int level) const;

    std::string data_dir_;
    int max_level_;

    // File catalogue — protected by catalogue_mutex_
    mutable std::mutex catalogue_mutex_;
    std::vector<SSTableMeta> files_;

    // Ensures only one compaction runs at a time
    std::mutex compaction_mutex_;

    // Monotonically increasing counter for generating unique file names
    // mutable so it can be incremented from logically-const methods like GenerateOutputPath
    mutable std::atomic<uint64_t> next_file_number_{1};
};

} // namespace storage
} // namespace vectordb
