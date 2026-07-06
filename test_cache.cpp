#include "src/storage/cache/block_cache.h"
#include <iostream>
#include <cassert>

using namespace vectordb::storage;

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "Assertion failed: " << #expr << std::endl; std::abort(); } } while(0)

void TestCacheBasic() {
    // 1 MB cache
    BlockCache cache(1);

    auto key1 = BlockCache::MakeCacheKey("/data/1.sst", 0);
    std::vector<uint8_t> data1(4096, 'A');

    cache.Put(key1, data1);
    
    auto result = cache.Get(key1);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value() == data1);
    
    ASSERT_TRUE(cache.hits() == 1);
    ASSERT_TRUE(cache.misses() == 0);

    auto result_miss = cache.Get("nonexistent");
    ASSERT_TRUE(!result_miss.has_value());
    ASSERT_TRUE(cache.misses() == 1);

    std::cout << "  TestCacheBasic: PASSED" << std::endl;
}

void TestCacheEviction() {
    // 1 MB cache
    BlockCache cache(1);

    // 1 MB is 1,048,576 bytes.
    // Let's create blocks of ~500 KB each.
    std::vector<uint8_t> huge_block(500 * 1024, 'X');

    // Add block 1
    cache.Put("block1", huge_block);
    // Add block 2
    cache.Put("block2", huge_block);
    
    // Both should be in cache (total ~1000 KB < 1MB)
    ASSERT_TRUE(cache.Get("block2").has_value());
    ASSERT_TRUE(cache.Get("block1").has_value()); // block1 becomes MRU

    // By calling Get("block1"), block1 is now the MRU. block2 is the LRU.
    
    // Add block 3
    cache.Put("block3", huge_block);

    // Total would be 1500 KB, exceeding 1MB.
    // LRU (block2) should be evicted.
    ASSERT_TRUE(cache.Get("block1").has_value()); // Still here (was MRU)
    ASSERT_TRUE(!cache.Get("block2").has_value()); // Evicted
    ASSERT_TRUE(cache.Get("block3").has_value()); // Just added

    std::cout << "  TestCacheEviction: PASSED" << std::endl;
}

int main() {
    std::cout << "Testing BlockCache..." << std::endl;
    TestCacheBasic();
    TestCacheEviction();
    std::cout << "All BlockCache tests passed." << std::endl;
    return 0;
}
