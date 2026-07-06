#include "src/storage/wal/wal.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>

using namespace vectordb::storage;
using namespace vectordb::common;

static const std::string kTestWALPath = "build/test_data/test_wal.log";

void Cleanup() {
    std::filesystem::remove_all("build/test_data");
}

void TestBasicAppendAndRead() {
    Cleanup();
    
    // Open a fresh WAL
    auto wal_or = WAL::Open(kTestWALPath);
    if (!wal_or.IsOk()) { std::cerr << "Open failed: " << wal_or.status().message() << "\n"; exit(1); }
    auto& wal = wal_or.value();

    // Append 5 entries
    for (int i = 0; i < 5; ++i) {
        WALEntry entry;
        entry.type = WALEntryType::kPut;
        entry.key = "vec_" + std::to_string(i);
        std::string val = "data_" + std::to_string(i);
        entry.value.assign(val.begin(), val.end());

        auto seq_or = wal->Append(entry);
        if (!seq_or.IsOk()) { std::cerr << "Append failed\n"; exit(1); }
        if (seq_or.value() != static_cast<uint64_t>(i + 1)) { 
            std::cerr << "Sequence mismatch: expected " << (i+1) << " got " << seq_or.value() << "\n"; 
            exit(1); 
        }
    }

    // Append a delete entry
    WALEntry del;
    del.type = WALEntryType::kDelete;
    del.key = "vec_2";
    auto del_seq = wal->Append(del);
    if (!del_seq.IsOk()) { std::cerr << "Delete append failed\n"; exit(1); }

    // Read all entries back
    auto entries_or = wal->ReadAll();
    if (!entries_or.IsOk()) { std::cerr << "ReadAll failed\n"; exit(1); }
    auto& entries = entries_or.value();

    if (entries.size() != 6) { 
        std::cerr << "Expected 6 entries, got " << entries.size() << "\n"; exit(1); 
    }
    if (entries[0].key != "vec_0") { std::cerr << "Key mismatch at 0\n"; exit(1); }
    if (entries[5].type != WALEntryType::kDelete) { std::cerr << "Type mismatch at 5\n"; exit(1); }
    if (entries[5].key != "vec_2") { std::cerr << "Delete key mismatch\n"; exit(1); }

    std::cout << "  BasicAppendAndRead: PASSED" << std::endl;
}

void TestCrashRecovery() {
    Cleanup();
    
    // Write 3 entries then "crash" (destroy the WAL object)
    {
        auto wal_or = WAL::Open(kTestWALPath);
        auto& wal = wal_or.value();

        for (int i = 0; i < 3; ++i) {
            WALEntry entry;
            entry.type = WALEntryType::kPut;
            entry.key = "crash_test_" + std::to_string(i);
            entry.value = {0x01, 0x02, 0x03};
            wal->Append(entry);
        }
        // WAL destructor flushes and closes the file — simulating graceful crash
    }

    // "Restart" — reopen the WAL and verify all entries survived
    {
        auto wal_or = WAL::Open(kTestWALPath);
        auto& wal = wal_or.value();

        auto entries_or = wal->ReadAll();
        if (!entries_or.IsOk()) { std::cerr << "Recovery read failed\n"; exit(1); }
        if (entries_or.value().size() != 3) { 
            std::cerr << "Expected 3 recovered entries, got " << entries_or.value().size() << "\n"; 
            exit(1); 
        }

        // Sequence numbers should continue from where we left off
        WALEntry entry;
        entry.type = WALEntryType::kPut;
        entry.key = "post_crash";
        entry.value = {0x04};
        auto seq = wal->Append(entry);
        if (!seq.IsOk() || seq.value() != 4) {
            std::cerr << "Post-crash sequence wrong: " << seq.value() << "\n"; exit(1);
        }
    }

    std::cout << "  CrashRecovery: PASSED" << std::endl;
}

void TestCorruptionDetection() {
    Cleanup();

    // Write 3 valid entries
    {
        auto wal_or = WAL::Open(kTestWALPath);
        auto& wal = wal_or.value();

        for (int i = 0; i < 3; ++i) {
            WALEntry entry;
            entry.type = WALEntryType::kPut;
            entry.key = "key_" + std::to_string(i);
            entry.value = {static_cast<uint8_t>(i)};
            wal->Append(entry);
        }
    }

    // Corrupt the file by appending garbage bytes (simulating power loss mid-write)
    {
        std::ofstream corrupter(kTestWALPath, std::ios::binary | std::ios::app);
        uint8_t garbage[] = {0xFF, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x42};
        corrupter.write(reinterpret_cast<char*>(garbage), sizeof(garbage));
    }

    // Reopen — should recover the 3 valid entries and stop at the garbage
    {
        auto wal_or = WAL::Open(kTestWALPath);
        auto& wal = wal_or.value();

        auto entries_or = wal->ReadAll();
        if (!entries_or.IsOk()) { std::cerr << "Corruption read failed\n"; exit(1); }
        if (entries_or.value().size() != 3) {
            std::cerr << "Expected 3 entries before corruption, got " << entries_or.value().size() << "\n";
            exit(1);
        }
    }

    std::cout << "  CorruptionDetection: PASSED" << std::endl;
}

int main() {
    std::cout << "Testing WAL..." << std::endl;
    TestBasicAppendAndRead();
    TestCrashRecovery();
    TestCorruptionDetection();
    Cleanup();
    std::cout << "All WAL tests passed." << std::endl;
    return 0;
}
