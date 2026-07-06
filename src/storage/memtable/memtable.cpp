#include "src/storage/memtable/memtable.h"

namespace vectordb {
namespace storage {

void MemTable::Put(const std::string& key, const std::vector<uint8_t>& value) {
    table_.Put(key, value);
}

void MemTable::Delete(const std::string& key) {
    table_.Delete(key);
}

std::optional<std::vector<uint8_t>> MemTable::Get(const std::string& key) const {
    SkipListValue val;
    bool found = table_.Get(key, val);
    
    if (found && !val.is_tombstone) {
        return val.data;
    }
    
    // If not found, or if it is a tombstone (deleted), return nullopt
    return std::nullopt;
}

size_t MemTable::ApproximateMemoryUsage() const {
    return table_.ApproximateMemoryUsage();
}

bool MemTable::IsEmpty() const {
    return table_.IsEmpty();
}

SkipList::Iterator MemTable::Begin() const {
    return table_.Begin();
}

} // namespace storage
} // namespace vectordb
