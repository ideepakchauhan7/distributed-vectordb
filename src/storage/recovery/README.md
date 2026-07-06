# Recovery (Component 5)

## 🔄 The Insurance Policy

The **Recovery Manager** acts as the database's insurance policy against crashes. Because the `MemTable` lives entirely in memory, any process crash (power failure, OOM, segfault, etc.) will completely destroy it. However, because every operation is durably logged to the Write-Ahead Log (WAL) before being applied to the MemTable, no data is actually lost.

The Recovery Manager is responsible for reconstructing the MemTable's state by replaying the WAL exactly as it happened before the crash.

## 🚀 Recovery Flow

1. **System Starts**: The system initiates boot sequence. MemTable is completely empty.
2. **Find Checkpoint**: The system determines the `last_flushed_lsn` (Log Sequence Number). This represents the highest LSN that was already safely flushed into immutable SSTables.
3. **Open WAL**: The Recovery Manager opens the existing WAL file from disk.
4. **Replay Loop**:
   - Reads every `WALEntry`.
   - If the entry is corrupted or partial (failed CRC32 check), reading stops safely (truncating the garbage data caused by the crash).
   - If `entry.sequence_number <= last_flushed_lsn`, it skips it (idempotency — this data is already in an SSTable).
   - If `entry.sequence_number > last_flushed_lsn`, it applies the `Put` or `Delete` directly to the MemTable.
5. **Ready**: The MemTable is now exactly as it was just before the crash. The system can start accepting new client requests.

## ⚙️ Key Design Principles

- **Idempotency**: The recovery process is idempotent. If the system crashes *during* recovery, restarting recovery is perfectly safe.
- **Strict Ordering**: Entries are applied in the exact order they were written (monotonically increasing LSN), preserving the latest version of any key.
- **Fail-Safe Truncation**: When a machine crashes halfway through writing a WAL entry, that entry is mangled. `WAL::ReadAll()` uses CRC32 checksums to detect this, automatically ignoring the corrupted entry and all subsequent bytes.

## 📦 Integration

- Integrates with `storage_wal` (reads entries).
- Integrates with `storage_memtable` (applies entries via `Put`/`Delete`).

## 🧪 Testing

The tests (`test_recovery.cpp`) verify:
1. **Full Recovery**: Correctly replays a mix of `Put` and `Delete` operations, correctly restoring the MemTable state.
2. **Checkpoint Skip**: Verifies that passing a `last_flushed_lsn` correctly skips older entries and only replays new ones.
3. **Graceful No-Op**: Booting with no WAL file (fresh database) returns `0` gracefully without crashing.
