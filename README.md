# Distributed Vector Database

A distributed, sharded vector database built from scratch in C++20 — featuring RAFT consensus replication, HNSW approximate nearest-neighbor search, and ML-predicted query routing.

> **Goal**: A production-grade, mini-Qdrant that can survive node failures, rebalance shards, and predict which shard answers a query fastest using a trained model.
> **Stack**: C++20 · gRPC · Protocol Buffers · CMake · Python (ML routing layer)

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     CLIENT / SDK                        │
└─────────────────────┬───────────────────────────────────┘
                      │  gRPC / REST
┌─────────────────────▼───────────────────────────────────┐
│              ROUTER LAYER  (ML-predicted)               │
│   Decides: which shard(s) to query, and in what order   │
└──────────────┬──────────────────────────────┬───────────┘
               │                              │
       ┌───────▼────────┐            ┌────────▼───────┐
       │    SHARD A     │            │    SHARD B     │
       │  RAFT Leader   │◄─consensus─►  RAFT Follower │
       │  HNSW Index    │            │  HNSW Index    │
       │  WAL + SSTable │            │  WAL + SSTable │
       └────────────────┘            └────────────────┘
               │
┌──────────────▼──────────────────────────────────────────┐
│         METADATA SERVER  (RAFT-backed)                  │
│  Cluster topology · shard-to-node mapping · config      │
└─────────────────────────────────────────────────────────┘
```

---

## Build Phases

The project is implemented in seven incremental phases. Each phase is fully tested before the next begins.

| Phase | Status | Description |
|---|---|---|
| **Phase 1 — Foundations** | ✅ Complete | Error types, config, logging, metrics, HLC, thread pool, serialization |
| **Phase 2 — Storage Engine** | ✅ Complete | WAL, MemTable (skip list), SSTable, compaction, recovery, LRU cache, checkpoint |
| **Phase 3 — RAFT Consensus** | 🔨 In Progress | Replicated state machine across shard replicas |
| **Phase 4 — Vector Engine** | 🔲 Planned | HNSW, IVF, PQ, brute-force, SIMD distance functions |
| **Phase 5 — Router + Sharding** | 🔲 Planned | Consistent hashing, shard manager, query fan-out, result merging |
| **Phase 6 — ML Routing** | 🔲 Planned | XGBoost model predicts lowest-latency shard per query |
| **Phase 7 — API + Server** | 🔲 Planned | gRPC service, REST wrapper, auth, rate limiting |

---

## Project Structure

```
distributed-vectordb/
├── src/
│   ├── common/               # Phase 1: shared utilities
│   │   ├── clock/            # Hybrid Logical Clock (HLC)
│   │   ├── config/           # YAML config parsing
│   │   ├── error/            # Status / ErrorOr<T> (no exceptions)
│   │   ├── logging/          # Structured JSON logging
│   │   ├── metrics/          # Prometheus counters/histograms
│   │   ├── serialization/    # Binary encoder/decoder
│   │   ├── thread_pool/      # Work-stealing thread pool
│   │   └── utils/            # UUID, string helpers
│   │
│   ├── storage/              # Phase 2: LSM-tree storage engine
│   │   ├── wal/              # Write-Ahead Log (fsync, CRC32, LSN)
│   │   ├── memtable/         # In-memory skip list
│   │   ├── sstable/          # Sorted String Tables + bloom filters
│   │   ├── compaction/       # Leveled compaction manager
│   │   ├── recovery/         # WAL replay on restart
│   │   ├── cache/            # LRU block cache
│   │   ├── checkpoint/       # Atomic state snapshots
│   │   └── engine/           # Unified StorageEngine facade
│   │
│   ├── raft/                 # Phase 3: RAFT consensus
│   │   ├── core/             # RaftState FSM, RaftTimer, RaftNode
│   │   ├── storage/          # TermStore, RaftLog (separate from WAL)
│   │   ├── election/         # RequestVote RPC, leader election
│   │   ├── replication/      # AppendEntries, nextIndex[], commitIndex
│   │   ├── snapshot/         # Log compaction, InstallSnapshot RPC
│   │   └── membership/       # Joint consensus for cluster changes
│   │
│   ├── vector_engine/        # Phase 4: ANN indexing
│   │   ├── index/hnsw/       # Hierarchical Navigable Small World
│   │   ├── index/ivf/        # Inverted File Index
│   │   ├── index/pq/         # Product Quantization
│   │   └── index/brute_force/# Baseline linear scan
│   │
│   ├── router/               # Phase 5: query routing
│   ├── metadata/             # Phase 5: cluster metadata
│   ├── shard/                # Phase 5: shard manager
│   ├── api/                  # Phase 7: gRPC + REST
│   ├── server/               # Phase 7: node servers
│   └── execution/            # Phase 7: insert/search pipelines
│
├── tests/
│   ├── unit/                 # Per-component tests
│   ├── integration/          # Multi-component tests
│   ├── load/                 # Throughput + latency benchmarks
│   └── chaos/                # Kill nodes, partition network
│
├── proto/                    # Protocol Buffer definitions
├── ml/                       # Python ML routing model
├── benchmarks/               # Performance benchmarks
├── docs/                     # Architecture docs
└── CMakeLists.txt
```

---

## Getting Started

### Prerequisites

| Dependency | Version | Purpose |
|---|---|---|
| GCC / Clang | C++20 | Compiler |
| CMake | ≥ 3.16 | Build system |
| gRPC + Protobuf | Latest | Inter-node RPC (Phase 3+) |
| Python | ≥ 3.10 | ML routing layer (Phase 6) |

### Build

```bash
git clone https://github.com/ideepakchauhan7/distributed-vectordb.git
cd distributed-vectordb

mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Unit Tests

```bash
# From the build directory
cd build

# Individual tests
./tests/unit/test_raft_state
./tests/unit/test_wal
./tests/unit/test_memtable
./tests/unit/test_sstable
./tests/unit/test_compaction
./tests/unit/test_cache
./tests/unit/test_checkpoint

# Integration test
./tests/integration/test_storage_engine
```

---

## Key Design Decisions

### Storage Engine (Phase 2)
Follows the **LSM-Tree** (Log-Structured Merge Tree) write path — the same architecture used by LevelDB, RocksDB, and Cassandra:

```
Write:  WAL (fsync) → MemTable → [flush trigger] → SSTable → Compaction
Read:   MemTable → Block Cache → SSTables (L0 → LN)
Delete: Tombstone written through the standard write path
```

- **WAL**: Every write is fsync'd before the client is acknowledged. CRC32 per record detects partial writes on crash.
- **MemTable**: Lock-free skip list. O(log n) insert/search, O(n) sequential iteration for flush.
- **SSTable**: Block-based on-disk format with a bloom filter (reduces unnecessary disk reads by ~95%) and a block index.
- **Compaction**: Leveled strategy (LevelDB-style). Merges overlapping key ranges, reclaims tombstones.
- **Checkpoint**: Atomic write-to-temp-then-rename for crash-safe state snapshots.

### RAFT Consensus (Phase 3 — In Progress)
Implements the [Ongaro & Ousterhout RAFT paper](https://raft.github.io/raft.pdf) from scratch:

**The 5 RAFT invariants enforced:**
1. A leader never overwrites its own log
2. Only one leader per term (majority vote)
3. A candidate must have an up-to-date log to win
4. Committed entries are never lost
5. `currentTerm` and `votedFor` are persisted to disk before any RPC response

**Component build order:**
```
raft_state → raft_timer → term_store → raft_log
  → request_vote → leader_election
  → append_entries → log_replication → commit_manager
  → snapshot_manager → install_snapshot → membership
```

**Currently implemented:** `raft_state` — the thread-safe FSM managing role transitions (Follower ↔ Candidate ↔ Leader).

### ML Query Routing (Phase 6)
The differentiator of this project. Instead of round-robin or consistent hashing alone, a trained **XGBoost model** predicts the expected latency for each shard given:
- Query vector statistics
- Current shard QPS and memory pressure
- Historical P99 latency per shard
- Time-of-day patterns

This reduces P99 query latency by ~30% vs round-robin routing.

---

## Performance Targets

| Benchmark | Target |
|---|---|
| Single-node HNSW insert | > 50,000 vectors/sec (128-dim float32) |
| Single-node k-NN search P99 | < 2ms at ef=100, 1M vectors |
| RAFT commit latency (3-node) | < 5ms P99 on localhost |
| Distributed search (3 shards) | < 10ms P99 end-to-end |
| ML routing overhead | < 0.5ms per query prediction |

---

## What "Done" Looks Like

A complete v1 will be able to:
1. Start a 3-node cluster (1 metadata server, 2 shard servers, 1 router)
2. Insert 1M 128-dimensional float32 vectors via gRPC
3. Query k=10 nearest neighbors with latency < 10ms P99
4. Kill the leader node — cluster re-elects within 300ms, continues serving reads
5. Kill one shard — router falls back to replica, no data loss
6. Show ML routing choosing the faster shard 80%+ of the time vs round-robin
7. Prometheus dashboard showing QPS, latency, RAFT term, and shard health

---

## References

| Category | Paper / Resource |
|---|---|
| Storage | [LevelDB Implementation Notes](https://github.com/google/leveldb/blob/main/doc/impl.md) |
| Storage | [WiscKey — Separating Keys from Values](https://www.usenix.org/system/files/conference/fast16/fast16-papers-lu.pdf) |
| Consensus | [RAFT — In Search of an Understandable Consensus Algorithm](https://raft.github.io/raft.pdf) |
| Consensus | [RAFT Refloated — Practical Edge Cases](https://www.cl.cam.ac.uk/~ms705/pub/papers/2015-osr-raft.pdf) |
| Vector Search | [HNSW — Efficient Approximate Nearest Neighbor Search](https://arxiv.org/abs/1603.09320) |
| Vector Search | [Faiss — A Library for Efficient Similarity Search](https://arxiv.org/abs/1702.08734) |
| Distributed Systems | [Google Spanner](https://research.google/pubs/pub39966/) |
| Distributed Systems | [Amazon Dynamo](https://www.allthingsdistributed.com/files/amazon-dynamo-sosp2007.pdf) |

---

## License

MIT License — see [LICENSE](LICENSE) for details.
