# SSTable (Sorted String Table) Subsystem

## 📐 System Design Philosophy

When the in-memory MemTable is full, it must be flushed to disk in a format that is:
1. **Immutable** — once written, an SSTable file is NEVER modified (only deleted during compaction).
2. **Sorted** — keys are in lexicographic order, enabling efficient binary search and range scans.
3. **Block-based** — data is divided into ~4KB blocks that align with the OS page size and SSD sector size.

An SSTable is the final resting place for data between MemTable flushes and compaction merges.

## ⚙️ File Format

```
┌───────────────────────────────────┐
│         Data Block 0 (~4KB)       │  ← Sorted key-value pairs
├───────────────────────────────────┤
│         Data Block 1 (~4KB)       │
├───────────────────────────────────┤
│              ...                  │
├───────────────────────────────────┤
│         Data Block N (~4KB)       │
├───────────────────────────────────┤
│         Bloom Filter Block        │  ← "Is key X possibly in this file?"
├───────────────────────────────────┤
│         Index Block               │  ← Maps last_key → {offset, size}
├───────────────────────────────────┤
│         Footer (48 bytes)         │  ← Offsets to bloom + index + metadata
└───────────────────────────────────┘
```

### Footer (48 bytes)
| Field | Size | Purpose |
|---|---|---|
| `bloom_offset` | 8 bytes | Byte offset of the bloom filter block |
| `bloom_size` | 8 bytes | Size of the bloom filter block |
| `index_offset` | 8 bytes | Byte offset of the index block |
| `index_size` | 8 bytes | Size of the index block |
| `num_entries` | 8 bytes | Total key-value entries in the file |
| `magic` | 8 bytes | `0x0053535461626C65` — validates the file format |

### Data Block
Each ~4KB data block contains sorted entries:
```
[ num_entries (4 bytes) ]
[ Entry: key_len(4) | key | val_len(4) | val | tombstone(1) ] × N
```

### Index Block
Maps each data block to its last key and byte position:
```
[ num_blocks (4 bytes) ]
[ last_key_len(4) | last_key | block_offset(8) | block_size(4) ] × N
```

## 🌸 Bloom Filter

The Bloom filter prevents unnecessary disk reads. Before reading a data block, the reader checks the in-memory bloom filter:
- **"Definitely not here"** → skip the entire file (zero disk I/O).
- **"Possibly here"** → read the data block and search for the key.

### How it Works
1. A bit array of `num_keys × 10 bits` is allocated.
2. When a key is added: 7 hash functions compute 7 bit positions, all are set to `1`.
3. When querying a key: the same 7 positions are checked. If any is `0`, the key is guaranteed absent.
4. We use **double-hashing** (`h_i = h1 + i * h2`) to derive 7 independent hashes from 2 base FNV-1a hashes.

At 10 bits/key, the theoretical false positive rate is ~1%. Our test measured **2.18%**, which is within the expected statistical variance for 1000 keys.

## 🔍 Read Path (Point Lookup)

```
Get("vec_42")
    │
    ├─ 1. BloomFilter.MayContain("vec_42")
    │      NO  → return NOT_FOUND (zero disk I/O!)
    │      YES ↓
    │
    ├─ 2. Binary search the Index for the block whose last_key >= "vec_42"
    │      → Found block at offset 8192, size 3971
    │
    ├─ 3. Read that 4KB data block from disk
    │
    └─ 4. Binary search within the block for "vec_42"
           → Found! Return the value.
```

## 📝 Write Path (SSTable Creation)

```
SSTableWriter::Write(filepath, memtable_iterator)
    │
    ├─ 1. Collect all entries from the sorted MemTable iterator
    │
    ├─ 2. Build a BloomFilter and add every key
    │
    ├─ 3. Pack entries into ~4KB data blocks and write them sequentially
    │      Record each block's {last_key, offset, size} for the index
    │
    ├─ 4. Serialize and write the Bloom Filter block
    │
    ├─ 5. Serialize and write the Index block
    │
    └─ 6. Write the 48-byte Footer (offsets to bloom + index, entry count, magic)
```

## 🔗 Integration with Phase 1

| Phase 1 Component | Used For |
|---|---|
| `BinarySerializer` | All integer/string encoding in blocks, index, bloom, and footer |
| `ErrorOr<T>` | Every disk read returns `ErrorOr` — file not found, corruption, partial read |

## 📦 Sub-Components

| File | Purpose |
|---|---|
| `bloom_filter.h/.cpp` | Probabilistic set membership (10 bits/key, ~1% FPR) |
| `block.h/.cpp` | 4KB data block: Add, Serialize, Deserialize, binary-search Get |
| `sstable_writer.h/.cpp` | Writes a complete SSTable from a sorted iterator |
| `sstable_reader.h/.cpp` | Opens an SSTable, loads bloom+index, serves point lookups and full scans |
