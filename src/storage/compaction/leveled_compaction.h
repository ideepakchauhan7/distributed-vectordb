#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace vectordb {
namespace storage {

/**
 * @struct SSTableMeta
 * @brief Metadata describing a single SSTable file on disk.
 *
 * The compaction manager tracks all live SSTable files using this struct.
 * The actual data lives in the .sst file — this is just the catalogue entry.
 */
struct SSTableMeta {
    std::string filepath;       // Absolute path to the .sst file
    int level;                  // Which level this file belongs to (0, 1, 2, ...)
    std::string smallest_key;   // First key in the file (from the index)
    std::string largest_key;    // Last key in the file (from the index)
    uint64_t file_size_bytes;   // Approximate size for level size tracking
    uint64_t sequence_number;   // Creation order (higher = newer, wins on conflict)

    /// Returns true if this file's key range overlaps with [lo, hi]
    bool OverlapsWith(const std::string& lo, const std::string& hi) const {
        return !(largest_key < lo || smallest_key > hi);
    }
};

/**
 * @namespace leveled
 * @brief Constants and decision logic for LevelDB-style leveled compaction.
 *
 * Leveled Compaction Strategy:
 * ─────────────────────────────────────────────────────────────────────
 * Level 0:  Direct MemTable flushes — files CAN have overlapping key ranges
 * Level 1+: Fully sorted, non-overlapping — merged into from the level above
 *
 *  Level 0:  [SST_a] [SST_b] [SST_c]      ← Overlapping allowed
 *  Level 1:  [SST_1] [SST_2] [SST_3]      ← Non-overlapping
 *  Level 2:  [SST_A] ... [SST_H]          ← 10x bigger than Level 1
 *  Level 3:  [SST_...] ... [SST_...]      ← 10x bigger than Level 2
 *
 * Trigger conditions:
 *  - Level 0: triggers when file count exceeds kLevel0CompactionTrigger
 *  - Level 1+: triggers when total bytes exceeds MaxBytesForLevel(level)
 */
namespace leveled {

    static constexpr int kNumLevels = 7;

    /// Trigger a Level 0 compaction when this many files accumulate
    static constexpr int kLevel0CompactionTrigger = 4;

    /// Slow down writes when Level 0 reaches this many files
    static constexpr int kLevel0SlowdownTrigger = 8;

    /// Stop writes entirely when Level 0 reaches this many files
    static constexpr int kLevel0StopTrigger = 12;

    /// Level 1 target size in bytes (10 MB)
    static constexpr uint64_t kLevel1MaxBytes = 10ULL * 1024 * 1024;

    /// Each subsequent level is 10x larger
    static constexpr uint64_t kLevelMultiplier = 10;

    /// Returns the maximum byte budget for a given level
    inline uint64_t MaxBytesForLevel(int level) {
        if (level <= 0) return 0; // Level 0 is count-based, not size-based
        uint64_t result = kLevel1MaxBytes;
        for (int i = 1; i < level; ++i) {
            result *= kLevelMultiplier;
        }
        return result;
    }

} // namespace leveled

} // namespace storage
} // namespace vectordb
