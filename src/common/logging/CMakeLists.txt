include(FetchContent)

# Automatically download and build spdlog during CMake configuration
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0
)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

add_library(common_logging
    logger.cpp
)

target_include_directories(common_logging PUBLIC
    ${CMAKE_SOURCE_DIR}
)

# Link against spdlog
target_link_libraries(common_logging PUBLIC
    spdlog::spdlog
)
