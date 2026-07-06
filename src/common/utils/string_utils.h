#pragma once
#include <string>
#include <vector>

namespace vectordb {
namespace common {

/**
 * @class StringUtils
 * @brief Helper functions for string manipulation.
 */
class StringUtils {
public:
    /**
     * @brief Splits a string by a given delimiter.
     */
    static std::vector<std::string> Split(const std::string& str, char delimiter);
    
    /**
     * @brief Removes leading and trailing whitespace from a string.
     */
    static std::string Trim(const std::string& str);
};

} // namespace common
} // namespace vectordb
