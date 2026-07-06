# Checkpoint Manager (Component 7)

## 📸 The Photograph

Without checkpoints, a crash would force the Recovery Manager to replay the *entire* Write-Ahead Log from the beginning of time. If your database has been running for months, this replay could take hours.

The **Checkpoint Manager** solves this by periodically taking a "photograph" of the exact state of the database.

## 🏗️ Architecture

A checkpoint records:
1. `last_flushed_lsn`: All WAL entries with a Log Sequence Number (LSN) less than or equal to this value have been safely permanently flushed to SSTable files on disk. 
2. `sstables`: A complete catalogue of every active SSTable file (Level 0 through Level N), including their key ranges and exact byte sizes.

When the database restarts after a crash:
1. It loads this checkpoint.
2. It passes the catalogue to the `CompactionManager`.
3. It hands the `last_flushed_lsn` to the `RecoveryManager`, which ignores all WAL files/entries prior to that point, and only replays the handful of writes that were lost in RAM during the crash.

## 🔒 Atomic Safety

If the system loses power *while* writing the checkpoint file itself, we could end up with a corrupt checkpoint!
To prevent this, the `CheckpointManager` uses an atomic POSIX rename strategy:
1. Write the fully serialized checkpoint to a temporary file (`MANIFEST.checkpoint.tmp`).
2. Run `fsync` to guarantee it is on disk.
3. Use a standard OS `rename()` to atomically overwrite the active `MANIFEST.checkpoint`.

If a crash happens during Step 1 or 2, the old checkpoint is perfectly safe. Step 3 is a single atomic CPU instruction at the filesystem level.

## 🧪 Testing
The `test_checkpoint.cpp` ensures that saving and loading checkpoint data (including the `last_flushed_lsn` and multi-level SSTable meta-data) is fully lossless and accurate.
