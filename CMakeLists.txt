cmake_minimum_required(VERSION 3.16)
project(distributed-vectordb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Useful compile options for a robust C++ codebase
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Add the root directory to the include path
# This allows includes like #include "src/common/error/status.h"
include_directories(${CMAKE_SOURCE_DIR})

# Add subdirectories
add_subdirectory(src/common)

# A quick test executable to verify compilation
add_executable(test_error test_error.cpp)
target_link_libraries(test_error PRIVATE common_error)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE common_config)

add_executable(test_logging test_logging.cpp)
target_link_libraries(test_logging PRIVATE common_logging)

add_executable(test_metrics test_metrics.cpp)
target_link_libraries(test_metrics PRIVATE common_metrics)

add_executable(test_clock test_clock.cpp)
target_link_libraries(test_clock PRIVATE common_clock)

add_executable(test_thread_pool test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE common_thread_pool)

add_executable(test_serialization test_serialization.cpp)
target_link_libraries(test_serialization PRIVATE common_serialization)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils PRIVATE common_utils)

add_subdirectory(src/storage)

add_executable(test_wal test_wal.cpp)
target_link_libraries(test_wal PRIVATE storage_wal)

add_executable(test_memtable test_memtable.cpp)
target_link_libraries(test_memtable PRIVATE storage_memtable)

add_executable(test_sstable test_sstable.cpp)
target_link_libraries(test_sstable PRIVATE storage_sstable)

add_executable(test_compaction test_compaction.cpp)
target_link_libraries(test_compaction PRIVATE storage_compaction)

add_executable(test_recovery test_recovery.cpp)
target_link_libraries(test_recovery PRIVATE storage_recovery storage_memtable storage_wal common_error)
add_test(NAME RecoveryTest COMMAND test_recovery)

add_executable(test_cache test_cache.cpp)
target_link_libraries(test_cache PRIVATE storage_cache)
add_test(NAME CacheTest COMMAND test_cache)

add_executable(test_checkpoint test_checkpoint.cpp)
target_link_libraries(test_checkpoint PRIVATE storage_checkpoint)
add_test(NAME CheckpointTest COMMAND test_checkpoint)

add_executable(test_storage_engine test_storage_engine.cpp)
target_link_libraries(test_storage_engine PRIVATE storage_engine)
add_test(NAME StorageEngineTest COMMAND test_storage_engine)

add_subdirectory(src/raft)

add_executable(test_raft_state test_raft_state.cpp)
target_link_libraries(test_raft_state PRIVATE raft_core)
add_test(NAME RaftStateTest COMMAND test_raft_state)
