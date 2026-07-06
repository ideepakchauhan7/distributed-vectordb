add_library(storage_engine STATIC
    storage_engine.cpp
)

target_link_libraries(storage_engine PUBLIC
    storage_wal
    storage_memtable
    storage_sstable
    storage_compaction
    storage_recovery
    storage_cache
    storage_checkpoint
    common_error
)

target_include_directories(storage_engine PUBLIC ${CMAKE_SOURCE_DIR})
