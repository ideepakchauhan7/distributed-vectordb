add_library(storage_recovery STATIC
    recovery_manager.cpp
)

target_include_directories(storage_recovery PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(storage_recovery PUBLIC
    common_error
    storage_wal
    storage_memtable
)
