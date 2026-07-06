cmake_minimum_required(VERSION 3.16)
project(distributed-vectordb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Useful compile options for a robust C++ codebase
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Add the root directory to the include path
# This allows includes like #include "src/common/error/status.h"
include_directories(${CMAKE_SOURCE_DIR})

# Add subdirectories
add_subdirectory(src/common)
add_subdirectory(src/storage)
add_subdirectory(src/raft)
add_subdirectory(tests)
