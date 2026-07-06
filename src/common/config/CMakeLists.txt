include(FetchContent)

# Automatically download and build yaml-cpp during CMake configuration
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
# Disable yaml-cpp tests and tools to drastically speed up compilation
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

add_library(common_config
    config_loader.cpp
)

target_include_directories(common_config PUBLIC
    ${CMAKE_SOURCE_DIR}
)

# Link against our error handling component and yaml-cpp
target_link_libraries(common_config PUBLIC
    common_error
    yaml-cpp
)
