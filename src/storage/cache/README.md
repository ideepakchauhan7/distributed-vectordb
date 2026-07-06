# Block Cache (Component 6)

## ⚡ The Front Desk

When performing searches or range scans, the database often needs to read the same 4KB blocks from SSTables repeatedly. If we hit the disk every single time, performance would be strictly limited by disk IOPS.

The **Block Cache** acts as an LRU (Least Recently Used) cache in memory, storing the most frequently and recently accessed 4KB blocks.

## 🏗️ Architecture

The cache is implemented using a classic LRU design combining a Hash Map and a Doubly-Linked List.

```text
┌───────────────────────────────────────────────┐
│  Hash Map: block_key → List Iterator          │  ← O(1) lookup
├───────────────────────────────────────────────┤
│  Doubly-Linked List (MRU ↔ LRU)              │  ← O(1) eviction / promotion
│                                               │
│  [Most Recent] ←→ [Older] ←→ [LRU]           │
└───────────────────────────────────────────────┘
```

- **Cache Key**: A unique string combining the SSTable filename and the block offset (e.g., `/data/level1_00000001.sst:4096`).
- **Cache Value**: The raw uncompressed 4KB block data (`std::vector<uint8_t>`).
- **Capacity**: Configured in Megabytes. When the cache exceeds this capacity, the items at the tail of the list (LRU) are evicted.

## 🔒 Thread Safety

The Block Cache uses a `std::shared_mutex` (Read-Write Lock).
- A cache "hit" initially acquires a shared read lock to check if the item exists.
- If it does, it briefly drops the read lock and acquires a unique write lock to promote the item to the MRU position.
- This balances high concurrency for checking the cache while safely allowing modifications to the internal list structure.

## 🧪 Testing

The unit tests (`test_cache.cpp`) verify:
1. **Basic Operations**: Items can be inserted and successfully retrieved. Cache misses behave correctly.
2. **Eviction Policy**: When inserting blocks that exceed the total capacity, the oldest un-accessed blocks are seamlessly evicted to make room.
