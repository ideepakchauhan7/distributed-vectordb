add_library(storage_cache STATIC
    block_cache.cpp
)

target_include_directories(storage_cache PUBLIC ${CMAKE_SOURCE_DIR})
