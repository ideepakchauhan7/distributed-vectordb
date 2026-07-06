add_library(raft_core STATIC raft_state.cpp)
target_link_libraries(raft_core PUBLIC common_error common_clock)
