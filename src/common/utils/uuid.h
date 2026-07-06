#pragma once

#include <string>
#include <cstdint>
#include <array>
#include <random>
#include <sstream>
#include <iomanip>

namespace vectordb {
namespace common {
namespace utils {

/**
 * @class UUID
 * @brief A simple, self-contained UUID v4 generator.
 *
 * UUID v4 is randomly generated. Format:
 *   xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 * where 'x' is a random hex digit and 'y' is one of 8, 9, a, or b.
 *
 * Used for:
 *   - Node IDs (unique identifier per cluster node)
 *   - Request IDs (for distributed tracing)
 *   - Vector IDs (when the user doesn't supply an ID)
 */
class UUID {
public:
    /**
     * @brief Generates a new random UUID v4 string.
     * @return A UUID string in canonical format: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
     */
    static std::string Generate() {
        // Use thread_local to avoid re-seeding on every call
        thread_local std::mt19937 rng{std::random_device{}()};
        thread_local std::uniform_int_distribution<uint32_t> dist(0, 15);
        thread_local std::uniform_int_distribution<uint32_t> dist2(8, 11);

        std::ostringstream ss;
        for (int i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                ss << '-';
            }
            if (i == 12) {
                ss << '4';          // Version 4
            } else if (i == 16) {
                ss << std::hex << dist2(rng);  // Variant bits: 8, 9, a, or b
            } else {
                ss << std::hex << dist(rng);
            }
        }
        return ss.str();
    }

    /**
     * @brief Generates a short 8-char hex ID (for node names, shard IDs, etc.)
     * Not a full UUID — just a human-readable short identifier.
     */
    static std::string ShortID() {
        thread_local std::mt19937 rng{std::random_device{}()};
        thread_local std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
        
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << dist(rng);
        return ss.str();
    }
};

} // namespace utils
} // namespace common
} // namespace vectordb
