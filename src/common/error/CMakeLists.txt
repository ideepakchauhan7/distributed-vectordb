add_library(common_error
    status.cpp
)

target_include_directories(common_error PUBLIC
    ${CMAKE_SOURCE_DIR}
)
