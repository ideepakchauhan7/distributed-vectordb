add_library(common_clock
    physical_clock.cpp
    fake_clock.cpp
    hlc.cpp
)

target_include_directories(common_clock PUBLIC
    ${CMAKE_SOURCE_DIR}
)
