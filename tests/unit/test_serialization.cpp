#include "src/common/serialization/binary_serializer.h"
#include <iostream>
#include <cmath>

using namespace vectordb::common;

int main() {
    std::cout << "Testing Serialization..." << std::endl;
    
    std::vector<uint8_t> buffer;
    
    // Write arbitrary data using the serializer
    BinarySerializer::WriteUint32(buffer, 42);
    BinarySerializer::WriteUint64(buffer, 123456789012345ULL);
    BinarySerializer::WriteString(buffer, "vectordb_test");
    
    std::vector<float> original_vector = {0.1f, 0.2f, 0.3f, -0.4f};
    BinarySerializer::WriteFloatVector(buffer, original_vector);
    
    size_t offset = 0;
    
    // Read the data back and unwrap the ErrorOr<> safely
    auto v1_or = BinarySerializer::ReadUint32(buffer, offset);
    if (!v1_or.IsOk() || v1_or.value() != 42) { std::cerr << "v1 failed\n"; exit(1); }
    
    auto v2_or = BinarySerializer::ReadUint64(buffer, offset);
    if (!v2_or.IsOk() || v2_or.value() != 123456789012345ULL) { std::cerr << "v2 failed\n"; exit(1); }
    
    auto v3_or = BinarySerializer::ReadString(buffer, offset);
    if (!v3_or.IsOk() || v3_or.value() != "vectordb_test") { std::cerr << "v3 failed\n"; exit(1); }
    
    auto v4_or = BinarySerializer::ReadFloatVector(buffer, offset);
    if (!v4_or.IsOk() || v4_or.value().size() != original_vector.size()) { std::cerr << "v4 size failed\n"; exit(1); }
    
    for(size_t i=0; i<v4_or.value().size(); ++i) {
        if (std::abs(v4_or.value()[i] - original_vector[i]) > 1e-6) { std::cerr << "v4 element failed\n"; exit(1); }
    }
    
    // Deliberate out-of-bounds read to test the error subsystem integration
    auto v5_or = BinarySerializer::ReadUint32(buffer, offset);
    if (v5_or.IsOk() || v5_or.status().code() != StatusCode::kStorageCorruption) {
        std::cerr << "OOB read failed to trigger correct Error status\n";
        exit(1);
    }
    
    std::cout << "All Serialization tests passed." << std::endl;
    return 0;
}
