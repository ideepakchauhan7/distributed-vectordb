add_library(common_serialization
    binary_serializer.cpp
)

target_include_directories(common_serialization PUBLIC
    ${CMAKE_SOURCE_DIR}
)

# Link against our error library for ErrorOr macro definitions
target_link_libraries(common_serialization PUBLIC
    common_error
)
