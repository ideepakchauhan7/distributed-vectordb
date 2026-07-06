add_library(storage_memtable STATIC
    skip_list.cpp
    memtable.cpp
)

target_include_directories(storage_memtable PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(storage_memtable PUBLIC
    common_error
)
