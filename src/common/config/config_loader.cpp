#include "src/common/config/config_loader.h"
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace vectordb {
namespace common {

namespace {
NodeRole ParseRole(const std::string& role_str) {
    if (role_str == "router") return NodeRole::kRouter;
    if (role_str == "shard") return NodeRole::kShard;
    if (role_str == "metadata") return NodeRole::kMetadata;
    return NodeRole::kStandalone;
}
} // namespace

ErrorOr<ServerConfig> ConfigLoader::LoadFromFile(const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return Status(StatusCode::kNotFound, "Config file not found: " + file_path);
    }

    try {
        YAML::Node yaml = YAML::LoadFile(file_path);
        ServerConfig config;

        // Parse Node Config
        if (yaml["node"]) {
            config.node.node_id = yaml["node"]["node_id"].as<std::string>("default-node");
            config.node.listen_address = yaml["node"]["listen_address"].as<std::string>("0.0.0.0:50051");
            config.node.role = ParseRole(yaml["node"]["role"].as<std::string>("standalone"));
        } else {
            return Status(StatusCode::kInvalidArgument, "Missing required 'node' section in config");
        }

        // Parse Storage Config (with defaults)
        if (yaml["storage"]) {
            config.storage.data_dir = yaml["storage"]["data_dir"].as<std::string>("/tmp/vectordb");
            config.storage.memtable_size_mb = yaml["storage"]["memtable_size_mb"].as<size_t>(64);
            config.storage.block_cache_mb = yaml["storage"]["block_cache_mb"].as<size_t>(256);
        }

        // Parse Raft Config (with defaults)
        if (yaml["raft"]) {
            config.raft.election_timeout_ms = yaml["raft"]["election_timeout_ms"].as<int>(300);
            config.raft.heartbeat_interval_ms = yaml["raft"]["heartbeat_interval_ms"].as<int>(50);
            config.raft.snapshot_threshold = yaml["raft"]["snapshot_threshold"].as<int>(10000);
        }

        // Parse Cluster Config
        if (yaml["cluster"] && yaml["cluster"]["peers"]) {
            for (const auto& peer : yaml["cluster"]["peers"]) {
                config.cluster.peer_addresses.push_back(peer.as<std::string>());
            }
        }

        return config;

    } catch (const YAML::Exception& e) {
        // We catch YAML parser exceptions and convert them to our exception-free Status!
        return Status(StatusCode::kInvalidArgument, std::string("Failed to parse YAML config: ") + e.what());
    }
}

} // namespace common
} // namespace vectordb
