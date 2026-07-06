#include "src/common/config/config_loader.h"
#include <iostream>

using namespace vectordb::common;

int main() {
    // Attempt to load the YAML file we just created
    auto config_or = ConfigLoader::LoadFromFile("src/common/config/config.yaml");

    if (!config_or.IsOk()) {
        std::cerr << "Failed to load config: " << config_or.status().ToString() << std::endl;
        return 1;
    }

    ServerConfig config = config_or.value();

    std::cout << "Successfully loaded configuration!" << std::endl;
    std::cout << "Node ID: " << config.node.node_id << std::endl;
    std::cout << "Listen Address: " << config.node.listen_address << std::endl;
    std::cout << "Data Directory: " << config.storage.data_dir << std::endl;
    std::cout << "Raft Election Timeout: " << config.raft.election_timeout_ms << "ms" << std::endl;
    std::cout << "Peer count: " << config.cluster.peer_addresses.size() << std::endl;

    return 0;
}
