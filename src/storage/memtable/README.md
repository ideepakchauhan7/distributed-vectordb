# MemTable Subsystem

## 📐 System Design Philosophy

Disks (even NVMe SSDs) are slow compared to RAM. If every write to our database directly hit a data file, our write throughput would plummet. Instead, the storage engine buffers recent writes into memory. This in-memory buffer is the **MemTable**.

When the MemTable exceeds a configured memory threshold (e.g., 64MB), it is frozen, becomes immutable, and a background thread flushes its contents into an on-disk **SSTable**. Meanwhile, a new active MemTable takes its place.

## ⚙️ Backing Data Structure: SkipList

While many simple implementations might use `std::map` (a Red-Black Tree), LevelDB, RocksDB, and our distributed vector database use a **Skip List**. 

### Why a Skip List?
1. **Concurrency**: It is far easier to implement lock-free or highly concurrent read/write mechanisms on a skip list than on balanced trees, which require complex node rotations that lock large subtrees.
2. **Sequential Iteration**: Level 0 of a skip list is a simple linked list. This allows for extremely cache-friendly, O(N) sequential iteration, which is required when flushing to an SSTable.
3. **Simplicity**: Random height generation naturally balances the tree probabilistically without strict rotation rules.

```
Level 3:  HEAD ──────────────────────────────── 50 ──────── NIL
Level 2:  HEAD ────────── 20 ──────────────── 50 ──── 70 ── NIL
Level 1:  HEAD ──── 10 ── 20 ──── 35 ──────── 50 ──── 70 ── NIL
Level 0:  HEAD ── 5 ─ 10 ─ 20 ─ 25 ─ 35 ─ 40 ─ 50 ─ 60 ─ 70 ── NIL
```

## 🔄 The Life Cycle of a Mutation

1. **WAL First**: A mutation is durably logged to the `WAL`.
2. **MemTable Put**: The `MemTable::Put` is called.
3. **SkipList Traversal**: The SkipList probabilistically generates a height for the new key and links it into the corresponding levels.
4. **Memory Tracking**: The MemTable updates its internal `memory_usage_` counter (tracking the size of the key, value, and node pointers).

## 🪦 Tombstones (Deletions)

In Log-Structured Merge (LSM) architectures, we don't immediately free memory when a user deletes a key. If we did, an old version of the key resting in an SSTable on disk might suddenly "reappear." 

Instead, a deletion is an insert of a **Tombstone**.
If the user deletes `"vec_42"`, we set `is_tombstone = true` for that key in the SkipList. 
When a reader queries `"vec_42"`, they hit the tombstone and the engine correctly returns "Not Found." When the MemTable is eventually flushed to an SSTable, the tombstone is carried with it to shadow any older versions of the key on disk.

## 🧵 Thread Safety

Our implementation utilizes a `std::shared_mutex` for Read-Write locking:
- `Put()` and `Delete()` take a **Unique Lock** (`std::unique_lock`).
- `Get()` and `ApproximateMemoryUsage()` take a **Shared Lock** (`std::shared_lock`).

This enables single-writer, multiple-reader (SWMR) concurrency semantics. One thread handles WAL replay/inserts, while dozens of web threads can simultaneously query the MemTable.
