add_library(common_thread_pool
    thread_pool.cpp
)

target_include_directories(common_thread_pool PUBLIC
    ${CMAKE_SOURCE_DIR}
)

# Requires pthread on Linux
find_package(Threads REQUIRED)
target_link_libraries(common_thread_pool PUBLIC Threads::Threads)
