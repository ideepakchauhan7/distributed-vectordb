#include "src/storage/cache/block_cache.h"
#include <iostream>

using namespace vectordb::storage;

int main() {
    BlockCache cache(1);
    std::vector<uint8_t> huge_block(500 * 1024, 'X');
    cache.Put("block1", huge_block);
    std::cout << cache.current_size_bytes() << std::endl;
    cache.Put("block2", huge_block);
    std::cout << cache.current_size_bytes() << std::endl;
    std::cout << cache.Get("block1").has_value() << std::endl;
    return 0;
}
