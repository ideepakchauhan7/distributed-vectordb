include(FetchContent)

# Automatically download and build prometheus-cpp
FetchContent_Declare(
  prometheus-cpp
  GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
  GIT_TAG v1.3.0
)
# Disable unnecessary components to dramatically reduce build time
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
set(USE_THIRDPARTY_LIBRARIES ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(prometheus-cpp)

add_library(common_metrics
    metrics_registry.cpp
)

target_include_directories(common_metrics PUBLIC
    ${CMAKE_SOURCE_DIR}
)

# Link against prometheus-cpp core and pull server
target_link_libraries(common_metrics PUBLIC
    prometheus-cpp::core
    prometheus-cpp::pull
)
