#include "src/storage/sstable/bloom_filter.h"
#include "src/storage/sstable/block.h"
#include "src/storage/sstable/sstable_writer.h"
#include "src/storage/sstable/sstable_reader.h"
#include "src/storage/memtable/memtable.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace vectordb::storage;

static const std::string kTestSSTablePath = "/tmp/test_sstable.sst";

void Cleanup() {
    std::filesystem::remove(kTestSSTablePath);
}

// ─── Test 1: Bloom Filter ────────────────────────────────────────────

void TestBloomFilter() {
    BloomFilter bf(1000, 10);

    // Add 1000 keys
    for (int i = 0; i < 1000; ++i) {
        bf.Add("key_" + std::to_string(i));
    }

    // All added keys MUST return true (no false negatives)
    for (int i = 0; i < 1000; ++i) {
        assert(bf.MayContain("key_" + std::to_string(i)));
    }

    // Count false positives for 10000 keys that were NOT added
    int false_positives = 0;
    for (int i = 1000; i < 11000; ++i) {
        if (bf.MayContain("key_" + std::to_string(i))) {
            false_positives++;
        }
    }
    // At 10 bits/key, FPR should be ~1%. Allow up to 3% for statistical variance.
    double fpr = static_cast<double>(false_positives) / 10000.0;
    assert(fpr < 0.03);

    std::cout << "  TestBloomFilter: PASSED (FPR = " << (fpr * 100) << "%)" << std::endl;
}

// ─── Test 2: Block Serialize / Deserialize ───────────────────────────

void TestBlock() {
    Block block;
    
    assert(block.IsEmpty());

    block.Add({"apple", {1, 2, 3}, false});
    block.Add({"banana", {4, 5}, false});
    block.Add({"cherry", {}, true}); // Tombstone

    assert(!block.IsEmpty());
    assert(block.EntryCount() == 3);
    assert(block.LastKey() == "cherry");

    // Serialize → Deserialize roundtrip
    auto serialized = block.Serialize();
    auto deserialized_or = Block::Deserialize(serialized);
    assert(deserialized_or.IsOk());
    auto& deserialized = deserialized_or.value();

    assert(deserialized.EntryCount() == 3);
    assert(deserialized.Entries()[0].key == "apple");
    assert(deserialized.Entries()[1].value == std::vector<uint8_t>({4, 5}));
    assert(deserialized.Entries()[2].is_tombstone == true);

    // Point lookup via binary search
    auto found = deserialized.Get("banana");
    assert(found.has_value());
    assert(found->value == std::vector<uint8_t>({4, 5}));

    auto not_found = deserialized.Get("dragonfruit");
    assert(!not_found.has_value());

    std::cout << "  TestBlock: PASSED" << std::endl;
}

// ─── Test 3: Full SSTable Write → Read roundtrip ─────────────────────

void TestSSTableWriteAndRead() {
    Cleanup();

    // Build a MemTable with sorted data
    MemTable mem;
    for (int i = 0; i < 500; ++i) {
        // Zero-pad keys so they sort lexicographically: key_000, key_001, ...
        char key[16];
        snprintf(key, sizeof(key), "key_%03d", i);
        mem.Put(key, {static_cast<uint8_t>(i % 256), static_cast<uint8_t>(i / 256)});
    }
    // Add a tombstone
    mem.Delete("key_042");

    // Write SSTable from MemTable iterator
    auto iter = mem.Begin();
    auto write_status = SSTableWriter::Write(kTestSSTablePath, iter);
    assert(write_status.IsOk());

    // Open the SSTable
    auto reader_or = SSTableReader::Open(kTestSSTablePath);
    assert(reader_or.IsOk());
    auto& reader = reader_or.value();

    assert(reader->NumEntries() == 500); // 499 puts + 1 tombstone

    // Point lookup — existing key
    auto result_or = reader->Get("key_100");
    assert(result_or.IsOk());
    assert(result_or.value().has_value());
    assert(result_or.value()->key == "key_100");
    assert(!result_or.value()->is_tombstone);

    // Point lookup — tombstone key
    auto tomb_or = reader->Get("key_042");
    assert(tomb_or.IsOk());
    assert(tomb_or.value().has_value());
    assert(tomb_or.value()->is_tombstone);

    // Point lookup — non-existent key (bloom filter should reject most of these)
    auto miss_or = reader->Get("nonexistent_key");
    assert(miss_or.IsOk());
    assert(!miss_or.value().has_value());

    // ReadAll for compaction
    auto all_or = reader->ReadAll();
    assert(all_or.IsOk());
    assert(all_or.value().size() == 500);
    // Verify sorted order
    for (size_t i = 1; i < all_or.value().size(); ++i) {
        assert(all_or.value()[i - 1].key < all_or.value()[i].key);
    }

    Cleanup();
    std::cout << "  TestSSTableWriteAndRead: PASSED" << std::endl;
}

// ─── Test 4: Bloom filter integration in SSTable ─────────────────────

void TestSSTableBloomRejectsAbsent() {
    Cleanup();

    MemTable mem;
    for (int i = 0; i < 100; ++i) {
        mem.Put("exists_" + std::to_string(i), {1});
    }

    auto iter = mem.Begin();
    auto ws = SSTableWriter::Write(kTestSSTablePath, iter);
    assert(ws.IsOk());

    auto reader_or = SSTableReader::Open(kTestSSTablePath);
    assert(reader_or.IsOk());
    auto& reader = reader_or.value();

    // All existing keys should be found
    for (int i = 0; i < 100; ++i) {
        auto r = reader->Get("exists_" + std::to_string(i));
        assert(r.IsOk());
        assert(r.value().has_value());
    }

    // Non-existent keys should mostly be rejected by bloom filter
    int not_found_count = 0;
    for (int i = 0; i < 1000; ++i) {
        auto r = reader->Get("absent_" + std::to_string(i));
        assert(r.IsOk());
        if (!r.value().has_value()) not_found_count++;
    }
    assert(not_found_count == 1000); // All must be correctly rejected

    Cleanup();
    std::cout << "  TestSSTableBloomRejectsAbsent: PASSED" << std::endl;
}

int main() {
    std::cout << "Testing SSTable..." << std::endl;
    TestBloomFilter();
    TestBlock();
    TestSSTableWriteAndRead();
    TestSSTableBloomRejectsAbsent();
    std::cout << "All SSTable tests passed." << std::endl;
    return 0;
}
