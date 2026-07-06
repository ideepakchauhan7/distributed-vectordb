#include <iostream>
#include <cassert>
#include "src/raft/core/raft_state.h"

using namespace vectordb::raft;
using namespace vectordb::common;

void TestInitialState() {
    std::cout << "Running TestInitialState...\n";
    RaftState state("node_1");
    
    assert(state.role() == RaftRole::kFollower);
    assert(state.current_term() == 0);
    assert(state.voted_for() == "");
    assert(state.leader_id() == "");
    assert(state.self_id() == "node_1");
}

void TestTransitionToCandidate() {
    std::cout << "Running TestTransitionToCandidate...\n";
    RaftState state("node_1");
    
    bool callback_fired = false;
    state.SetStateChangeCallback([&](RaftRole old_role, RaftRole new_role) {
        (void)old_role;
        (void)new_role;
        assert(old_role == RaftRole::kFollower);
        assert(new_role == RaftRole::kCandidate);
        callback_fired = true;
    });

    Status s = state.TransitionToCandidate();
    assert(s.IsOk());
    
    assert(state.role() == RaftRole::kCandidate);
    assert(state.current_term() == 1);
    assert(state.voted_for() == "node_1");
    assert(state.leader_id() == "");
    assert(callback_fired);
}

void TestTransitionToLeader() {
    std::cout << "Running TestTransitionToLeader...\n";
    RaftState state("node_1");
    
    // Must be candidate first
    state.TransitionToCandidate();
    
    bool callback_fired = false;
    state.SetStateChangeCallback([&](RaftRole old_role, RaftRole new_role) {
        (void)old_role;
        (void)new_role;
        assert(old_role == RaftRole::kCandidate);
        assert(new_role == RaftRole::kLeader);
        callback_fired = true;
    });

    Status s = state.TransitionToLeader();
    assert(s.IsOk());
    
    assert(state.role() == RaftRole::kLeader);
    assert(state.leader_id() == "node_1");
    assert(callback_fired);
}

void TestIllegalTransitions() {
    std::cout << "Running TestIllegalTransitions...\n";
    RaftState state("node_1");
    
    // Follower -> Leader is illegal
    Status s = state.TransitionToLeader();
    assert(!s.IsOk());
    
    // Step down to lower term is illegal
    state.TransitionToCandidate(); // Term becomes 1
    s = state.StepDown(0);
    assert(!s.IsOk());

    // Leader -> Candidate is illegal directly
    state.TransitionToLeader();
    s = state.TransitionToCandidate();
    assert(!s.IsOk());
}

void TestStepDown() {
    std::cout << "Running TestStepDown...\n";
    RaftState state("node_1");
    
    state.TransitionToCandidate(); // Term 1
    state.TransitionToLeader();
    
    // Discover higher term leader
    Status s = state.StepDown(2, "node_2");
    assert(s.IsOk());
    
    assert(state.role() == RaftRole::kFollower);
    assert(state.current_term() == 2);
    assert(state.voted_for() == "");
    assert(state.leader_id() == "node_2");
}

void TestVoting() {
    std::cout << "Running TestVoting...\n";
    RaftState state("node_1");
    
    Status s = state.GrantVote("node_2");
    assert(s.IsOk());
    assert(state.voted_for() == "node_2");
    
    // Voting for same candidate again should be Ok
    s = state.GrantVote("node_2");
    assert(s.IsOk());

    // Voting for different candidate in same term should fail
    s = state.GrantVote("node_3");
    assert(!s.IsOk());
}

int main() {
    std::cout << "Starting RaftState Tests...\n";
    TestInitialState();
    TestTransitionToCandidate();
    TestTransitionToLeader();
    TestIllegalTransitions();
    TestStepDown();
    TestVoting();
    std::cout << "All RaftState Tests Passed!\n";
    return 0;
}
