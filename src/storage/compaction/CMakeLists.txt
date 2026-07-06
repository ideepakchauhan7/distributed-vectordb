add_library(storage_compaction STATIC
    compaction_manager.cpp
)

target_include_directories(storage_compaction PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(storage_compaction PUBLIC
    common_error
    storage_sstable
    storage_memtable
)
