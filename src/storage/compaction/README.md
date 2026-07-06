# Compaction (Component 4)

## 🧹 The Janitor of the Storage Engine

As the database runs, the MemTable is repeatedly flushed to disk, creating many immutable SSTable files. Over time, this leads to three problems:
1. **Read Amplification**: A single point lookup might have to check many SSTable files to find a key.
2. **Space Amplification**: Overwritten keys and deleted keys (tombstones) take up disk space, even though they represent stale data.
3. **Write Amplification**: Overlapping key ranges make range scans inefficient.

The **CompactionManager** solves these problems by running a background process that merges multiple SSTables into fewer, larger ones, discarding duplicates and tombstones in the process.

## 📊 Leveled Compaction Strategy

We use a LevelDB/RocksDB-style leveled compaction strategy. Files are organized into levels (0 to 6):

```text
Level 0:  [SST_a] [SST_b] [SST_c]     ← Overlapping allowed (direct MemTable flushes)
Level 1:  [SST_1] [SST_2] [SST_3]     ← Non-overlapping, sorted
Level 2:  [SST_A] [SST_B] ... [SST_H] ← Non-overlapping, 10x larger than Level 1
Level 3:  [SST_...] ... [SST_...]     ← Non-overlapping, 10x larger than Level 2
```

### Compaction Triggers
1. **Level 0**: Triggered when the number of Level 0 files reaches `kLevel0CompactionTrigger` (default 4).
2. **Level 1+**: Triggered when the total byte size of the level exceeds `MaxBytesForLevel(L)`.
   - Level 1 target: 10 MB
   - Level 2 target: 100 MB
   - Level 3 target: 1 GB, etc.

## ⚙️ Merge Process

1. **Selection**: The `CompactionManager` selects a file from level $L$ (or all files if $L=0$) and all overlapping files from level $L+1$.
2. **N-Way Merge**: Using a min-heap, it performs an N-way merge sort across all selected files.
3. **Deduplication**: 
   - If the same key exists in multiple files, the version with the highest `sequence_number` (i.e., the newest) wins. Older versions are dropped.
   - If a key is a tombstone and the merge targets the bottom-most level (where no older versions can possibly exist), the tombstone is permanently dropped.
4. **Writing**: The merged data is written into new SSTable files (split at 64MB boundaries).
5. **Atomic Update**: The file catalogue is atomically updated: new files are added, and old files are deleted from the catalogue and disk.

## 📦 Sub-Components

| File | Purpose |
|---|---|
| `leveled_compaction.h` | Defines constants, triggers, size targets, and the `SSTableMeta` descriptor. |
| `compaction_manager.h/.cpp` | Orchestrates the background merges, maintains the file catalogue, and performs N-way merge deduplication. |

## 🔗 Integration with Phase 1 & 2

- Uses `SSTableReader` and `SSTableWriter` to read and write blocks.
- Output files are safely integrated into the system using `ErrorOr` and `Status` structures.
- Compaction runs safely alongside concurrent read queries because the file catalogue is protected by a mutex, and SSTables are immutable.

## 🧪 Testing

The compaction tests (`test_compaction.cpp`) verify:
1. **Trigger Detection**: Compaction correctly triggers when Level 0 hits 4 files.
2. **Key Deduplication**: When multiple SSTables contain the same key, the newest value overwrites the older values.
3. **Tombstone Propagation**: Tombstones are preserved in upper levels, but correctly dropped when they reach the bottom level.
4. **Stats Generation**: `CompactionStats` correctly reports bytes read/written, keys merged, and duration.
