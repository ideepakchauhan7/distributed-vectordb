#include "src/storage/memtable/skip_list.h"
#include <vector>
#include <cassert>
#include <mutex>

namespace vectordb {
namespace storage {

struct SkipListNode {
    std::string key;
    SkipListValue value;
    std::vector<SkipListNode*> forward;

    SkipListNode(const std::string& k, const SkipListValue& v, int height)
        : key(k), value(v), forward(height, nullptr) {}
};

SkipList::SkipList() 
    : head_(new SkipListNode("", SkipListValue{}, kMaxHeight)), max_height_(1), rnd_(42) { // 42 is fixed seed for reproducibility, can use random_device if preferred
    memory_usage_ = sizeof(SkipList) + sizeof(SkipListNode) + (kMaxHeight * sizeof(SkipListNode*));
}

SkipList::~SkipList() {
    SkipListNode* current = head_;
    while (current != nullptr) {
        SkipListNode* next = current->forward[0];
        delete current;
        current = next;
    }
}

int SkipList::RandomHeight() {
    int height = 1;
    // 25% probability of increasing height
    while (height < kMaxHeight && (rnd_() % 4 == 0)) {
        height++;
    }
    return height;
}

void SkipList::Put(const std::string& key, const std::vector<uint8_t>& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::vector<SkipListNode*> update(kMaxHeight, nullptr);
    SkipListNode* current = head_;

    for (int i = max_height_ - 1; i >= 0; i--) {
        while (current->forward[i] != nullptr && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // If key exists, update value
    if (current != nullptr && current->key == key) {
        memory_usage_ -= current->value.data.size();
        current->value.is_tombstone = false;
        current->value.data = value;
        memory_usage_ += current->value.data.size();
        return;
    }

    // Insert new node
    int height = RandomHeight();
    if (height > max_height_) {
        for (int i = max_height_; i < height; i++) {
            update[i] = head_;
        }
        max_height_ = height;
    }

    SkipListValue v;
    v.is_tombstone = false;
    v.data = value;
    SkipListNode* new_node = new SkipListNode(key, v, height);

    for (int i = 0; i < height; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    memory_usage_ += sizeof(SkipListNode) + key.size() + value.size() + (height * sizeof(SkipListNode*));
}

void SkipList::Delete(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::vector<SkipListNode*> update(kMaxHeight, nullptr);
    SkipListNode* current = head_;

    for (int i = max_height_ - 1; i >= 0; i--) {
        while (current->forward[i] != nullptr && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // If key exists, convert to tombstone rather than deleting the node
    // This allows concurrent readers to still traverse the list safely and SSTable flush to see the deletion.
    if (current != nullptr && current->key == key) {
        if (!current->value.is_tombstone) {
            memory_usage_ -= current->value.data.size();
            current->value.data.clear();
            current->value.is_tombstone = true;
        }
        return;
    }

    // If it doesn't exist, we must insert a tombstone so the delete propagates to SSTables
    int height = RandomHeight();
    if (height > max_height_) {
        for (int i = max_height_; i < height; i++) {
            update[i] = head_;
        }
        max_height_ = height;
    }

    SkipListValue v;
    v.is_tombstone = true;
    SkipListNode* new_node = new SkipListNode(key, v, height);

    for (int i = 0; i < height; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    memory_usage_ += sizeof(SkipListNode) + key.size() + (height * sizeof(SkipListNode*));
}

bool SkipList::Get(const std::string& key, SkipListValue& out_value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    SkipListNode* current = head_;

    for (int i = max_height_ - 1; i >= 0; i--) {
        while (current->forward[i] != nullptr && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if (current != nullptr && current->key == key) {
        out_value = current->value;
        return true;
    }

    return false;
}

size_t SkipList::ApproximateMemoryUsage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return memory_usage_;
}

bool SkipList::IsEmpty() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return head_->forward[0] == nullptr;
}

// ─── Iterator ────────────────────────────────────────────────────────

SkipList::Iterator::Iterator(SkipListNode* node) : current_(node) {}

bool SkipList::Iterator::Valid() const {
    return current_ != nullptr;
}

void SkipList::Iterator::Next() {
    assert(Valid());
    current_ = current_->forward[0];
}

const std::string& SkipList::Iterator::Key() const {
    assert(Valid());
    return current_->key;
}

const SkipListValue& SkipList::Iterator::Value() const {
    assert(Valid());
    return current_->value;
}

SkipList::Iterator SkipList::Begin() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return Iterator(head_->forward[0]);
}

} // namespace storage
} // namespace vectordb
