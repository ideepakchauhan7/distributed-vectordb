#pragma once

#include <string>
#include <mutex>
#include <functional>
#include "src/common/error/error_or.h"

namespace vectordb {
namespace raft {

/**
 * @enum RaftRole
 * @brief Represents the three possible states of a RAFT node.
 */
enum class RaftRole {
    kFollower,
    kCandidate,
    kLeader
};

std::string RaftRoleToString(RaftRole role);

/**
 * @class RaftState
 * @brief Thread-safe Finite State Machine (FSM) for a RAFT node.
 * 
 * Manages the node's current role, term, vote record, and known leader.
 * Enforces legal state transitions and triggers callbacks on role changes.
 */
class RaftState {
public:
    using StateChangeCallback = std::function<void(RaftRole old_role, RaftRole new_role)>;

    /**
     * @brief Constructs the initial state (always Follower).
     * @param self_id The UUID of this node.
     */
    explicit RaftState(std::string self_id);

    // Disable copy/move
    RaftState(const RaftState&) = delete;
    RaftState& operator=(const RaftState&) = delete;

    /**
     * @brief Register a callback to be notified when the role changes.
     */
    void SetStateChangeCallback(StateChangeCallback callback);

    // --- State Transitions ---

    /**
     * @brief Transitions to Candidate and increments the term.
     * Legal from: Follower (timeout), Candidate (split vote retry).
     * Side effects: Clears leader_id, votes for self (sets voted_for).
     * @return Status::Ok() or InvalidArgument if illegal transition.
     */
    common::Status TransitionToCandidate();

    /**
     * @brief Transitions to Leader.
     * Legal from: Candidate (won election).
     * Side effects: Sets leader_id to self_id.
     * @return Status::Ok() or InvalidArgument if illegal transition.
     */
    common::Status TransitionToLeader();

    /**
     * @brief Transitions to Follower and updates the term.
     * Legal from: Any role (when discovering a higher term or valid leader).
     * Side effects: Clears voted_for if term changes.
     * @param new_term The term to update to. Must be >= current_term.
     * @param known_leader_id The ID of the leader (if known), else empty.
     * @return Status::Ok() or InvalidArgument if illegal transition.
     */
    common::Status StepDown(uint64_t new_term, const std::string& known_leader_id = "");

    // --- State Mutation & Access ---

    /**
     * @brief Records a vote for a candidate in the current term.
     * @param candidate_id The ID of the candidate we are voting for.
     * @return Status::Ok() or error if we already voted for someone else this term.
     */
    common::Status GrantVote(const std::string& candidate_id);

    // Thread-safe accessors
    RaftRole role() const;
    uint64_t current_term() const;
    std::string voted_for() const;
    std::string leader_id() const;
    std::string self_id() const;

    // Direct term/vote setters (e.g., for loading from disk on startup)
    void LoadState(uint64_t term, const std::string& voted_for);

private:
    mutable std::mutex mutex_;

    RaftRole current_role_;
    uint64_t current_term_;
    std::string voted_for_;
    std::string leader_id_;
    std::string self_id_;

    StateChangeCallback on_state_change_;
};

} // namespace raft
} // namespace vectordb
