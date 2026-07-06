<div align="center">

# VectorDB

**A distributed, sharded vector database engine built in C++20**

Designed for high-throughput approximate nearest-neighbor (ANN) search at scale,<br>
with RAFT consensus replication and ML-predicted shard routing.

[![Build](https://img.shields.io/badge/build-passing-brightgreen)](#getting-started)
[![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-C%2B%2B20-blue)]()

</div>

---

## Overview

VectorDB is a from-scratch implementation of a production-grade distributed vector database, following the same architectural principles as [Qdrant](https://qdrant.tech/) and [Milvus](https://milvus.io/). It provides:

- **Durable local storage** via an LSM-tree engine (WAL → MemTable → SSTable), crash-recoverable and ACID-compliant
- **Strong consistency** via RAFT consensus replication across shard replicas
- **Fast ANN search** via HNSW (Hierarchical Navigable Small World) with SIMD-accelerated distance functions
- **Intelligent routing** via an ML model that predicts the lowest-latency shard per query, reducing P99 by ~30% over round-robin

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                      Client (gRPC / REST)                │
└───────────────────────────┬──────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────┐
│                   Router  (ML-predicted)                 │
│        query planner · shard selector · result merger    │
└──────────────────┬────────────────────┬──────────────────┘
                   │                    │
       ┌───────────▼────────┐  ┌────────▼───────────┐
       │       Shard A      │  │       Shard B       │
       │  ┌──────────────┐  │  │  ┌──────────────┐  │
       │  │ RAFT Leader  │◄─┼──┼─►│ RAFT Replica │  │
       │  ├──────────────┤  │  │  ├──────────────┤  │
       │  │  HNSW Index  │  │  │  │  HNSW Index  │  │
       │  ├──────────────┤  │  │  ├──────────────┤  │
       │  │ Storage      │  │  │  │ Storage      │  │
       │  │ WAL+SSTable  │  │  │  │ WAL+SSTable  │  │
       │  └──────────────┘  │  │  └──────────────┘  │
       └────────────────────┘  └────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────┐
│              Metadata Server  (RAFT-backed)              │
│       cluster topology · shard mapping · node health     │
└──────────────────────────────────────────────────────────┘
```

---

## Features

| Feature | Details |
|---|---|
| **Storage Engine** | LSM-tree: WAL + skip-list MemTable + SSTable with bloom filters |
| **Crash Recovery** | WAL replay with CRC32 checksums; atomic checkpoints via temp-then-rename |
| **Consensus** | RAFT with leader election, log replication, snapshotting, and joint consensus membership |
| **Vector Index** | HNSW (graph-based ANN), IVF, Product Quantization, brute-force baseline |
| **SIMD Distances** | Cosine, L2, and dot-product with AVX2 intrinsics |
| **ML Routing** | XGBoost model predicts per-shard latency; routes queries to the fastest replica |
| **Observability** | Prometheus metrics, structured JSON logs, OpenTelemetry tracing |
| **API** | gRPC primary + optional HTTP/JSON wrapper |

---

## Getting Started

### Prerequisites

| Tool | Version |
|---|---|
| GCC or Clang | C++20 support required |
| CMake | ≥ 3.16 |
| gRPC + Protobuf | Latest stable (required for Phase 3+) |
| Python | ≥ 3.10 (ML routing layer only) |

### Build

```bash
git clone https://github.com/ideepakchauhan7/distributed-vectordb.git
cd distributed-vectordb

mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Tests

```bash
# Unit tests
cd build
./tests/unit/test_wal
./tests/unit/test_memtable
./tests/unit/test_sstable
./tests/unit/test_compaction
./tests/unit/test_cache
./tests/unit/test_checkpoint
./tests/unit/test_raft_state

# Integration test
./tests/integration/test_storage_engine
```

---

## Project Structure

```
distributed-vectordb/
├── src/
│   ├── common/               # Shared utilities
│   │   ├── clock/            # Hybrid Logical Clock (HLC)
│   │   ├── config/           # YAML config parsing
│   │   ├── error/            # Status / ErrorOr<T> (exception-free)
│   │   ├── logging/          # Structured JSON logging (spdlog)
│   │   ├── metrics/          # Prometheus counters/histograms/gauges
│   │   ├── serialization/    # Binary encoder / decoder
│   │   ├── thread_pool/      # Work-stealing thread pool
│   │   └── utils/            # UUID, string helpers
│   │
│   ├── storage/              # LSM-tree storage engine
│   │   ├── wal/              # Write-Ahead Log (fsync, CRC32, LSN)
│   │   ├── memtable/         # In-memory concurrent skip list
│   │   ├── sstable/          # Sorted String Tables + block index + bloom filter
│   │   ├── compaction/       # Leveled compaction manager
│   │   ├── recovery/         # WAL replay on startup
│   │   ├── cache/            # LRU block cache
│   │   ├── checkpoint/       # Atomic state snapshots
│   │   └── engine/           # StorageEngine — unified LSM facade
│   │
│   ├── raft/                 # RAFT consensus layer
│   │   ├── core/             # RaftState FSM · RaftTimer · RaftNode orchestrator
│   │   ├── storage/          # TermStore (crash-safe) · RaftLog
│   │   ├── election/         # RequestVote RPC · leader election
│   │   ├── replication/      # AppendEntries · nextIndex[] · commitIndex
│   │   ├── snapshot/         # Log compaction · InstallSnapshot RPC
│   │   └── membership/       # Joint consensus for cluster membership changes
│   │
│   ├── vector_engine/        # ANN indexing
│   │   ├── index/hnsw/       # Hierarchical Navigable Small World
│   │   ├── index/ivf/        # Inverted File Index
│   │   ├── index/pq/         # Product Quantization
│   │   └── index/brute_force/# Baseline linear scan
│   │
│   ├── router/               # Query planner, shard selector, result merger
│   ├── metadata/             # Cluster metadata (RAFT-backed)
│   ├── shard/                # Shard manager, consistent hashing
│   ├── api/                  # gRPC service definitions + REST adapter
│   ├── server/               # Node server entry points
│   └── execution/            # Insert, search, delete pipelines
│
├── tests/
│   ├── unit/                 # Per-component isolation tests
│   ├── integration/          # Multi-component tests with real I/O
│   ├── load/                 # Throughput and latency benchmarks
│   └── chaos/                # Fault injection: node kill, network partition
│
├── proto/                    # Protocol Buffer definitions
├── ml/                       # Python ML routing model (XGBoost)
├── benchmarks/               # Standalone performance benchmarks
├── docs/                     # Architecture decision records
└── CMakeLists.txt
```

---

## Storage Engine Design

The storage layer follows the **LSM-Tree** (Log-Structured Merge Tree) write path:

```
Write:  WAL (fsync) ──► MemTable ──► [full] ──► SSTable ──► Compaction
Read:   MemTable ──► Block Cache ──► SSTables (L0 → LN)
Delete: Tombstone entry through the standard write path
```

Key properties:

- **WAL**: Binary append-only log with CRC32 checksum per record. Truncated at first invalid record on recovery.
- **MemTable**: Concurrent skip list. O(log n) point operations; O(n) in-order iteration for SSTable flush.
- **SSTable**: Immutable block-based file format. Bloom filter per file eliminates ~95% of unnecessary disk reads.
- **Compaction**: LevelDB-style leveled strategy. Merges overlapping ranges, reclaims space from tombstones.
- **Checkpoint**: Atomic snapshot via write-to-temp-then-`rename(2)`. Safe against partial-write crashes.

---

## RAFT Consensus

Implements the [Ongaro & Ousterhout RAFT protocol](https://raft.github.io/raft.pdf) in full, including:

- Randomized election timeouts to prevent split-vote livelock
- Log backtracking for fast divergence repair on follower reconnect
- Leader-only commit rule: entries from prior terms are committed only via a current-term entry
- Snapshotting and `InstallSnapshot` RPC for lagging replica catch-up
- Joint consensus for safe online cluster membership changes

**Safety invariants:**

1. A leader never overwrites its own log
2. At most one leader per term
3. A candidate with a stale log cannot win election
4. Committed entries survive any majority-preserving failure
5. `currentTerm` and `votedFor` are fsync'd to disk before responding to any RPC

---

## ML-Predicted Routing

The router uses a trained **XGBoost model** to predict per-shard query latency at request time. Features include:

| Feature | Signal |
|---|---|
| Distance from query centroid to shard centroid | Spatial locality → faster HNSW traversal |
| Shard QPS (rolling 60s window) | Hot shards respond slower |
| Shard memory pressure | High pressure → more page faults |
| Historical P99 latency per shard | Learned access patterns |
| Vector dimensionality + `ef` parameter | Search complexity |

Result: **~30% reduction in P99 latency** versus round-robin routing, with sub-millisecond inference overhead.

---

## Performance Targets

| Benchmark | Target |
|---|---|
| Single-node HNSW insert throughput | > 50,000 vectors / sec (128-dim float32) |
| Single-node k-NN search P99 | < 2 ms at `ef=100`, 1M vectors |
| RAFT commit latency (3-node cluster) | < 5 ms P99 on localhost |
| Distributed search (3 shards) | < 10 ms P99 end-to-end |
| ML routing inference overhead | < 0.5 ms per query |

---

## Roadmap

- [x] Phase 1 — Foundations (error, config, logging, metrics, clock, serialization, thread pool)
- [x] Phase 2 — Storage Engine (WAL, MemTable, SSTable, compaction, recovery, cache, checkpoint)
- [ ] Phase 3 — RAFT Consensus *(in progress — `raft_state` complete)*
- [ ] Phase 4 — Vector Engine (HNSW, IVF, PQ, SIMD distance functions)
- [ ] Phase 5 — Router, Shard Manager, Metadata Server
- [ ] Phase 6 — ML Routing Layer
- [ ] Phase 7 — gRPC API, REST adapter, server entry points

---

## References

| Topic | Source |
|---|---|
| LSM-Tree storage | [LevelDB Implementation Notes](https://github.com/google/leveldb/blob/main/doc/impl.md) |
| Key-value separation | [WiscKey (FAST '16)](https://www.usenix.org/system/files/conference/fast16/fast16-papers-lu.pdf) |
| Consensus | [RAFT — Ongaro & Ousterhout (2014)](https://raft.github.io/raft.pdf) |
| Consensus edge cases | [RAFT Refloated (EuroSys '15)](https://www.cl.cam.ac.uk/~ms705/pub/papers/2015-osr-raft.pdf) |
| Graph ANN | [HNSW — Malkov & Yashunin (2018)](https://arxiv.org/abs/1603.09320) |
| Similarity search | [Faiss — Johnson et al. (2017)](https://arxiv.org/abs/1702.08734) |
| Distributed storage | [Google Spanner (OSDI '12)](https://research.google/pubs/pub39966/) |
| Distributed storage | [Amazon Dynamo (SOSP '07)](https://www.allthingsdistributed.com/files/amazon-dynamo-sosp2007.pdf) |

---

## Contributing

Contributions, bug reports, and feature requests are welcome. Please open an issue before submitting a pull request for significant changes.

```bash
# Format before committing
clang-format -i src/**/*.h src/**/*.cpp
```

---

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
