#include "src/storage/engine/storage_engine.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

using namespace vectordb::storage;
namespace fs = std::filesystem;

#define ASSERT_TRUE(expr) do { if (!(expr)) { \
    std::cerr << "FAILED: " << #expr << " (line " << __LINE__ << ")" << std::endl; \
    std::abort(); } } while(0)

#define ASSERT_OK(status) do { if (!(status).IsOk()) { \
    std::cerr << "FAILED: Status error: " << (status).message() << " (line " << __LINE__ << ")" << std::endl; \
    std::abort(); } } while(0)

const std::string kTestDir = "/tmp/test_storage_engine";

StorageConfig MakeConfig() {
    StorageConfig cfg;
    cfg.data_dir = kTestDir;
    cfg.wal_dir  = kTestDir + "/wal";
    cfg.sst_dir  = kTestDir + "/sst";
    cfg.memtable_size_bytes = 1024; // 1KB — tiny, so flushes trigger quickly in tests
    cfg.block_cache_mb = 1;
    return cfg;
}

std::vector<uint8_t> ToBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::string FromBytes(const std::vector<uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

// ─── Test 1: Basic Put and Get ───────────────────────────────────────────────
void TestBasicPutGet() {
    fs::remove_all(kTestDir);

    auto engine_result = StorageEngine::Open(MakeConfig());
    ASSERT_TRUE(engine_result.IsOk());
    auto& engine = engine_result.value();

    ASSERT_OK(engine->Put("apple", ToBytes("fruit")));
    ASSERT_OK(engine->Put("banana", ToBytes("yellow_fruit")));
    ASSERT_OK(engine->Put("carrot", ToBytes("vegetable")));

    auto r1 = engine->Get("apple");
    ASSERT_TRUE(r1.IsOk() && r1.value().has_value());
    ASSERT_TRUE(FromBytes(r1.value().value()) == "fruit");

    auto r2 = engine->Get("banana");
    ASSERT_TRUE(r2.IsOk() && r2.value().has_value());
    ASSERT_TRUE(FromBytes(r2.value().value()) == "yellow_fruit");

    auto r_miss = engine->Get("dragon_fruit");
    ASSERT_TRUE(r_miss.IsOk() && !r_miss.value().has_value());

    std::cout << "  TestBasicPutGet: PASSED" << std::endl;
}

// ─── Test 2: Delete (Tombstone) ───────────────────────────────────────────────
void TestDelete() {
    fs::remove_all(kTestDir);

    auto engine_result = StorageEngine::Open(MakeConfig());
    ASSERT_TRUE(engine_result.IsOk());
    auto& engine = engine_result.value();

    ASSERT_OK(engine->Put("key1", ToBytes("value1")));
    
    auto r_before = engine->Get("key1");
    ASSERT_TRUE(r_before.IsOk() && r_before.value().has_value());

    ASSERT_OK(engine->Delete("key1"));

    // After delete, MemTable returns nullopt for tombstoned key
    auto r_after = engine->Get("key1");
    ASSERT_TRUE(r_after.IsOk() && !r_after.value().has_value());

    std::cout << "  TestDelete: PASSED" << std::endl;
}

// ─── Test 3: MemTable Flush (write enough data to trigger it) ────────────────
void TestFlushTriggered() {
    fs::remove_all(kTestDir);

    auto engine_result = StorageEngine::Open(MakeConfig());
    ASSERT_TRUE(engine_result.IsOk());
    auto& engine = engine_result.value();

    // Write enough data to exceed 1KB memtable threshold — this triggers a flush
    for (int i = 0; i < 50; ++i) {
        std::string key = "vec_" + std::to_string(i);
        std::string val(50, static_cast<char>('A' + (i % 26)));
        ASSERT_OK(engine->Put(key, ToBytes(val)));
    }

    ASSERT_TRUE(engine->memtable_flushes() >= 1);

    // Data should still be readable even after flush (now on SSTable)
    auto r = engine->Get("vec_0");
    ASSERT_TRUE(r.IsOk() && r.value().has_value());

    std::cout << "  TestFlushTriggered: PASSED (flushes=" 
              << engine->memtable_flushes() << ")" << std::endl;
}

// ─── Test 4: Crash Recovery ───────────────────────────────────────────────────
void TestCrashRecovery() {
    fs::remove_all(kTestDir);

    // Write some data and then destroy the engine (simulating a clean shutdown)
    {
        auto engine_result = StorageEngine::Open(MakeConfig());
        ASSERT_TRUE(engine_result.IsOk());
        auto& engine = engine_result.value();

        ASSERT_OK(engine->Put("persistent_key", ToBytes("persistent_value")));
        // Engine destructor flushes MemTable and writes final checkpoint
    }

    // Re-open the engine — recovery should restore the data
    {
        auto engine_result = StorageEngine::Open(MakeConfig());
        ASSERT_TRUE(engine_result.IsOk());
        auto& engine = engine_result.value();

        auto r = engine->Get("persistent_key");
        ASSERT_TRUE(r.IsOk() && r.value().has_value());
        ASSERT_TRUE(FromBytes(r.value().value()) == "persistent_value");
    }

    std::cout << "  TestCrashRecovery: PASSED" << std::endl;
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "Testing StorageEngine..." << std::endl;
    TestBasicPutGet();
    TestDelete();
    TestFlushTriggered();
    TestCrashRecovery();
    std::cout << "\nAll StorageEngine tests PASSED ✅" << std::endl;
    fs::remove_all(kTestDir);
    return 0;
}
