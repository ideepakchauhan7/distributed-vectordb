#pragma once

#include <string>
#include "src/common/config/config.h"
#include "src/common/error/error_or.h"

namespace vectordb {
namespace common {

/**
 * @class ConfigLoader
 * @brief Responsible for parsing YAML files into strongly-typed ServerConfig structs.
 */
class ConfigLoader {
public:
    /**
     * @brief Loads and validates configuration from a YAML file.
     * 
     * @param file_path The absolute or relative path to the YAML file.
     * @return ErrorOr<ServerConfig> The parsed configuration, or a Status error if parsing fails.
     */
    static ErrorOr<ServerConfig> LoadFromFile(const std::string& file_path);
};

} // namespace common
} // namespace vectordb
