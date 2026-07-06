#pragma once

#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <random>

namespace vectordb {
namespace storage {

struct SkipListValue {
    bool is_tombstone{false};
    std::vector<uint8_t> data;
};

// Forward declaration of Node
struct SkipListNode;

/**
 * @class SkipList
 * @brief A thread-safe (via shared_mutex) Skip List mapping std::string to SkipListValue.
 * 
 * Supports single-writer, multiple-reader concurrency semantics via locks.
 * Optimized for fast sequential iteration for flushing to SSTables.
 */
class SkipList {
public:
    static constexpr int kMaxHeight = 12; // Supports up to 2^12 (4096) elements optimally, enough for MemTable
    
    SkipList();
    ~SkipList();

    // Disallow copy/move
    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    void Put(const std::string& key, const std::vector<uint8_t>& value);
    void Delete(const std::string& key);
    
    bool Get(const std::string& key, SkipListValue& out_value) const;

    size_t ApproximateMemoryUsage() const;
    bool IsEmpty() const;

    // Iteration support for flushing to SSTable
    class Iterator {
    public:
        explicit Iterator(SkipListNode* node);
        bool Valid() const;
        void Next();
        const std::string& Key() const;
        const SkipListValue& Value() const;
    private:
        SkipListNode* current_;
    };

    Iterator Begin() const;

private:
    int RandomHeight();
    
    SkipListNode* head_;
    int max_height_;
    
    // Concurrency control:
    // Writers (Put/Delete) take a unique lock. Readers (Get/Iterate) take a shared lock.
    mutable std::shared_mutex mutex_;
    
    // Random number generation for node height
    std::mt19937 rnd_;
    
    size_t memory_usage_{0};
};

} // namespace storage
} // namespace vectordb
