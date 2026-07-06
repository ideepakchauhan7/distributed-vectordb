#include "src/storage/recovery/recovery_manager.h"
#include "src/storage/wal/wal.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>

using namespace vectordb::storage;

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " << #expr << std::endl; std::abort(); } } while(0)
#define ASSERT_OK(expr) do { auto&& s_ = (expr); if (!s_.IsOk()) { std::cerr << "Assertion failed: " << #expr << " returns error" << std::endl; std::abort(); } } while(0)

static const std::string kTestWalPath = "/tmp/test_recovery.wal";

void Cleanup() {
    std::filesystem::remove(kTestWalPath);
}

// ─── Test 1: Full Recovery ───────────────────────────────────────────────────

void TestFullRecovery() {
    Cleanup();

    // 1. Simulate normal operation (writing to WAL)
    {
        auto wal_or = WAL::Open(kTestWalPath);
        ASSERT_OK(wal_or);
        auto& wal = wal_or.value();

        WALEntry put1{1, WALEntryType::kPut, "key1", {10, 20}};
        WALEntry put2{2, WALEntryType::kPut, "key2", {30}};
        WALEntry del1{3, WALEntryType::kDelete, "key1", {}};
        
        ASSERT_OK(wal->Append(put1));
        ASSERT_OK(wal->Append(put2));
        ASSERT_OK(wal->Append(del1));
        ASSERT_OK(wal->Sync());
    } // WAL is closed here (simulating crash)

    // 2. Perform recovery
    MemTable mem;
    auto recovered_count_or = RecoveryManager::RecoverMemTable(kTestWalPath, mem);
    ASSERT_OK(recovered_count_or);
    ASSERT_TRUE(recovered_count_or.value() == 3);

    // 3. Verify MemTable state
    // key1 was deleted, so Get() should return nullopt
    auto result1 = mem.Get("key1");
    ASSERT_TRUE(!result1.has_value());

    // key2 should have value {30}
    auto result2 = mem.Get("key2");
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result2.value() == std::vector<uint8_t>({30}));

    std::cout << "  TestFullRecovery: PASSED" << std::endl;
    Cleanup();
}

// ─── Test 2: Recovery with Checkpoint LSN ────────────────────────────────────

void TestRecoveryWithCheckpoint() {
    Cleanup();

    // 1. Simulate writes
    {
        auto wal_or = WAL::Open(kTestWalPath);
        ASSERT_OK(wal_or);
        auto& wal = wal_or.value();

        WALEntry put1{1, WALEntryType::kPut, "key1", {1}};
        WALEntry put2{2, WALEntryType::kPut, "key2", {2}}; // Imagine checkpoint happens after this
        WALEntry put3{3, WALEntryType::kPut, "key3", {3}};
        WALEntry put4{4, WALEntryType::kPut, "key4", {4}};
        
        ASSERT_OK(wal->Append(put1));
        ASSERT_OK(wal->Append(put2));
        ASSERT_OK(wal->Append(put3));
        ASSERT_OK(wal->Append(put4));
    }

    // 2. Perform recovery with last_flushed_lsn = 2
    MemTable mem;
    auto recovered_count_or = RecoveryManager::RecoverMemTable(kTestWalPath, mem, 2);
    ASSERT_OK(recovered_count_or);
    
    // Should only apply LSN 3 and 4
    ASSERT_TRUE(recovered_count_or.value() == 2);

    ASSERT_TRUE(!mem.Get("key1").has_value()); // Was flushed, not recovered
    ASSERT_TRUE(!mem.Get("key2").has_value()); // Was flushed, not recovered
    ASSERT_TRUE(mem.Get("key3").has_value());  // Recovered
    ASSERT_TRUE(mem.Get("key4").has_value());  // Recovered

    std::cout << "  TestRecoveryWithCheckpoint: PASSED" << std::endl;
    Cleanup();
}

// ─── Test 3: No WAL File ─────────────────────────────────────────────────────

void TestNoWalFile() {
    Cleanup();
    
    MemTable mem;
    auto recovered_count_or = RecoveryManager::RecoverMemTable(kTestWalPath, mem);
    
    ASSERT_OK(recovered_count_or);
    ASSERT_TRUE(recovered_count_or.value() == 0); // Nothing applied

    std::cout << "  TestNoWalFile: PASSED" << std::endl;
    Cleanup();
}

int main() {
    std::cout << "Testing RecoveryManager..." << std::endl;
    TestFullRecovery();
    TestRecoveryWithCheckpoint();
    TestNoWalFile();
    std::cout << "All RecoveryManager tests passed." << std::endl;
    return 0;
}
