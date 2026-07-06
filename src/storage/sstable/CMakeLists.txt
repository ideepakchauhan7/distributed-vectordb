add_library(storage_sstable STATIC
    bloom_filter.cpp
    block.cpp
    sstable_writer.cpp
    sstable_reader.cpp
)

target_include_directories(storage_sstable PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(storage_sstable PUBLIC
    common_error
    common_serialization
    storage_memtable
)
