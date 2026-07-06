#include "src/common/logging/logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace vectordb::common;

int main() {
    // Initialize the logger for "router-node-1", logging to a local test file
    Logger::Initialize("router-node-1", "vectordb_test.log");

    std::cout << "Writing logs..." << std::endl;

    LOG_INFO("startup", "VectorDB node starting up...");
    LOG_INFO("config", "Configuration loaded successfully");
    LOG_WARN("raft", "Election timeout triggered. Attempting to become leader.");
    LOG_ERROR("storage", "Failed to flush memtable to disk. Retrying.");

    // Sleep briefly to ensure async logs are processed by the worker thread
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Shutdown flushes the queue
    Logger::Shutdown();

    std::cout << "Logs written to vectordb_test.log and console." << std::endl;

    return 0;
}
