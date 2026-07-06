add_library(storage_checkpoint STATIC
    checkpoint_manager.cpp
)

target_link_libraries(storage_checkpoint PUBLIC
    storage_compaction
    common_serialization
    common_error
)

target_include_directories(storage_checkpoint PUBLIC ${CMAKE_SOURCE_DIR})
