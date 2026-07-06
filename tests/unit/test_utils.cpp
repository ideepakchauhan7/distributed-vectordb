#include "src/common/utils/hash.h"
#include "src/common/utils/string_utils.h"
#include <iostream>

using namespace vectordb::common;

int main() {
    std::cout << "Testing Utils..." << std::endl;
    
    // Test FNV-1a Hash
    uint64_t h1 = Hash::FNV1a64("collection_alpha");
    uint64_t h2 = Hash::FNV1a64("collection_beta");
    uint64_t h3 = Hash::FNV1a64("collection_alpha"); // Identical input
    
    if (h1 == h2) { std::cerr << "Hash collision detected\n"; exit(1); }
    if (h1 != h3) { std::cerr << "Hash non-deterministic behavior\n"; exit(1); }
    
    // Test String Split
    auto split = StringUtils::Split("192.168.1.100:8080", ':');
    if (split.size() != 2 || split[0] != "192.168.1.100" || split[1] != "8080") {
        std::cerr << "String split failed\n"; 
        exit(1); 
    }
    
    // Test String Trim
    auto trimmed = StringUtils::Trim("   raft_leader  \t ");
    if (trimmed != "raft_leader") { 
        std::cerr << "String trim failed: [" << trimmed << "]\n"; 
        exit(1); 
    }
    
    std::cout << "All Utils tests passed." << std::endl;
    return 0;
}
