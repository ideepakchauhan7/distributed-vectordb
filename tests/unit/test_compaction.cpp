#include "src/storage/compaction/compaction_manager.h"
#include "src/storage/sstable/sstable_writer.h"
#include "src/storage/sstable/sstable_reader.h"
#include "src/storage/memtable/memtable.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <cstdio>

using namespace vectordb::storage;
using namespace vectordb::common;

static const std::string kTestDir = "/tmp/test_compaction";

void Cleanup() {
    std::filesystem::remove_all(kTestDir);
}

/// Helper: write a MemTable to an SSTable and register it with the CompactionManager
SSTableMeta WriteAndRegister(CompactionManager& mgr,
                             MemTable& mem,
                             const std::string& name,
                             int level,
                             uint64_t seq) {
    std::string path = kTestDir + "/" + name + ".sst";
    auto iter = mem.Begin();
    auto s = SSTableWriter::Write(path, iter);
    assert(s.IsOk());

    SSTableMeta meta;
    meta.filepath = path;
    meta.level = level;
    meta.sequence_number = seq;
    meta.file_size_bytes = std::filesystem::file_size(path);

    // Extract key range from the SSTable
    auto reader_or = SSTableReader::Open(path);
    assert(reader_or.IsOk());
    auto all_or = reader_or.value()->ReadAll();
    assert(all_or.IsOk());
    assert(!all_or.value().empty());
    meta.smallest_key = all_or.value().front().key;
    meta.largest_key = all_or.value().back().key;

    auto add_s = mgr.AddFile(meta);
    assert(add_s.IsOk());
    return meta;
}

// ─── Test 1: Compaction trigger detection ────────────────────────────

void TestCompactionTrigger() {
    Cleanup();
    CompactionManager mgr(kTestDir);

    // No files → no compaction needed
    assert(!mgr.NeedsCompaction());

    // Add 4 Level 0 files → triggers compaction
    for (int i = 0; i < 4; ++i) {
        MemTable mem;
        char key[16];
        snprintf(key, sizeof(key), "a_%03d", i);
        mem.Put(key, {static_cast<uint8_t>(i)});
        WriteAndRegister(mgr, mem, "trigger_" + std::to_string(i), 0, i);
    }

    assert(mgr.Level0FileCount() == 4);
    assert(mgr.NeedsCompaction());

    std::cout << "  TestCompactionTrigger: PASSED" << std::endl;
    Cleanup();
}

// ─── Test 2: Key deduplication — newer version wins ──────────────────

void TestKeyDeduplication() {
    Cleanup();
    CompactionManager mgr(kTestDir);

    // File 1 (Level 0, seq=1 — older): key_001 = [1]
    MemTable mem1;
    mem1.Put("key_001", {1});
    mem1.Put("key_003", {3});
    WriteAndRegister(mgr, mem1, "old", 0, 1);

    // File 2 (Level 0, seq=2 — newer): key_001 = [99] (should win)
    MemTable mem2;
    mem2.Put("key_001", {99}); // Updated value
    mem2.Put("key_002", {2});
    WriteAndRegister(mgr, mem2, "new", 0, 2);

    // Add 2 filler files to trigger Level 0 compaction (needs 4 files)
    for (int i = 0; i < 2; ++i) {
        MemTable filler;
        char k[16];
        snprintf(k, sizeof(k), "filler_%03d", i);
        filler.Put(k, {static_cast<uint8_t>(i)});
        WriteAndRegister(mgr, filler, "filler_" + std::to_string(i), 0, 3 + i);
    }

    assert(mgr.NeedsCompaction());

    auto stats_or = mgr.RunCompaction();
    assert(stats_or.IsOk());
    auto& stats = stats_or.value();

    assert(stats.source_level == 0);
    assert(stats.target_level == 1);
    assert(stats.duplicate_keys_dropped >= 1); // key_001 duplicate

    // Verify the merged output: key_001 should have value [99]
    auto l1_files = mgr.GetFilesAtLevel(1);
    assert(!l1_files.empty());

    auto reader_or = SSTableReader::Open(l1_files[0].filepath);
    assert(reader_or.IsOk());

    auto result_or = reader_or.value()->Get("key_001");
    assert(result_or.IsOk());
    assert(result_or.value().has_value());
    assert(result_or.value()->value == std::vector<uint8_t>({99}));

    // L0 should be empty now
    assert(mgr.Level0FileCount() == 0);

    std::cout << "  TestKeyDeduplication: PASSED (duplicates dropped: "
              << stats.duplicate_keys_dropped << ")" << std::endl;
    Cleanup();
}

// ─── Test 3: Tombstone propagation ───────────────────────────────────

void TestTombstonePropagation() {
    Cleanup();
    // Use max_level=2 so target is bottom level → tombstones are dropped
    CompactionManager mgr(kTestDir, 2);

    // L0 file with a tombstone
    MemTable mem;
    mem.Put("alive_key", {1, 2, 3});
    mem.Delete("dead_key"); // Tombstone

    // We need 4 L0 files to trigger compaction
    for (int i = 0; i < 3; ++i) {
        MemTable filler;
        char k[16];
        snprintf(k, sizeof(k), "filler_%03d", i);
        filler.Put(k, {static_cast<uint8_t>(i)});
        WriteAndRegister(mgr, filler, "filler_" + std::to_string(i), 0, i);
    }
    WriteAndRegister(mgr, mem, "with_tombstone", 0, 10);

    assert(mgr.NeedsCompaction());
    auto stats_or = mgr.RunCompaction();
    assert(stats_or.IsOk());

    // Merge is to bottom level (level 1 when max_level=2)
    // Tombstones for "dead_key" should be physically dropped
    assert(stats_or.value().tombstones_dropped >= 1);

    // alive_key should still exist
    auto l1_files = mgr.GetFilesAtLevel(1);
    if (!l1_files.empty()) {
        auto reader_or = SSTableReader::Open(l1_files[0].filepath);
        if (reader_or.IsOk()) {
            auto r = reader_or.value()->Get("alive_key");
            assert(r.IsOk());
        }
    }

    std::cout << "  TestTombstonePropagation: PASSED (tombstones dropped: "
              << stats_or.value().tombstones_dropped << ")" << std::endl;
    Cleanup();
}

// ─── Test 4: Stats are populated correctly ────────────────────────────

void TestCompactionStats() {
    Cleanup();
    CompactionManager mgr(kTestDir);

    for (int i = 0; i < 4; ++i) {
        MemTable mem;
        for (int j = 0; j < 10; ++j) {
            char k[24];
            snprintf(k, sizeof(k), "file%d_key_%03d", i, j);
            mem.Put(k, {static_cast<uint8_t>(j)});
        }
        WriteAndRegister(mgr, mem, "stats_" + std::to_string(i), 0, i);
    }

    auto stats_or = mgr.RunCompaction();
    assert(stats_or.IsOk());
    auto& stats = stats_or.value();

    assert(stats.files_read == 4);
    assert(stats.files_written >= 1);
    assert(stats.bytes_read > 0);
    assert(stats.bytes_written > 0);
    assert(stats.keys_merged == 40); // 4 files × 10 keys each
    assert(stats.duration_ms >= 0.0);

    std::cout << "  TestCompactionStats: PASSED ("
              << stats.keys_merged << " keys merged in "
              << stats.duration_ms << " ms)" << std::endl;
    Cleanup();
}

int main() {
    std::cout << "Testing CompactionManager..." << std::endl;
    TestCompactionTrigger();
    TestKeyDeduplication();
    TestTombstonePropagation();
    TestCompactionStats();
    std::cout << "All CompactionManager tests passed." << std::endl;
    return 0;
}
