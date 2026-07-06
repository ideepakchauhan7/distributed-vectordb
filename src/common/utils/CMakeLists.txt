add_library(common_utils
    hash.cpp
    string_utils.cpp
)

target_include_directories(common_utils PUBLIC
    ${CMAKE_SOURCE_DIR}
)
