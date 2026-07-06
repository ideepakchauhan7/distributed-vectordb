#include "src/raft/core/raft_state.h"

namespace vectordb {
namespace raft {

std::string RaftRoleToString(RaftRole role) {
    switch (role) {
        case RaftRole::kFollower: return "Follower";
        case RaftRole::kCandidate: return "Candidate";
        case RaftRole::kLeader: return "Leader";
        default: return "Unknown";
    }
}

RaftState::RaftState(std::string self_id)
    : current_role_(RaftRole::kFollower),
      current_term_(0),
      voted_for_(""),
      leader_id_(""),
      self_id_(std::move(self_id)) {}

void RaftState::SetStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_state_change_ = std::move(callback);
}

common::Status RaftState::TransitionToCandidate() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_role_ == RaftRole::kLeader) {
        return common::Status(common::StatusCode::kInvalidArgument, 
                              "Leader cannot transition directly to Candidate. Must StepDown first.");
    }

    RaftRole old_role = current_role_;
    
    current_role_ = RaftRole::kCandidate;
    current_term_++;
    voted_for_ = self_id_; // Vote for self
    leader_id_ = "";       // We don't know the leader anymore

    if (on_state_change_) {
        on_state_change_(old_role, current_role_);
    }

    return common::Status::Ok();
}

common::Status RaftState::TransitionToLeader() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_role_ != RaftRole::kCandidate) {
        return common::Status(common::StatusCode::kInvalidArgument, 
                              "Only a Candidate can transition to Leader.");
    }

    RaftRole old_role = current_role_;
    current_role_ = RaftRole::kLeader;
    leader_id_ = self_id_;

    if (on_state_change_) {
        on_state_change_(old_role, current_role_);
    }

    return common::Status::Ok();
}

common::Status RaftState::StepDown(uint64_t new_term, const std::string& known_leader_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (new_term < current_term_) {
        return common::Status(common::StatusCode::kInvalidArgument, 
                              "Cannot step down to a lower term.");
    }

    RaftRole old_role = current_role_;
    
    // If we're moving to a strictly newer term, we haven't voted in it yet.
    if (new_term > current_term_) {
        voted_for_ = "";
    }

    current_role_ = RaftRole::kFollower;
    current_term_ = new_term;
    leader_id_ = known_leader_id;

    if (old_role != current_role_ && on_state_change_) {
        on_state_change_(old_role, current_role_);
    }

    return common::Status::Ok();
}

common::Status RaftState::GrantVote(const std::string& candidate_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (voted_for_.empty() || voted_for_ == candidate_id) {
        voted_for_ = candidate_id;
        return common::Status::Ok();
    }
    
    return common::Status(common::StatusCode::kAlreadyExists, 
                          "Already voted for another candidate in this term.");
}

RaftRole RaftState::role() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_role_;
}

uint64_t RaftState::current_term() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_term_;
}

std::string RaftState::voted_for() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return voted_for_;
}

std::string RaftState::leader_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return leader_id_;
}

std::string RaftState::self_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return self_id_;
}

void RaftState::LoadState(uint64_t term, const std::string& voted_for) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_term_ = term;
    voted_for_ = voted_for;
}

} // namespace raft
} // namespace vectordb
