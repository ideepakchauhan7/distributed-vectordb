#include "src/storage/checkpoint/checkpoint_manager.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace vectordb::storage;
namespace fs = std::filesystem;

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " << #expr << std::endl; std::abort(); } } while(0)

void TestCheckpointSaveLoad() {
    std::string dir = "/tmp/test_checkpoint_db";
    fs::remove_all(dir);

    CheckpointManager manager(dir);

    // Initial empty state
    auto cp_empty_res = manager.LoadCheckpoint();
    ASSERT_TRUE(cp_empty_res.IsOk());
    ASSERT_TRUE(cp_empty_res.value().last_flushed_lsn == 0);
    ASSERT_TRUE(cp_empty_res.value().sstables.empty());

    // Save a checkpoint
    Checkpoint cp;
    cp.last_flushed_lsn = 42000;
    
    SSTableMeta meta1;
    meta1.level = 0;
    meta1.file_size_bytes = 4096;
    meta1.sequence_number = 100;
    meta1.smallest_key = "apple";
    meta1.largest_key = "banana";
    meta1.filepath = dir + "/001.sst";

    SSTableMeta meta2;
    meta2.level = 1;
    meta2.file_size_bytes = 8192;
    meta2.sequence_number = 200;
    meta2.smallest_key = "carrot";
    meta2.largest_key = "dog";
    meta2.filepath = dir + "/002.sst";

    cp.sstables.push_back(meta1);
    cp.sstables.push_back(meta2);

    auto status = manager.SaveCheckpoint(cp);
    ASSERT_TRUE(status.IsOk());

    // Load and verify
    auto cp_loaded_res = manager.LoadCheckpoint();
    ASSERT_TRUE(cp_loaded_res.IsOk());
    auto cp_loaded = cp_loaded_res.value();

    ASSERT_TRUE(cp_loaded.last_flushed_lsn == 42000);
    ASSERT_TRUE(cp_loaded.sstables.size() == 2);

    ASSERT_TRUE(cp_loaded.sstables[0].filepath == dir + "/001.sst");
    ASSERT_TRUE(cp_loaded.sstables[0].smallest_key == "apple");
    ASSERT_TRUE(cp_loaded.sstables[1].level == 1);
    ASSERT_TRUE(cp_loaded.sstables[1].largest_key == "dog");

    fs::remove_all(dir);
    std::cout << "  TestCheckpointSaveLoad: PASSED" << std::endl;
}

int main() {
    std::cout << "Testing CheckpointManager..." << std::endl;
    TestCheckpointSaveLoad();
    std::cout << "All CheckpointManager tests passed." << std::endl;
    return 0;
}
