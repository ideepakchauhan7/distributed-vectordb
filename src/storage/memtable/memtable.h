#pragma once

#include <string>
#include <vector>
#include <optional>
#include "src/storage/memtable/skip_list.h"

namespace vectordb {
namespace storage {

/**
 * @class MemTable
 * @brief In-memory write buffer representing the most recent database mutations.
 * 
 * Mutations (Put/Delete) go to the MemTable after being durably logged in the WAL.
 * When the MemTable exceeds a configured size, it becomes immutable and is flushed
 * to an SSTable on disk.
 */
class MemTable {
public:
    MemTable() = default;
    ~MemTable() = default;

    // Disallow copy/move
    MemTable(const MemTable&) = delete;
    MemTable& operator=(const MemTable&) = delete;

    /**
     * @brief Inserts or updates a key-value pair.
     * @param key The unique string identifier.
     * @param value The serialized binary value (e.g., a vector).
     */
    void Put(const std::string& key, const std::vector<uint8_t>& value);

    /**
     * @brief Marks a key as deleted using a tombstone.
     * @param key The unique string identifier to delete.
     */
    void Delete(const std::string& key);

    /**
     * @brief Retrieves a value by key.
     * @param key The unique string identifier.
     * @return std::nullopt if the key is not found OR if it is marked as deleted.
     *         Otherwise, returns the binary value.
     */
    std::optional<std::vector<uint8_t>> Get(const std::string& key) const;

    /**
     * @brief Returns the approximate memory usage of this MemTable in bytes.
     */
    size_t ApproximateMemoryUsage() const;

    /**
     * @brief Returns true if the MemTable contains zero records.
     */
    bool IsEmpty() const;

    /**
     * @brief Get an iterator for flushing the MemTable to an SSTable.
     * @return SkipList::Iterator pointing to the first element (sorted by key).
     */
    SkipList::Iterator Begin() const;

private:
    SkipList table_;
};

} // namespace storage
} // namespace vectordb
