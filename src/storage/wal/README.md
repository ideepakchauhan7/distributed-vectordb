# Write-Ahead Log (WAL) Subsystem

## 📐 System Design Philosophy

The Write-Ahead Log (WAL) is the absolute foundational layer of durability in any storage engine (LevelDB, RocksDB, Postgres). 

Because writing to memory is fast but volatile (data is lost on crash), and writing structured data to disk (like an SSTable) requires sorting and complex I/O, we need a middle ground. The WAL solves this by strictly appending every mutation to a sequential file **before** applying it to the in-memory MemTable.

If the system crashes (power loss, segfault), the MemTable is lost. Upon restart, the database replays the WAL from disk to flawlessly reconstruct the MemTable state.

> **Golden Rule of Databases:** A write is NEVER acknowledged to a client as "successful" until its WAL entry has been `fsync()`'d to persistent storage.

## ⚙️ How It Works

### 1. On-Disk Binary Layout

The WAL is designed for pure, unadulterated sequential write speed. We completely bypass Protobuf overhead here.

Every entry written to disk has a fixed 8-byte header followed by a variable payload:
```
┌──────────┬──────────┬──────────┬───────────────────┐
│ Length   │ CRC32    │ Type     │ Payload           │
│ (4 bytes)│ (4 bytes)│ (1 byte) │ (variable)        │
└──────────┴──────────┴──────────┴───────────────────┘
```
- **Length**: Used by the reader to know exactly how many bytes to read for the payload.
- **CRC32**: Checksum of the payload. **Critical for crash safety.**
- **Type**: `0x01` for PUT (insert/update), `0x02` for DELETE.

### 2. Crash Recovery and Partial Writes

What happens if the server loses power exactly while the WAL is writing an entry? The entry on disk will be cut off (partial write).

When the database restarts, `WAL::ReadAll()` scans the file:
1. It reads the 8-byte header.
2. It reads `Length` bytes of the payload.
3. It computes the CRC32 of the read payload.
4. If the computed CRC32 does **not** match the expected CRC32 in the header, the WAL immediately stops reading. It assumes this was a partial write due to a crash, truncates the file at this exact byte, and considers recovery complete.

### 3. Log Sequence Numbers (LSN)

Every entry is assigned a `sequence_number`. This is a monotonically increasing integer.
- It uniquely identifies a mutation.
- It is used for idempotency (if we replay the WAL twice, we ignore sequence numbers we've already seen).
- It is used by the RAFT consensus engine to track exactly which records have been replicated to follower nodes.

### 4. Integration with Phase 1

The WAL heavily relies on the foundation built in Phase 1:
- `ErrorOr<T>`: If disk space runs out, or file permissions are wrong, `WAL::Append` returns `StatusCode::kStorageIoError`.
- `BinarySerializer`: The WAL delegates the encoding and decoding of sequence numbers, string lengths, and payload boundaries to the heavily-tested `common_serialization` module.
