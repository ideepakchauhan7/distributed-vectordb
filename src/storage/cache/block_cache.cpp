#include "src/storage/cache/block_cache.h"
#include <sstream>

namespace vectordb {
namespace storage {

BlockCache::BlockCache(size_t capacity_mb) 
    : capacity_bytes_(capacity_mb * 1024 * 1024) {}

std::string BlockCache::MakeCacheKey(const std::string& sstable_filepath, uint64_t block_offset) {
    std::ostringstream oss;
    oss << sstable_filepath << ":" << block_offset;
    return oss.str();
}

std::optional<std::vector<uint8_t>> BlockCache::Get(const std::string& cache_key) {
    // Attempt an optimistic read lock
    {
        std::shared_lock<std::shared_mutex> read_lock(mutex_);
        auto it = hash_map_.find(cache_key);
        if (it == hash_map_.end()) {
            misses_++;
            return std::nullopt;
        }
    }

    // Cache hit: we need to promote this item to the front of the LRU, 
    // which modifies the list. We need a write lock for that.
    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    
    // Double check it wasn't evicted between dropping the read lock and acquiring the write lock
    auto it = hash_map_.find(cache_key);
    if (it == hash_map_.end()) {
        misses_++;
        return std::nullopt;
    }

    // Move to front (Most Recently Used)
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    hits_++;

    return it->second->data;
}

void BlockCache::Put(const std::string& cache_key, const std::vector<uint8_t>& block_data) {
    size_t item_size = cache_key.size() + block_data.size();
    
    // Ignore items larger than the entire cache (rare edge case)
    if (item_size > capacity_bytes_) return;

    std::unique_lock<std::shared_mutex> write_lock(mutex_);

    // If it already exists, remove the old one first
    auto it = hash_map_.find(cache_key);
    if (it != hash_map_.end()) {
        current_size_bytes_ -= (it->second->key.size() + it->second->data.size());
        lru_list_.erase(it->second);
        hash_map_.erase(it);
    }

    // Insert new item at the front
    lru_list_.push_front({cache_key, block_data});
    hash_map_[cache_key] = lru_list_.begin();
    current_size_bytes_ += item_size;

    // Evict items from the back until we fit within capacity
    while (current_size_bytes_ > capacity_bytes_ && !lru_list_.empty()) {
        auto& last = lru_list_.back();
        size_t last_size = last.key.size() + last.data.size();
        
        hash_map_.erase(last.key);
        lru_list_.pop_back();
        current_size_bytes_ -= last_size;
    }
}

} // namespace storage
} // namespace vectordb
