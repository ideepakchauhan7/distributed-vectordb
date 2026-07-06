#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <optional>
#include <mutex>

namespace vectordb {
namespace storage {

/**
 * @class BlockCache
 * @brief An LRU cache for 4KB SSTable data blocks.
 *
 * This prevents the database from repeatedly reading the same blocks from disk
 * during searches or range scans. It uses an LRU (Least Recently Used) eviction
 * policy when it reaches capacity.
 *
 * Thread-safety is achieved using a shared_mutex (read-write lock) to allow
 * concurrent cache hits.
 */
class BlockCache {
public:
    /**
     * @param capacity_mb The maximum size of the cache in megabytes.
     */
    explicit BlockCache(size_t capacity_mb);

    ~BlockCache() = default;

    /**
     * @brief Formats a unique cache key for a block.
     */
    static std::string MakeCacheKey(const std::string& sstable_filepath, uint64_t block_offset);

    /**
     * @brief Retrieves a block from the cache.
     * @return The block data if found (Cache Hit), or std::nullopt (Cache Miss).
     */
    std::optional<std::vector<uint8_t>> Get(const std::string& cache_key);

    /**
     * @brief Inserts a block into the cache. Evicts the oldest block if full.
     */
    void Put(const std::string& cache_key, const std::vector<uint8_t>& block_data);

    // Metrics
    size_t hits() const { return hits_; }
    size_t misses() const { return misses_; }
    size_t current_size_bytes() const { return current_size_bytes_; }
    size_t capacity_bytes() const { return capacity_bytes_; }

private:
    struct CacheItem {
        std::string key;
        std::vector<uint8_t> data;
    };

    size_t capacity_bytes_;
    size_t current_size_bytes_ = 0;

    std::list<CacheItem> lru_list_;
    std::unordered_map<std::string, decltype(lru_list_)::iterator> hash_map_;

    mutable std::shared_mutex mutex_; // Read-Write lock

    // Metrics
    size_t hits_ = 0;
    size_t misses_ = 0;
};

} // namespace storage
} // namespace vectordb
