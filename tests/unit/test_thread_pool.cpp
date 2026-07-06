#include "src/common/thread_pool/thread_pool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib> // for abort()

using namespace vectordb::common;

int main() {
    std::cout << "Testing ThreadPool..." << std::endl;
    
    // Create a pool with 4 background worker threads
    ThreadPool pool(4);
    
    std::vector<std::future<int>> results;
    
    // Enqueue 8 arbitrary jobs
    for(int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.Enqueue([i] {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return i * i;
            })
        );
    }
    
    // Wait for and verify results
    for(int i = 0; i < 8; ++i) {
        int res = results[i].get();
        if (res != i * i) {
            std::cerr << "Mismatch at index " << i << ": expected " << (i*i) << ", got " << res << "\n";
            std::abort();
        }
    }
    
    std::cout << "All ThreadPool tests passed." << std::endl;
    return 0;
}
