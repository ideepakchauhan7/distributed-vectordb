add_library(storage_wal
    wal.cpp
)

target_include_directories(storage_wal PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(storage_wal PUBLIC
    common_error
    common_serialization
)
