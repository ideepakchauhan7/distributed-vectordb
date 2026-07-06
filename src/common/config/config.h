#pragma once

#include <string>
#include <vector>

namespace vectordb {
namespace common {

/**
 * @brief Defines the role of this node within the cluster.
 */
enum class NodeRole {
    kRouter,    // Receives requests, predicts shard latency, routes queries
    kShard,     // Holds vector data, runs RAFT + HNSW index
    kMetadata,  // Manages cluster topology and shard mappings
    kStandalone // Runs all roles in a single process (for testing)
};

/**
 * @brief Configuration for the node's identity and network presence.
 */
struct NodeConfig {
    std::string node_id;         // e.g., "node-1"
    std::string listen_address;  // e.g., "0.0.0.0:50051"
    NodeRole role;
};

/**
 * @brief Configuration for the local storage engine.
 */
struct StorageConfig {
    std::string data_dir;        // Path to store WAL and SSTables
    size_t memtable_size_mb;     // Max size before flushing to disk
    size_t block_cache_mb;       // LRU cache size for reads
};

/**
 * @brief Configuration for the RAFT consensus module.
 */
struct RaftConfig {
    int election_timeout_ms;     // Must be randomized per node (e.g., 150-300ms)
    int heartbeat_interval_ms;   // Must be significantly less than election timeout (e.g., 50ms)
    int snapshot_threshold;      // Number of log entries before triggering a snapshot
};

/**
 * @brief Configuration for cluster topology (finding other nodes).
 */
struct ClusterConfig {
    std::vector<std::string> peer_addresses; // Addresses of seed nodes or other raft peers
};

/**
 * @brief The root configuration object that holds all subsystem configs.
 */
struct ServerConfig {
    NodeConfig node;
    StorageConfig storage;
    RaftConfig raft;
    ClusterConfig cluster;
};

} // namespace common
} // namespace vectordb
