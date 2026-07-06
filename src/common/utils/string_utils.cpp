#include "src/common/utils/string_utils.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace vectordb {
namespace common {

std::vector<std::string> StringUtils::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string StringUtils::Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

} // namespace common
} // namespace vectordb
