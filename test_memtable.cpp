#include "src/storage/memtable/memtable.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace vectordb::storage;

void TestBasicOperations() {
    MemTable mem;
    assert(mem.IsEmpty());
    assert(mem.ApproximateMemoryUsage() > 0); // Base size

    // Put
    mem.Put("key1", {1, 2, 3});
    mem.Put("key2", {4, 5, 6});
    assert(!mem.IsEmpty());

    // Get
    auto val1 = mem.Get("key1");
    assert(val1.has_value());
    assert(val1.value() == std::vector<uint8_t>({1, 2, 3}));

    // Update
    mem.Put("key1", {7, 8, 9});
    val1 = mem.Get("key1");
    assert(val1.has_value());
    assert(val1.value() == std::vector<uint8_t>({7, 8, 9}));

    // Delete
    mem.Delete("key1");
    val1 = mem.Get("key1");
    assert(!val1.has_value()); // Should be a tombstone now

    // Missing key
    auto val3 = mem.Get("missing_key");
    assert(!val3.has_value());

    std::cout << "  TestBasicOperations: PASSED" << std::endl;
}

void TestIteration() {
    MemTable mem;
    mem.Put("c", {3});
    mem.Put("a", {1});
    mem.Put("b", {2});
    mem.Delete("b"); // Tombstone

    auto it = mem.Begin();
    
    // Iteration should be sorted: a, b (tombstone), c
    assert(it.Valid());
    assert(it.Key() == "a");
    assert(!it.Value().is_tombstone);
    assert(it.Value().data[0] == 1);
    
    it.Next();
    assert(it.Valid());
    assert(it.Key() == "b");
    assert(it.Value().is_tombstone); // b is deleted
    
    it.Next();
    assert(it.Valid());
    assert(it.Key() == "c");
    assert(!it.Value().is_tombstone);
    assert(it.Value().data[0] == 3);
    
    it.Next();
    assert(!it.Valid());

    std::cout << "  TestIteration: PASSED" << std::endl;
}

void TestConcurrency() {
    MemTable mem;
    
    auto writer = [&]() {
        for (int i = 0; i < 1000; i++) {
            mem.Put("key_" + std::to_string(i), {static_cast<uint8_t>(i % 256)});
        }
    };
    
    auto reader = [&]() {
        for (int i = 0; i < 1000; i++) {
            mem.Get("key_" + std::to_string(i));
        }
    };

    std::vector<std::thread> threads;
    // 1 writer
    threads.emplace_back(writer);
    // 4 readers
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(reader);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    assert(mem.Get("key_999").has_value());
    std::cout << "  TestConcurrency: PASSED" << std::endl;
}

int main() {
    std::cout << "Testing MemTable..." << std::endl;
    TestBasicOperations();
    TestIteration();
    TestConcurrency();
    std::cout << "All MemTable tests passed." << std::endl;
    return 0;
}
