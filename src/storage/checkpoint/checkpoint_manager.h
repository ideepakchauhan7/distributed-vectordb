#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include "src/common/error/error_or.h"
#include "src/storage/compaction/leveled_compaction.h"

namespace vectordb {
namespace storage {

/**
 * @struct Checkpoint
 * @brief Represents a point in time where the MemTable was flushed.
 */
struct Checkpoint {
    uint64_t last_flushed_lsn;           // All WAL entries <= this LSN are safely in SSTables
    std::vector<SSTableMeta> sstables;   // The exact catalogue of active SSTable files
};

/**
 * @class CheckpointManager
 * @brief Handles saving and loading the database state (Checkpoint) atomically.
 *
 * It uses a write-to-temp-then-rename approach to guarantee that the
 * checkpoint file is never partially written or corrupted during a crash.
 */
class CheckpointManager {
public:
    /**
     * @param data_dir The root directory where checkpoint files will be stored.
     */
    explicit CheckpointManager(std::string data_dir);

    /**
     * @brief Atomically saves the checkpoint to disk.
     */
    common::Status SaveCheckpoint(const Checkpoint& checkpoint);

    /**
     * @brief Loads the latest checkpoint from disk.
     * @return The Checkpoint, or an error if corrupted/missing. 
     *         If the file does not exist, returns a Checkpoint with LSN=0 and empty files.
     */
    common::ErrorOr<Checkpoint> LoadCheckpoint();

private:
    std::string data_dir_;
    std::string checkpoint_path_;
    std::string temp_path_;
    mutable std::mutex mutex_;
};

} // namespace storage
} // namespace vectordb
